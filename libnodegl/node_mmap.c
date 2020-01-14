/*
 * Copyright 2020 GoPro Inc.
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

#include <stddef.h>
#include <string.h>

#include "memory.h"
#include "nodes.h"
#include "drawutils.h"
#include "log.h"
#include "math_utils.h"
#include "pipeline.h"
#include "program.h"
#include "type.h"
#include "topology.h"
#include "utils.h"

struct mmap_priv {
    struct ngl_node *child;

    struct texture texture;
    struct canvas canvas;
    struct program program;
    struct buffer vertices;
    struct buffer uvcoords;
    struct pipeline pipeline;

    int modelview_matrix_index;
    int projection_matrix_index;
};

#define OFFSET(x) offsetof(struct mmap_priv, x)
static const struct node_param mmap_params[] = {
    {"child",        PARAM_TYPE_NODE, OFFSET(child), .flags=PARAM_FLAG_CONSTRUCTOR,
                     .desc=NGLI_DOCSTRING("yep")},
    {NULL}
};

static void set_canvas_dimensions(struct canvas *canvas)
{
    canvas->w = 300;
    canvas->h = 200;
}

static int prepare_canvas(struct mmap_priv *s)
{
    set_canvas_dimensions(&s->canvas);

    /* Allocate, draw background, print mmap */
    s->canvas.buf = ngli_calloc(s->canvas.w * s->canvas.h, sizeof(*s->canvas.buf) * 4);
    if (!s->canvas.buf)
        return NGL_ERROR_MEMORY;
    const uint32_t bg = 0x333333ff;
    struct rect rect = {.w = s->canvas.w, .h = s->canvas.h};
    ngli_drawutils_draw_rect(&s->canvas, &rect, bg);
    return 0;
}

static const char * const vertex_data =
    "#version 100"                                                          "\n"
    "precision highp float;"                                                "\n"
    "attribute vec4 position;"                                              "\n"
    "attribute vec2 uvcoord;"                                               "\n"
    "uniform mat4 modelview_matrix;"                                        "\n"
    "uniform mat4 projection_matrix;"                                       "\n"
    "varying vec2 var_tex_coord;"                                           "\n"
    "void main()"                                                           "\n"
    "{"                                                                     "\n"
    "    gl_Position = projection_matrix * modelview_matrix * position;"    "\n"
    "    var_tex_coord = uvcoord;"                                          "\n"
    "}";

static const char * const fragment_data =
    "#version 100"                                                          "\n"
    "precision highp float;"                                                "\n"
    "uniform sampler2D tex;"                                                "\n"
    "varying vec2 var_tex_coord;"                                           "\n"
    "void main(void)"                                                       "\n"
    "{"                                                                     "\n"
    "    gl_FragColor = texture2D(tex, var_tex_coord);"                     "\n"
    "}";

#define C(index) box_corner[index]
#define W(index) box_width[index]
#define H(index) box_height[index]

