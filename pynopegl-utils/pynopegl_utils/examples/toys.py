#
# Copyright 2020-2022 GoPro Inc.
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

import os.path as op

import pynopegl as ngl


@ngl.scene()
def outline(cfg: ngl.SceneCfg):
    cfg.duration = 3
    animkf_outline = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(0.5, 0.06),
        ngl.AnimKeyFrameFloat(1, 0),
    ]
    animated_outline = ngl.AnimatedFloat(animkf_outline)
    return ngl.Text(
        text="outline",
        font_files=op.expanduser("~/.local/share/fonts/GloriaHallelujah-Regular.ttf"),
        aspect_ratio=cfg.aspect_ratio,
        padding=15,
        effects=[
            ngl.TextEffect(
                outline_color=(1, 0, 0.5),
                start=0,
                end=cfg.duration,
                outline=animated_outline,
            )
        ],
    )
