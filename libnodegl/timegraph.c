/*
 * Copyright 2019 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "nodegl.h"
#include "nodes.h"
#include "timerange.h"

#define W 2.0
#define H 2.0
#define PAD 0.03
#define TEXT_RATIO (1 / 6.)
#define TEXT_W (TEXT_RATIO * W)
#define TIME_W (W - TEXT_W)

static const char * const shader = \
"#version 100"                      "\n"
"precision mediump float;"          "\n"
"uniform vec4 color;"               "\n"
"void main()"                       "\n"
"{"                                 "\n"
"    gl_FragColor = color;"         "\n"
"}";

static const float text_bgcolor[4] = {0.2, 0.2, 0.2, 1.0};

static struct ngl_node *get_pass_text(const char *label, float x, float y, float w, float h, const int *ar)
{
    const float box_corner[3] = {x,   y + PAD, 0.0};
    const float box_width[3]  = {w, 0.0, 0.0};
    const float box_height[3] = {0.0, h - PAD, 0.0};

    struct ngl_node *text = ngl_node_create(NGL_NODE_TEXT, label);

    ngl_node_param_set(text, "box_corner", box_corner);
    ngl_node_param_set(text, "box_width",  box_width);
    ngl_node_param_set(text, "box_height", box_height);
    ngl_node_param_set(text, "bg_color", text_bgcolor);
    ngl_node_param_set(text, "aspect_ratio", ar);
    return text;
}

static struct ngl_node *get_segment(float x, float y, float w, float h, const float *color)
{
    const float corner[3] = {x,   y + PAD, 0.0};
    const float width[3]  = {w, 0.0, 0.0};
    const float height[3] = {0.0, h - PAD, 0.0};

    struct ngl_node *quad    = ngl_node_create(NGL_NODE_QUAD);
    struct ngl_node *program = ngl_node_create(NGL_NODE_PROGRAM);
    struct ngl_node *render  = ngl_node_create(NGL_NODE_RENDER, quad);
    struct ngl_node *u_color = ngl_node_create(NGL_NODE_UNIFORMVEC4);

    ngl_node_param_set(quad, "corner", corner);
    ngl_node_param_set(quad, "width",  width);
    ngl_node_param_set(quad, "height", height);

    ngl_node_param_set(program, "fragment", shader);
    ngl_node_param_set(u_color, "value", color);

    ngl_node_param_set(render, "program", program);
    ngl_node_param_set(render, "uniforms", "color", u_color);

    ngl_node_unrefp(&program);
    ngl_node_unrefp(&quad);
    ngl_node_unrefp(&u_color);
    return render;
}

// TODO 2 colors (fade with time)
// TODO prefetch colors
static const float segment_colors[3][4] = {
    [NGLI_TIMERANGE_ONCE] = {0.6, 0.9, 0.6, 1.0},
    [NGLI_TIMERANGE_NOOP] = {0.5, 0.3, 0.3, 1.0},
    [NGLI_TIMERANGE_CONT] = {0.6, 0.6, 0.9, 1.0},
};

// TODO vertical line
static struct ngl_node *get_pass_node(const struct pass_info *pass, double duration,
                                      float y, float h, const int *ar)
{
    const struct darray *ranges_array = &pass->timerange.ranges;
    const struct timerangemode *ranges = ngli_darray_data(ranges_array);
    const int nb_ranges = ngli_darray_count(ranges_array);

    struct ngl_node *group = ngl_node_create(NGL_NODE_GROUP);

    struct ngl_node *text = get_pass_text(pass->label, -W/2., y, TEXT_W, h, ar);

    ngl_node_param_add(group, "children", 1, &text);

    if (!nb_ranges || ranges[0].start_time > 0.) {
        const double start_time = 0.;
        const double end_time = nb_ranges ? ranges[0].start_time : duration;
        const float seg_w_ratio = (end_time - start_time) / duration;
        const float seg_w = seg_w_ratio * TIME_W;
        const float seg_x_ratio = start_time / duration;
        const float seg_x = -W/2. + TEXT_W + seg_x_ratio * TIME_W;
        const float *color = segment_colors[NGLI_TIMERANGE_CONT];
        struct ngl_node *segment = get_segment(seg_x, y, seg_w, h, color);

        ngl_node_param_add(group, "children", 1, &segment);
        ngl_node_unrefp(&segment);
    }

    for (int i = 0; i < nb_ranges; i++) {
        const double start_time = ranges[i].start_time;
        const double end_time = i < nb_ranges - 1 ? ranges[i + 1].start_time : duration;
        const float seg_w_ratio = (end_time - start_time) / duration;
        const float seg_w = seg_w_ratio * TIME_W;
        const float seg_x_ratio = start_time / duration;
        const float seg_x = -W/2. + TEXT_W + seg_x_ratio * TIME_W;
        const float *color = segment_colors[ranges[i].type];
        struct ngl_node *segment = get_segment(seg_x, y, seg_w, h, color);

        ngl_node_param_add(group, "children", 1, &segment);
        ngl_node_unrefp(&segment);
    }

    return group;
}

struct ngl_node *ngl_node_timegraph(const struct ngl_node *node, double duration, const int *ar)
{
    struct darray passes_array;

    // XXX const
    int ret = ngli_node_track_passes(node, &passes_array);
    if (ret < 0)
        return NULL;

    const struct pass_info *passes = ngli_darray_data(&passes_array);
    const int nb_passes = ngli_darray_count(&passes_array);
    struct ngl_node *group = ngl_node_create(NGL_NODE_GROUP);

    const float pass_h = H / (float)nb_passes;
    for (int i = 0; i < nb_passes; i++) {
        const struct pass_info *pass = &passes[i];
        const float pass_y = H/2. - (i + 1) * pass_h;
        struct ngl_node *pass_node = get_pass_node(pass, duration, pass_y, pass_h, ar);

        ngl_node_param_add(group, "children", 1, &pass_node);
        ngl_node_unrefp(&pass_node);
    }

    return group;
}
