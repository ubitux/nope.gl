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

import textwrap

import pynopegl as ngl
from pynopegl_utils.misc import load_media
from pynopegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.tests.cmp_resources import test_resources
from pynopegl_utils.toolbox.colors import COLORS
from pynopegl_utils.toolbox.grid import autogrid_simple


def _get_time_scene(cfg: ngl.SceneCfg):
    m0 = load_media("mire")

    media_seek = 10
    noop_duration = 2
    prefetch_duration = 2
    freeze_duration = 3
    playback_duration = 5

    range_start = noop_duration + prefetch_duration
    play_start = range_start + freeze_duration
    play_stop = play_start + playback_duration
    range_stop = play_stop + freeze_duration
    duration = range_stop + noop_duration

    cfg.duration = duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media_animkf = [
        ngl.AnimKeyFrameFloat(play_start, media_seek),
        ngl.AnimKeyFrameFloat(play_stop, media_seek + playback_duration),
    ]

    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(media_animkf))
    t = ngl.Texture2D(data_src=m, min_filter="nearest", mag_filter="nearest")
    r = ngl.DrawTexture(t)

    rf = ngl.TimeRangeFilter(r, range_start, range_stop, prefetch_time=prefetch_duration)

    return rf


@test_fingerprint(width=320, height=240, keyframes=3, tolerance=1)
@ngl.scene()
def media_flat_remap(cfg: ngl.SceneCfg):
    m0 = load_media("mire")
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media_animkf = [
        ngl.AnimKeyFrameFloat(cfg.duration / 2, 1.833),
    ]

    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(media_animkf))
    t = ngl.Texture2D(data_src=m, min_filter="nearest", mag_filter="nearest")
    return ngl.DrawTexture(t)


@test_cuepoints(
    width=320,
    height=240,
    points={"X": (0, -0.625)},
    keyframes=15,
    clear_color=list(COLORS.violet) + [1],
    tolerance=1,
)
@ngl.scene()
def media_phases_display(cfg: ngl.SceneCfg):
    return _get_time_scene(cfg)


@test_resources(width=320, height=240, keyframes=15)
@ngl.scene()
def media_phases_resources(cfg: ngl.SceneCfg):
    return _get_time_scene(cfg)


# Note: the following test only makes sure the clamping code shader compiles,
# not check for an actual overflow
@test_cuepoints(width=320, height=240, points={"X": (0, -0.625)}, keyframes=1, tolerance=1)
@ngl.scene()
def media_clamp(cfg: ngl.SceneCfg):
    m0 = load_media("mire")
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media = ngl.Media(m0.filename)
    texture = ngl.Texture2D(data_src=media, clamp_video=True, min_filter="nearest", mag_filter="nearest")
    return ngl.DrawTexture(texture)


@test_cuepoints(width=320, height=240, points={f"P{i}": (i / 5 * 2 - 1, 0) for i in range(5)}, keyframes=5, tolerance=1)
@ngl.scene()
def media_exposed_time(cfg: ngl.SceneCfg):
    m0 = load_media("mire")
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)

    vert = textwrap.dedent(
        """\
        void main()
        {
            ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
            uv = ngl_uvcoord;
        }
        """
    )

    frag = textwrap.dedent(
        """\
        void main()
        {
            ngl_out_color = vec4(vec3(step(0.0, tex0_ts/duration - uv.x)), 1.0);
        }
        """
    )

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    media = ngl.Media(m0.filename)
    texture = ngl.Texture2D(data_src=media, min_filter="nearest", mag_filter="nearest")
    program = ngl.Program(vertex=vert, fragment=frag)
    program.update_vert_out_vars(uv=ngl.IOVec2())
    draw = ngl.Draw(quad, program)
    draw.update_frag_resources(tex0=texture, duration=ngl.UniformFloat(cfg.duration))
    return draw