static int mmap_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct mmap_priv *s = node->priv_data;

    int ret = prepare_canvas(s);
    if (ret < 0)
        return ret;

    static const float box_corner[] = {-1.0,-1.0, 0.0};
    static const float box_width[]  = { 2.0, 0.0, 0.0};
    static const float box_height[] = { 0.0, 2.0, 0.0};

    const float vertices[] = {
        C(0),               C(1),               C(2),
        C(0) + W(0),        C(1) + W(1),        C(2) + W(2),
        C(0) + H(0) + W(0), C(1) + H(1) + W(1), C(2) + H(2) + W(2),
        C(0) + H(0),        C(1) + H(1),        C(2) + H(2),
    };

    static const float uvs[] = {0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0};

    ret = ngli_program_init(&s->program, ctx, vertex_data, fragment_data, NULL);
    if (ret < 0)
        return ret;

    ret = ngli_buffer_init(&s->vertices, ctx, sizeof(vertices), NGLI_BUFFER_USAGE_STATIC);
    if (ret < 0)
        return ret;

    ret = ngli_buffer_upload(&s->vertices, vertices, sizeof(vertices));
    if (ret < 0)
        return ret;

    ret = ngli_buffer_init(&s->uvcoords, ctx, sizeof(uvs), NGLI_BUFFER_USAGE_STATIC);
    if (ret < 0)
        return ret;

    ret = ngli_buffer_upload(&s->uvcoords, uvs, sizeof(uvs));
    if (ret < 0)
        return ret;

    struct texture_params tex_params = NGLI_TEXTURE_PARAM_DEFAULTS;
    tex_params.width = s->canvas.w;
    tex_params.height = s->canvas.h;
    tex_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
    tex_params.min_filter = NGLI_FILTER_LINEAR;
    tex_params.mag_filter = NGLI_FILTER_NEAREST;
    tex_params.mipmap_filter = NGLI_MIPMAP_FILTER_LINEAR;
    ret = ngli_texture_init(&s->texture, ctx, &tex_params);
    if (ret < 0)
        return ret;

    ret = ngli_texture_upload(&s->texture, s->canvas.buf, 0);
    if (ret < 0)
        return ret;

    const struct pipeline_uniform uniforms[] = {
        {.name = "modelview_matrix",  .type = NGLI_TYPE_MAT4, .count = 1, .data = NULL},
        {.name = "projection_matrix", .type = NGLI_TYPE_MAT4, .count = 1, .data = NULL},
    };

    const struct pipeline_texture textures[] = {
        {.name  = "tex", .texture = &s->texture},
    };

    const struct pipeline_attribute attributes[] = {
        {.name = "position", .format = NGLI_FORMAT_R32G32B32_SFLOAT, .stride = 3 * 4, .buffer = &s->vertices},
        {.name = "uvcoord",  .format = NGLI_FORMAT_R32G32_SFLOAT,    .stride = 2 * 4, .buffer = &s->uvcoords},
    };

    struct pipeline_params pipeline_params = {
        .type          = NGLI_PIPELINE_TYPE_GRAPHICS,
        .program       = &s->program,
        .textures      = textures,
        .nb_textures   = NGLI_ARRAY_NB(textures),
        .uniforms      = uniforms,
        .nb_uniforms   = NGLI_ARRAY_NB(uniforms),
        .attributes    = attributes,
        .nb_attributes = NGLI_ARRAY_NB(attributes),
        .graphics      = {
            .topology    = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
            .nb_vertices = 4,
        }
    };

    ret = ngli_pipeline_init(&s->pipeline, ctx, &pipeline_params);
    if (ret < 0)
        return ret;

    s->modelview_matrix_index = ngli_pipeline_get_uniform_index(&s->pipeline, "modelview_matrix");
    s->projection_matrix_index = ngli_pipeline_get_uniform_index(&s->pipeline, "projection_matrix");

    return 0;
}

static int mmap_update(struct ngl_node *node, double t)
{
    struct mmap_priv *s = node->priv_data;
    return ngli_node_update(s->child, t);
}

static void mmap_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct mmap_priv *s = node->priv_data;

    const float *modelview_matrix  = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    ngli_pipeline_update_uniform(&s->pipeline, s->modelview_matrix_index, modelview_matrix);
    ngli_pipeline_update_uniform(&s->pipeline, s->projection_matrix_index, projection_matrix);

    ngli_pipeline_exec(&s->pipeline);
}

static void mmap_uninit(struct ngl_node *node)
{
    struct mmap_priv *s = node->priv_data;
    ngli_pipeline_reset(&s->pipeline);
    ngli_texture_reset(&s->texture);
    ngli_buffer_reset(&s->vertices);
    ngli_buffer_reset(&s->uvcoords);
    ngli_program_reset(&s->program);
    ngli_free(s->canvas.buf);
}

const struct node_class ngli_mmap_class = {
    .id        = NGL_NODE_MMAP,
    .name      = "MMap",
    .init      = mmap_init,
    .update    = mmap_update,
    .draw      = mmap_draw,
    .uninit    = mmap_uninit,
    .priv_size = sizeof(struct mmap_priv),
    .params    = mmap_params,
    .file      = __FILE__,
};

