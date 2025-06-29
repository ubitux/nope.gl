#
# Copyright 2023 Clément Bœsch <u pkh.me>
# Copyright 2023 Nope Forge
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

import json
import os
import os.path as op
import sys
from pathlib import Path
from typing import Tuple

from pynopegl_utils.export import ENCODE_PROFILES, RESOLUTIONS
from PySide6 import QtCore


class Config(QtCore.QObject):
    FILEPATH = (
        Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share")) / "nope.gl" / "viewer.json"
    ).as_posix()

    FRAMERATES = [
        (8, 1),
        (12, 1),
        (15, 1),
        (24000, 1001),
        (24, 1),
        (25, 1),
        (30000, 1001),
        (30, 1),
        (50, 1),
        (60000, 1001),
        (60, 1),
    ]
    EXPORT_RESOLUTIONS = list(RESOLUTIONS.keys())
    EXPORT_PROFILES = list(ENCODE_PROFILES.keys())
    EXPORT_SAMPLES = [0, 2, 4, 8]

    def __init__(self):
        super().__init__()

        self._init_fields()
        with open(self.FILEPATH) as f:
            self._load_config(json.load(f))

        self._needs_saving = not op.exists(self.FILEPATH)

        self._config_timer = QtCore.QTimer()
        self._config_timer.setInterval(1000)  # every second
        self._config_timer.timeout.connect(self._check_config)
        self._config_timer.start()

    def _init_fields(self):
        # Script
        self._script = "pynopegl_utils.viewer.scenes"
        self._scene = "demo"

        # Rendering settings
        self._framerate = (60, 1)
        self._export_filename = (Path.home() / "nope.mp4").as_posix()

        # Export
        self._export_res = "1080p"
        self._export_profile = "mp4_h264_420"
        self._export_samples = 0

    def _load_config(self, data):
        self.script = data.get("script", self.script)
        self.scene = data.get("demo", self.scene)

        framerate = data.get("framerate", self.framerate)
        if framerate in self.FRAMERATES:
            self.framerate = framerate

        self.export_filename = data.get("export_filename", self.export_filename)

        export_res = data.get("export_res", self.export_res)
        if export_res in RESOLUTIONS.keys():
            self.export_res = export_res

        self.export_profile = data.get("export_profile", self.export_profile)
        self.export_samples = data.get("export_samples", self.export_samples)

    @QtCore.Slot()
    def _check_config(self):
        if not self._needs_saving:
            return

        config_dir = op.dirname(self.FILEPATH)
        os.makedirs(config_dir, exist_ok=True)

        config_file = open(self.FILEPATH, "w")

        cfg = dict(
            script=self.script,
            scene=self.scene,
            framerate=self.framerate,
            export_filename=self.export_filename,
            export_res=self.export_res,
            export_profile=self.export_profile,
            export_samples=self.export_samples,
        )
        json.dump(cfg, config_file, indent=4)
        config_file.close()

        self._needs_saving = False

    @property
    def script(self) -> str:
        return self._script

    @property
    def scene(self) -> str:
        return self._scene

    @property
    def framerate(self) -> Tuple[int, int]:
        return self._framerate

    @property
    def export_filename(self) -> str:
        return self._export_filename

    @property
    def export_res(self) -> str:
        return self._export_res

    @property
    def export_profile(self) -> str:
        return self._export_profile

    @property
    def export_samples(self) -> int:
        return self._export_samples

    @script.setter
    def script(self, script: str):
        self._script = script

    @scene.setter
    def scene(self, scene: str):
        self._scene = scene

    @framerate.setter
    def framerate(self, fr: Tuple[int, int]):
        if fr not in self.FRAMERATES:
            print(f"warning: unsupported framerate {fr}", file=sys.stderr)
            return
        self._framerate = fr

    @export_filename.setter
    def export_filename(self, filename: str):
        self._export_filename = filename

    @export_res.setter
    def export_res(self, res: str):
        if res not in self.EXPORT_RESOLUTIONS:
            print(f"warning: unsupported resolution {res}", file=sys.stderr)
            return
        self._export_res = res

    @export_profile.setter
    def export_profile(self, profile: str):
        if profile not in self.EXPORT_PROFILES:
            print(f"warning: unsupported export profile {profile}", file=sys.stderr)
            return
        self._export_profile = profile

    @export_samples.setter
    def export_samples(self, samples: int):
        if samples not in self.EXPORT_SAMPLES:
            print(f"warning: unsupported samples {samples}", file=sys.stderr)
            return
        self._export_samples = samples
