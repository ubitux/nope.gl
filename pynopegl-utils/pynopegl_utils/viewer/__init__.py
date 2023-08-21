#
# Copyright 2023 Clément Bœsch <u pkh.me>
# Copyright 2023 Nope Foundry
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

import importlib
import os
import os.path as op
import subprocess
import sys
from dataclasses import dataclass
from inspect import getmembers
from typing import Any, Dict, List, Optional, Tuple

from pynopegl_utils import qml
from pynopegl_utils.com import query_scene
from pynopegl_utils.misc import SceneCfg, SceneInfo, get_viewport
from pynopegl_utils.module import load_script
from pynopegl_utils.scriptsmgr import ScriptsManager
from PySide6.QtCore import QAbstractListModel, QModelIndex, QObject, QStringListModel, Qt, QUrl, Slot

import pynopegl as ngl


@dataclass
class _EncodeProfile:
    name: str
    format: str
    args: List[str]
    fps: Tuple[int, int]


@dataclass
class _Resolution:
    name: str
    rows: int


_RES = [
    _Resolution("144p", 144),
    _Resolution("240p", 240),
    _Resolution("360p", 360),
    _Resolution("480p", 480),
    _Resolution("720p", 720),
    _Resolution("1080p", 1080),
    _Resolution("1440p", 1440),
    _Resolution("4k", 2048),
]
_DEFAULT_RES = 5  # 1080p

_PROFILES = [
    _EncodeProfile(
        name="MP4 / H264 4:2:0",
        format="mp4",
        args=["-pix_fmt", "yuv420p"],
        fps=(60, 1),
    ),
    _EncodeProfile(
        name="MP4 / H264 4:4:4",
        format="mp4",
        args=["-pix_fmt", "yuv444p"],
        fps=(60, 1),
    ),
    _EncodeProfile(
        name="MOV / QTRLE (Lossless)",
        format="mov",
        args=["-c:v", "qtrle"],
        fps=(60, 1),
    ),
    _EncodeProfile(
        name="NUT / FFV1 (Lossless)",
        format="nut",
        args=["-c:v", "ffv1"],
        fps=(60, 1),
    ),
]
_DEFAULT_PROFILE = 0


