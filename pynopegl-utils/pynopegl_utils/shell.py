#
# Copyright 2023 Clément Bœsch <u pkh.me>
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

import cmd
import os.path as op
from pathlib import Path
import subprocess
import sys
from textwrap import indent, dedent

from pynodegl_utils.com import query_scene
from pynodegl_utils.config import Config
from pynodegl_utils.hooks import HooksCaller, HooksController
from pynodegl_utils.scriptsmgr import ScriptsManager

from dataclasses import dataclass


# XXX replace config timer with native one to avoid the qthread warning bs

# XXX share with the controller?
@dataclass
class SceneInfo:
    #uid: str
    doc: str
    # XXX widgets

class Shell(cmd.Cmd):

    prompt = "ngl> "

    def __init__(self, module_pkgname: str, hooks_scripts: Path):
        super().__init__()

        self._module_pkgname = module_pkgname
        self._scripts_mgr = ScriptsManager(module_pkgname)
        self._hooks_caller = HooksCaller(hooks_scripts)
        #self._hooks_ctl = HooksController(self._get_scene, self._hooks_caller)

        #get_scene_func = self._get_scene

        self._config = Config(module_pkgname)

        self._scenes = []
        self._current_scene = None

        #self._scripts_mgr.error.connect(self._all_scripts_err)
        self._scripts_mgr.scriptsChanged.connect(self._on_scripts_changed)
        self._scripts_mgr.start()

        # XXX needed?
        prev_pkgname = self._config.get("pkg")
        prev_module = self._config.get("module")
        prev_scene = self._config.get("scene")
        if prev_pkgname == module_pkgname:
            self._load_scene_from_name(prev_module, prev_scene)

    def _on_scripts_changed(self, scenes):
        #print(scenes)
        self._scenes = scenes

        scenes_map = {}

        for module_name, sub_scenes in self._scenes:
            #qitem_script = QtGui.QStandardItem(module_name)
            for scene_name, scene_doc, _ in sub_scenes:
                scene_id = f"{module_name}.{scene_name}"
                scenes_map[scene_id] = scene_doc
                #if arg == "full" and scene_doc:
                #    print(indent(dedent(scene_doc).strip(), "    "))

    #def _get_scene(self):

    #    cfg = self._scene_toolbar.get_cfg()
    #    if cfg["scene"] is None:
    #        return None
    #    medias = self._medias_view.get_medias()
    #    if medias:
    #        # Replace the default medias with user medias only if the user has
    #        # specified some
    #        cfg["medias"] = medias
    #    cfg["files"] = []
    #    cfg.update(cfg_overrides)

    #    self._scripts_mgr.inc_query_count()
    #    self._scripts_mgr.pause()
    #    ret = query_scene(self._module_pkgname, **cfg)
    #    self._scripts_mgr.update_filelist(ret["filelist"])
    #    self._scripts_mgr.update_modulelist(ret["modulelist"])
    #    self._scripts_mgr.resume()
    #    self._scripts_mgr.dec_query_count()

    #    if "error" in ret:
    #        self.error.emit(ret["error"])
    #        return None

    #    self.error.emit(None)
    #    self.sceneLoaded.emit(ret)

    #    return ret

    #def _get_cfg(self):
    #    choices = Config.CHOICES
    #    return {
    #        "scene": self._current_scene_data[:2] if self._current_scene_data else None,
    #        "aspect_ratio": choices["aspect_ratio"][self._ar_cbbox.currentIndex()],
    #        "framerate": choices["framerate"][self._fr_cbbox.currentIndex()],
    #        "samples": choices["samples"][self._samples_cbbox.currentIndex()],
    #        "extra_args": self._scene_extra_args,
    #        "clear_color": self._clear_color,
    #        "backend": choices["backend"][self._backend_cbbox.currentIndex()],
    #    }

    def _load_scene_from_name(self, module, scene):
        # TODO
        pass

    def do_media(self):
        # TODO
        # XXX subgroup?
        # add
        # rm
        # ...
        pass

    def do_spawn(self, arg):
        """Spawn a new ngl-desktop instance"""
        # TODO options
        subprocess.Popen(["ngl-desktop"])

    def do_list_scene_params(self, arg):
        # XXX maybe simply as pick arguments?
        pass

    def do_change_params(self, arg):
        pass

    def do_list(self, arg):
        """List all available scenes"""
        for module_name, sub_scenes in self._scenes:
            #qitem_script = QtGui.QStandardItem(module_name)
            for scene_name, scene_doc, _ in sub_scenes:
                scene_id = f"{module_name}.{scene_name}"
                marker = "*" if self._current_scene == scene_id else " "
                print(f"{marker} {scene_id}")
                if arg == "full" and scene_doc:
                    print(indent(dedent(scene_doc).strip(), "    "))

    def do_rlist(self, arg):
        """List all running instances"""
        # TODO

    def do_refresh(self, arg):
        """Refresh list of instances"""
        # TODO

    def do_pick(self, arg):
        """Pick a scene"""
        # TODO

    def complete_pick(self, arg):
        # TODO: return the list of available scenes
        pass


def run():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-m",
        dest="module",
        default="pynodegl_utils.examples",
        help="set the module name containing the scene functions",
    )
    parser.add_argument(
        "--hooks-script",
        dest="hooks_scripts",
        default=[op.join(op.dirname(__file__), "hooks", "desktop.py")],
        action="append",
        type=Path,
        help="set the script containing event hooks",
    )
    pargs = parser.parse_args(sys.argv[1:])
    Shell(pargs.module, pargs.hooks_scripts).cmdloop()
