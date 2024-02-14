#
# Copyright 2024 Nope Forge
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

from pynopegl_utils.misc import load_media

import pynopegl as ngl


@ngl.scene(
    controls=dict(
        box_resize=ngl.scene.Bool(),
        layout=ngl.scene.List(choices=["stretch", "fit", "cover"]),
        shape_layout=ngl.scene.List(choices=["stretch", "fit", "cover"]),
    )
)
def aspect_fit(
    cfg: ngl.SceneCfg,
    box_resize=True,
    layout="fit",
    shape_layout="stretch",
):
    cfg.aspect_ratio = (16, 9)

    media = load_media("panda")

    box = (-0.7, -0.6, 1.1, 1.4)
    ar = cfg.aspect_ratio_float
    bs = 0.1
    bs = 0

    tex = ngl.Texture2D(data_src=ngl.Media(filename=media.filename))
    root = ngl.Group(
        children=[
            ngl.DrawGradient4(),
            ngl.Rotate(
                ngl.Group(
                    children=[
                        ngl.Scale(
                            child=ngl.DrawColor(
                                box=box,
                                box_resize=tex if box_resize else None,
                                # how the content fits into the geometry
                                # how the geometry is resized to fit the content
                                layout=layout,
                                opacity=0.5,
                                blending="src_over",
                                label="whitebox",
                                shape=ngl.ShapeRectangle(
                                    diffusion=0.05,
                                    # layout=shape_layout,
                                ),
                            ),
                            factors=(1 + bs / ar, 1 + bs, 1),
                            anchor=(box[0] + box[2] / 2, box[1] + box[3] / 2, 0),
                        ),
                        ngl.DrawTexture(
                            texture=ngl.Translate(
                                tex,
                                # vector=(-0.5, -0.4, 0),
                            ),
                            box=box,
                            box_resize=tex if box_resize else None,
                            layout=layout,
                            blending="src_over",
                            label="texture",
                            shape=ngl.Translate(
                                ngl.ShapeCircle(
                                    radius=ngl.UniformFloat(value=1.0, live_id="shape_radius"),
                                    diffusion=0.05,
                                    layout=shape_layout,
                                ),
                                # vector=(-0.5, -0.4, 0),
                            ),
                        ),
                    ]
                ),
                angle=ngl.UniformFloat(value=0, live_id="rotate", live_min=0, live_max=360),
                anchor=(box[0] + box[2] / 2, box[1] + box[3] / 2, 0),
            ),
        ]
    )
    return root
    return ngl.Camera(
        root,
        orthographic=(-cfg.aspect_ratio_float, cfg.aspect_ratio_float, -1, 1),
        clipping=(1, 10),
        eye=(0, 0, 2),
        center=(0, 0, 0),
        up=(0, 1, 0),
    )