class _Viewer:
    def __init__(self, app, qml_engine, ngl_widget, args):
        self._app = app
        self._qml_engine = qml_engine
        app_window = qml_engine.rootObjects()[0]

        # Needed so that all the objects found with fincChild() remains accessible
        # https://bugreports.qt.io/browse/QTBUG-116311
        self._window = app_window

        self._cancel_export_request = False
        app_window.exportVideo.connect(self._export_video)
        app_window.cancelExport.connect(self._cancel_export)

        self._livectls_model = UIElementsModel()
        self._list_view = app_window.findChild(QObject, "controlList")
        self._list_view.setProperty("model", self._livectls_model)

        self._ngl_widget = ngl_widget
        self._ngl_widget.livectls_changed.connect(self._livectls_model.reset_data_model)
        self._livectls_model.dataChanged.connect(self._livectl_data_changed)

        self._scene_list = app_window.findChild(QObject, "sceneList")
        self._scene_list.activated.connect(self._select_scene)

        self._script = app_window.findChild(QObject, "script")
        self._script.editingFinished.connect(self._update_module)

        self._player = app_window.findChild(QObject, "player")
        self._player.timeChanged.connect(ngl_widget.set_time)

        self._export_bar = app_window.findChild(QObject, "exportBar")

        self._player = app_window.findChild(QObject, "player")
        self._scene_index = -1

        res_names = [p.name for p in _RES]
        res_list = app_window.findChild(QObject, "resList")
        res_list.setProperty("model", res_names)
        res_list.setProperty("currentIndex", _DEFAULT_RES)

        profile_names = [p.name for p in _PROFILES]
        profile_list = app_window.findChild(QObject, "profileList")
        profile_list.setProperty("model", profile_names)
        profile_list.setProperty("currentIndex", _DEFAULT_PROFILE)

        self._scene_data = []

        try:
            ret = subprocess.run(["ffmpeg", "-version"], check=True)
        except Exception:
            app_window.disable_export("No working `ffmpeg` command found,\nexport is disabled.")

        if len(args) > 1:
            mod_name = args[1]
        else:
            mod_name = "pynopegl_utils.examples.misc"
        func_name = args[2] if len(args) > 2 else None
        self._script.setProperty("text", mod_name)

        self._scripts_mgr = None
        self._load_module(mod_name, func_name)

    @Slot()
    def _cancel_export(self):
        self._cancel_export_request = True

    @Slot(str, int, int)
    def _export_video(self, filename: str, res_index: int, profile_index: int):
        assert self._scene_index != -1

        res = _RES[res_index].rows
        profile = _PROFILES[profile_index]

        scene_data = self._get_scene_data(self._scene_index)
        assert scene_data is not None

        try:
            scene_info: SceneInfo = scene_data["func"]()
            scene = scene_info.scene

            # Apply user live changes
            model_data = qml.ngl_widget.livectl_get_model_data(scene)
            changes = []
            for entry in model_data:
                value = scene_data["live"].get(entry["label"])
                if value is None:
                    continue
                entry["val"] = value
                changes.append(entry)
            qml.ngl_widget.livectl_apply_changes(changes)

            cfg = SceneCfg(framerate=profile.fps)

            ar = scene.aspect_ratio
            height = res
            width = int(height * ar[0] / ar[1])

            # make sure it's a multiple of 2 for the h264 codec
            width &= ~1

            fps = scene.framerate
            duration = scene.duration

            fd_r, fd_w = os.pipe()

            # fmt: off
            cmd = [
                "ffmpeg", "-r", "%d/%d" % fps,
                "-v", "warning",
                "-nostats", "-nostdin",
                "-f", "rawvideo",
                "-video_size", "%dx%d" % (width, height),
                "-pixel_format", "rgba",
                "-i", "pipe:%d" % fd_r,
            ] + profile.args + [
                "-f", profile.format,
                "-y", filename,
            ]
            # fmt: on

            reader = subprocess.Popen(cmd, pass_fds=(fd_r,))
            os.close(fd_r)

            capture_buffer = bytearray(width * height * 4)

            ctx = ngl.Context()
            ctx.configure(
                ngl.Config(
                    platform=ngl.Platform.AUTO,
                    backend=ngl.Backend.AUTO,
                    offscreen=True,
                    width=width,
                    height=height,
                    viewport=get_viewport(width, height, scene.aspect_ratio),
                    samples=cfg.samples,
                    capture_buffer=capture_buffer,
                )
            )
            ctx.set_scene(scene)

            # Draw every frame
            nb_frame = int(duration * fps[0] / fps[1])
            for i in range(nb_frame):
                if self._cancel_export_request:
                    break
                time = i * fps[1] / float(fps[0])
                ctx.draw(time)
                os.write(fd_w, capture_buffer)
                progress = i / (nb_frame - 1)
                self._export_bar.setProperty("value", progress)
                self._app.processEvents()  # refresh UI

            if self._cancel_export_request:
                self._cancel_export_request = False
            else:
                self._window.done_export()

            os.close(fd_w)
            reader.wait()

            del ctx

            self._ngl_widget.set_scene(scene)

        except Exception as e:
            self._window.error_export(str(e))

    @Slot()
    def _update_module(self):
        mod_name = self._script.property("text")
        mod_name = QUrl(mod_name).path()  # handle the file:// automatically added
        self._load_module(mod_name)

    @Slot(str)
    def _scriptmgr_error(self, err_str):
        # TODO add to UI
        if not err_str:
            return
        print(f"ERROR: {err_str}")

    @Slot(list)
    def _scriptmgr_scripts_changed(self, scenes):
        cur_scene_id = None
        scene_data = self._get_scene_data(self._scene_index)
        if scene_data:
            cur_scene_id = scene_data["scene_id"]

        scene_data = []
        for module_name, sub_scenes in scenes:
            base = module_name.rsplit(".", 1)[-1]
            prefix = base + "." if len(scenes) > 1 else ""
            for scene_name, func in sub_scenes:
                # XXX split out the live data?
                scene_data.append(
                    dict(
                        scene_id=prefix + scene_name,
                        module_name=module_name,
                        func=func,
                        live={},
                    )
                )

        self._scene_data = scene_data

        scene_ids = [data["scene_id"] for data in scene_data]
        self._scene_list.setProperty("model", scene_ids)

        index = scene_ids.index(cur_scene_id) if cur_scene_id and cur_scene_id in scene_ids else 0
        self._scene_list.setProperty("currentIndex", index)
        self._select_scene(index)

    def _load_module(self, mod_name: str, func_name: Optional[str] = None):
        if self._scripts_mgr is not None:
            self._scripts_mgr.pause()
            del self._scripts_mgr

        self._mod_name = mod_name
        self._func_name = func_name

        self._scripts_mgr = ScriptsManager(mod_name)
        self._scripts_mgr.error.connect(self._scriptmgr_error)
        self._scripts_mgr.scriptsChanged.connect(self._scriptmgr_scripts_changed)
        self._scripts_mgr.start()

    def _get_scene_data(self, index) -> Optional[Dict[str, Any]]:
        if index < 0 or index >= len(self._scene_data):
            return None
        return self._scene_data[index]

    @Slot()
    def _select_scene(self, index):
        self._scene_index = index

        scene_data = self._get_scene_data(index)
        print(f"select scene {index} -> {scene_data=}")
        if not scene_data:
            self._ngl_widget.stop()
            return

        cfg = SceneCfg()

        assert self._scripts_mgr is not None  # module is always loaded before we select a scene
        self._scripts_mgr.inc_query_count()
        self._scripts_mgr.pause()
        query_info = query_scene(self._mod_name, scene_data["func"], cfg)
        self._scripts_mgr.update_filelist(query_info.filelist)
        self._scripts_mgr.update_modulelist(query_info.modulelist)
        self._scripts_mgr.resume()
        self._scripts_mgr.dec_query_count()

        if query_info.error is not None:
            print("ERROR", query_info.error)
            # self.error.emit(ret["error"])
            return None

        # self.error.emit(None)
        # self.sceneLoaded.emit(ret)
        scene_info: SceneInfo = query_info.ret
        scene = scene_info.scene

        # TODO Ideally we would honor the saved live data settings for the
        # scene, but this implies an update of all the widgets data so we reset
        # it instead.
        scene_data["live"] = {}

        self._ngl_widget.set_scene(scene)

        self._player.setProperty("duration", scene.duration)
        self._player.setProperty("framerate", list(scene.framerate))
        self._player.setProperty("aspect", list(scene.aspect_ratio))

    @Slot()
    def _livectl_data_changed(self, top_left, bottom_right):
        scene_data = self._get_scene_data(self._scene_index)
        assert scene_data is not None

        for row in range(top_left.row(), bottom_right.row() + 1):
            data = self._livectls_model.get_row(row)
            self._ngl_widget.livectls_changes[data["label"]] = data

            # We do not store the associated node because we want to keep this
            # data context agnostic
            scene_data["live"][data["label"]] = data["val"]
        self._ngl_widget.update()