@test_fingerprint(width=1024, height=1024, keyframes=30, tolerance=2)
@ngl.scene(controls=dict(overlap_time=ngl.scene.Range(range=[0, 10], unit_base=10), dim=ngl.scene.Range(range=[1, 10])))
def media_queue(cfg: ngl.SceneCfg, overlap_time=7.0, dim=3):
    cfg.duration = 10
    cfg.aspect_ratio = (1, 1)

    nb_medias = dim * dim

    medias = [load_media(m).filename for m in ("mire", "cat", "rooster", "panda")]

    queued_medias = []
    for video_id in range(nb_medias):
        start = video_id * cfg.duration / nb_medias
        end = start + cfg.duration / nb_medias + overlap_time

        animkf = [
            ngl.AnimKeyFrameFloat(start, 0),
            ngl.AnimKeyFrameFloat(start + cfg.duration, cfg.duration),
        ]
        media = ngl.Media(medias[video_id % len(medias)], time_anim=ngl.AnimatedTime(animkf))

        texture = ngl.Texture2D(data_src=media, min_filter="linear", mag_filter="linear")
        draw = ngl.DrawTexture(texture)

        rf = ngl.TimeRangeFilter(draw, start, end)

        queued_medias.append(rf)

    return autogrid_simple(queued_medias)


@test_fingerprint(width=320, height=240, keyframes=20, tolerance=1)
@ngl.scene()
def media_timeranges_rtt(cfg: ngl.SceneCfg):
    m0 = load_media("mire")
    cfg.duration = d = 10
    cfg.aspect_ratio = (m0.width, m0.height)

    # Use a media/texture as leaf to exercise its prefetch/release mechanism
    media = ngl.Media(m0.filename)
    texture = ngl.Texture2D(data_src=media, min_filter="nearest", mag_filter="nearest")

    # Diamond tree on the same media texture
    draw0 = ngl.DrawTexture(texture, label="leaf 0")
    draw1 = ngl.DrawTexture(texture, label="leaf 1")

    # Create intermediate RTT "proxy" to exercise prefetch/release at this
    # level as well
    dst_tex0 = ngl.Texture2D(width=m0.width, height=m0.height, min_filter="nearest", mag_filter="nearest")
    dst_tex1 = ngl.Texture2D(width=m0.width, height=m0.height, min_filter="nearest", mag_filter="nearest")
    rtt0 = ngl.RenderToTexture(draw0, [dst_tex0])
    rtt1 = ngl.RenderToTexture(draw1, [dst_tex1])

    # Render the 2 RTTs vertically split (one half content each)
    quad0 = ngl.Quad((-1, -1, 0), (1, 0, 0), (0, 2, 0), uv_corner=(0, 0), uv_width=(0.5, 0))
    quad1 = ngl.Quad((0, -1, 0), (1, 0, 0), (0, 2, 0), uv_corner=(0.5, 0), uv_width=(0.5, 0))
    rtt_draw0 = ngl.DrawTexture(dst_tex0, geometry=quad0, label="draw RTT 0")
    rtt_draw1 = ngl.DrawTexture(dst_tex1, geometry=quad1, label="draw RTT 1")
    proxy0 = ngl.Group(children=[rtt0, rtt_draw0], label="proxy 0")
    proxy1 = ngl.Group(children=[rtt1, rtt_draw1], label="proxy 1")

    # We want to make sure the idle times are enough to exercise the
    # prefetch/release mechanism
    prefetch_time = 1
    assert prefetch_time < d / 5

    # Split the presentation in 5 segments such that there are inactive times,
    # prefetch times and both overlapping and non-overlapping times for the
    # RTTs
    trange0 = ngl.TimeRangeFilter(proxy0, start=1 / 5 * d, end=3 / 5 * d, prefetch_time=prefetch_time, label="left")
    trange1 = ngl.TimeRangeFilter(proxy1, start=2 / 5 * d, end=4 / 5 * d, prefetch_time=prefetch_time, label="right")

    return ngl.Group(children=[trange0, trange1])
