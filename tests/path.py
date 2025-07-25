#
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

import pynopegl as ngl
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint


def _shape_variant_0(cfg: ngl.SceneCfg, *kfs):
    cfg.aspect_ratio = (1, 1)
    keyframes = (
        [
            ngl.PathKeyMove(to=(-0.55, -0.9, 0)),
            ngl.PathKeyLine(to=(-0.55, -0.2, 0)),
            ngl.PathKeyLine(to=(-0.4, -0.2, 0)),
            ngl.PathKeyLine(to=(-0.4, -0.75, 0)),
        ]
        + list(kfs)
        + [
            ngl.PathKeyLine(to=(0.2, -0.9, 0)),
            ngl.PathKeyClose(),
        ]
    )
    path = ngl.Path(keyframes)
    return ngl.DrawPath(path, viewbox=(-1, -1, 2, 1))


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_shape_0(cfg: ngl.SceneCfg):
    kf = ngl.PathKeyLine(to=(0.2, -0.75, 0))
    return _shape_variant_0(cfg, kf)


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_shape_1(cfg: ngl.SceneCfg):
    kf = ngl.PathKeyBezier2(to=(0.2, -0.75, 0), control=(-0.1, -0.5, 0))
    return _shape_variant_0(cfg, kf)


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_shape_2(cfg: ngl.SceneCfg):
    kf = ngl.PathKeyBezier3(to=(0.2, -0.75, 0), control1=(0.0, -0.2, 0), control2=(-0.2, -1.0, 0))
    return _shape_variant_0(cfg, kf)


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_shape_3(cfg: ngl.SceneCfg):
    kf = ngl.PathKeyBezier3(to=(0.2, -0.75, 0), control1=(0.0, -1.0, 0), control2=(-0.2, -0.2, 0))
    return _shape_variant_0(cfg, kf)


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_shape_4(cfg: ngl.SceneCfg):
    kf = ngl.PathKeyBezier2(to=(0.2, -0.75, 0), control=(-0.1, -0.95, 0))
    return _shape_variant_0(cfg, kf)


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_shape_5(cfg: ngl.SceneCfg):
    kfs = [
        ngl.PathKeyLine(to=(-0.1, -0.65, 0)),
        ngl.PathKeyLine(to=(0.2, -0.75, 0)),
    ]
    return _shape_variant_0(cfg, *kfs)


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_shape_6(cfg: ngl.SceneCfg):
    kfs = [
        ngl.PathKeyLine(to=(-0.1, -0.85, 0)),
        ngl.PathKeyLine(to=(0.2, -0.75, 0)),
    ]
    return _shape_variant_0(cfg, *kfs)


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_shape_7(cfg: ngl.SceneCfg):
    kfs = [
        ngl.PathKeyLine(to=(-0.2, -0.65, 0)),
        ngl.PathKeyLine(to=(0.0, -0.85, 0)),
        ngl.PathKeyLine(to=(0.2, -0.75, 0)),
    ]
    return _shape_variant_0(cfg, *kfs)


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_shape_8(cfg: ngl.SceneCfg):
    kfs = [
        ngl.PathKeyLine(to=(-0.2, -0.85, 0)),
        ngl.PathKeyLine(to=(0.0, -0.65, 0)),
        ngl.PathKeyLine(to=(0.2, -0.75, 0)),
    ]
    return _shape_variant_0(cfg, *kfs)


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_parabola(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    orig = (-0.4, -0.75, 0)
    keyframes = [
        ngl.PathKeyMove(to=orig),
        ngl.PathKeyBezier2(to=(0.4, -0.75, 0), control=(0, 0.5, 0)),
        ngl.PathKeyLine(to=orig),
    ]
    path = ngl.Path(keyframes)
    return ngl.DrawPath(path, viewbox=(-1, -1, 2, 1))


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_flip_t(cfg: ngl.SceneCfg, x_base=-0.5, y_base=-0.5):
    cfg.aspect_ratio = (1, 1)
    keyframes = [
        ngl.PathKeyMove(to=(x_base + 1 / 8, y_base + 4 / 8, 0)),
        ngl.PathKeyLine(to=(x_base + 1 / 8, y_base + 3 / 8, 0)),
        ngl.PathKeyLine(to=(x_base + 7 / 8, y_base + 3 / 8, 0)),
        ngl.PathKeyLine(to=(x_base + 7 / 8, y_base + 4 / 8, 0)),
        ngl.PathKeyLine(to=(x_base + 5 / 8, y_base + 4 / 8, 0)),
        ngl.PathKeyLine(to=(x_base + 5 / 8, y_base + 6 / 8, 0)),
        ngl.PathKeyLine(to=(x_base + 4 / 8, y_base + 6 / 8, 0)),
        ngl.PathKeyLine(to=(x_base + 4 / 8, y_base + 4 / 8, 0)),
        ngl.PathKeyLine(to=(x_base + 1 / 8, y_base + 4 / 8, 0)),
    ]

    path = ngl.Path(keyframes)
    return ngl.DrawPath(path)


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_pie_slice(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    keyframes = [
        ngl.PathKeyMove(to=(0.3, 0.261727, 0)),
        ngl.PathKeyBezier3(control1=(0.35, 0.256288, 0), control2=(0.38, 0.253569, 0), to=(0.45, 0.253569, 0)),
        ngl.PathKeyLine(to=(0.4, 0.5, 0)),
        ngl.PathKeyLine(to=(0.3, 0.261727, 0)),
    ]

    path = ngl.Path(keyframes)
    return ngl.DrawPath(path, box=(-1.5, -1, 3.5, 2), viewbox=(0.2, 0.2, 0.4, 0.4))


def _get_overlap_shape0():
    return [
        ngl.PathKeyMove(to=(1, 5, 0)),
        ngl.PathKeyBezier2(control=(1, 7, 0), to=(3, 7, 0)),
        ngl.PathKeyBezier2(control=(6, 9, 0), to=(6, 5, 0)),
        ngl.PathKeyBezier3(control1=(5, 2, 0), control2=(2, 2, 0), to=(1, 5, 0)),
    ]


def _get_overlap_shape1(clockwise):
    a, b, c, d = (3, 4, 0), (6, 7, 0), (9, 5, 0), (7, 1, 0)
    if not clockwise:
        a, b, c, d = d, c, b, a
    return [
        ngl.PathKeyMove(to=a),
        ngl.PathKeyLine(to=b),
        ngl.PathKeyLine(to=c),
        ngl.PathKeyLine(to=d),
        ngl.PathKeyLine(to=a),
    ]


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_overlap_add(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    keyframes = _get_overlap_shape0() + _get_overlap_shape1(clockwise=True)
    path = ngl.Path(keyframes)
    return ngl.DrawPath(path, viewbox=(0, 0, 10, 10))


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_overlap_xor(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    keyframes = _get_overlap_shape0() + _get_overlap_shape1(clockwise=False)
    path = ngl.Path(keyframes)
    return ngl.DrawPath(path, viewbox=(0, 0, 10, 10))


@test_fingerprint(width=640, height=640)
@ngl.scene()
def path_open_and_effects(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    keyframes = [
        ngl.PathKeyMove(to=(-0.7, 0.0, 0.0)),
        ngl.PathKeyBezier3(control1=(-0.2, -0.9, 0.0), control2=(0.2, 0.8, 0.0), to=(0.8, 0.0, 0.0)),
    ]
    path = ngl.Path(keyframes)
    return ngl.DrawPath(
        path,
        outline=0.05,
        outline_color=(1, 0, 0.5),
        blur=0.03,
        glow=0.2,
        glow_color=(1, 0.5, 0),
    )