class UIElementsModel(QAbstractListModel):
    _roles_map = {Qt.UserRole + i: s for i, s in enumerate(("type", "label", "val", "min", "max"))}

    def __init__(self, parent=None):
        super().__init__(parent)
        self._data = []

    @Slot(object)
    def reset_data_model(self, data):
        self.beginResetModel()
        self._data = data
        self.endResetModel()

    def get_row(self, row):
        return self._data[row]

    def roleNames(self):
        names = super().roleNames()
        names.update({k: v.encode() for k, v in self._roles_map.items()})
        return names

    def rowCount(self, parent=QModelIndex()):
        return len(self._data)

    def data(self, index, role: int):
        if not index.isValid():
            return None
        return self._data[index.row()].get(self._roles_map[role])

    def setData(self, index, value, role):
        if not index.isValid():
            return False
        item = self._data[index.row()]
        item[self._roles_map[role]] = value
        self.dataChanged.emit(index, index)
        return True


def run():
    qml_file = op.join(op.dirname(qml.__file__), "viewer.qml")
    app, engine = qml.create_app_engine(sys.argv, qml_file)
    ngl_widget = qml.create_ngl_widget(engine)
    viewer = _Viewer(app, engine, ngl_widget, sys.argv)
    ret = app.exec()
    del viewer
    sys.exit(ret)
