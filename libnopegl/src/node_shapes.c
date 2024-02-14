/*
 * Copyright 2024 Clément Bœsch <u pkh.me>
 * Copyright 2024 Nope Forge
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

#include "darray.h"
#include "filterschain.h"
#include "internal.h"
#include "nopegl.h"
#include "pgcraft.h"
#include "type.h"

/* GLSL filters as string */
#include "shape_rectangle.h"
#include "shape_circle.h"
#include "shape_triangle.h"
#include "shape_ngon.h"
#include "utils.h"

extern const struct param_choices ngli_display_layout_choices;

struct shaperectangle_opts {
    struct ngl_node *size_node;
    float size[2];
    struct ngl_node *rounding_node;
    float rounding[4];
    struct shape_common_opts common;
};

struct shapecircle_opts {
    struct ngl_node *radius_node;
    float radius;
    struct shape_common_opts common;
};

struct shapetriangle_opts {
    struct ngl_node *radius_node;
    float radius;
    struct ngl_node *rounding_node;
    float rounding;
    struct shape_common_opts common;
};

struct shapengon_opts {
    struct ngl_node *n_node;
    int32_t n;
    struct ngl_node *radius_node;
    float radius;
    struct ngl_node *rounding_node;
    float rounding;
    struct shape_common_opts common;
};

NGLI_STATIC_ASSERT(filter_on_top_of_shape_priv, offsetof(struct shape_priv, filter) == 0);

#define COMMON_PARAMS                                                                            \
    {"diffusion", NGLI_PARAM_TYPE_F32, OFFSET(common.diffusion_node), {.f32=0.f},                \
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,         \
                  .desc=NGLI_DOCSTRING("how much the border smoothly spreads / gets blurry)")},  \
    {"layout",      NGLI_PARAM_TYPE_SELECT, OFFSET(common.layout), {.i32=NGLI_LAYOUT_FIT},       \
                    .choices=&ngli_display_layout_choices,                                       \
                    .desc=NGLI_DOCSTRING("aspect layout policy")},                               \

#define OFFSET(x) offsetof(struct shaperectangle_opts, x)
static const struct node_param shaperectangle_params[] = {
    {"size",      NGLI_PARAM_TYPE_VEC2, OFFSET(size_node), {.vec={1.f, 1.f}},
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                  .desc=NGLI_DOCSTRING("width and height")},
    {"rounding",  NGLI_PARAM_TYPE_VEC4, OFFSET(rounding_node),
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                  .desc=NGLI_DOCSTRING("corners rounding (top-left, top-right, bottom-right, bottom-left)")},
    COMMON_PARAMS
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct shapecircle_opts, x)
static const struct node_param shapecircle_params[] = {
    {"radius",    NGLI_PARAM_TYPE_F32, OFFSET(radius_node), {.f32=1.f},
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                  .desc=NGLI_DOCSTRING("circle radius")},
    COMMON_PARAMS
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct shapetriangle_opts, x)
static const struct node_param shapetriangle_params[] = {
    {"radius",    NGLI_PARAM_TYPE_F32, OFFSET(radius_node), {.f32=1.f},
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                  .desc=NGLI_DOCSTRING("circle radius")},
    {"rounding",  NGLI_PARAM_TYPE_F32, OFFSET(rounding_node),
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                  .desc=NGLI_DOCSTRING("edges rounding")},
    COMMON_PARAMS
    {NULL}
};
#undef OFFSET

#define OFFSET(x) offsetof(struct shapengon_opts, x)
static const struct node_param shapengon_params[] = {
    {"n",         NGLI_PARAM_TYPE_I32, OFFSET(n_node), {.i32=5},
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                  .desc=NGLI_DOCSTRING("number of points of the N-gon")},
    {"radius",    NGLI_PARAM_TYPE_F32, OFFSET(radius_node), {.f32=1.f},
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                  .desc=NGLI_DOCSTRING("circle radius")},
    {"rounding",  NGLI_PARAM_TYPE_F32, OFFSET(rounding_node),
                  .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE | NGLI_PARAM_FLAG_ALLOW_NODE,
                  .desc=NGLI_DOCSTRING("edges rounding")},
    COMMON_PARAMS
    {NULL}
};
#undef OFFSET

static int register_resource(struct darray *resources, const char *name,
                             const struct ngl_node *pnode, void *data, int data_type)
{
    struct pgcraft_uniform res = {
        .type  = data_type,
        .stage = NGLI_PROGRAM_SHADER_FRAG,
        .data  = ngli_node_get_data_ptr(pnode, data),
    };
    snprintf(res.name, sizeof(res.name), "%s", name);
    if (!ngli_darray_push(resources, &res))
        return NGL_ERROR_MEMORY;
    return 0;
}

static int shape_init(struct ngl_node *node)
{
    struct shape_priv *s = node->priv_data;
    ngli_darray_init(&s->filter.resources, sizeof(struct pgcraft_uniform), 0);
    return 0;
}

static int shaperectangle_init(struct ngl_node *node)
{
    int ret = shape_init(node);
    if (ret < 0)
        return ret;
    struct shape_priv *s = node->priv_data;
    struct shaperectangle_opts *o = node->opts;
    s->common_opts = &o->common;
    s->filter.name = "rectangle";
    s->filter.code = shape_rectangle_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_SHAPES;
    if ((ret = register_resource(&s->filter.resources, "size", o->size_node, &o->size, NGLI_TYPE_VEC2)) < 0 ||
        (ret = register_resource(&s->filter.resources, "rounding", o->rounding_node, &o->rounding, NGLI_TYPE_VEC4)) < 0 ||
        (ret = register_resource(&s->filter.resources, "diffusion", o->common.diffusion_node, &o->common.diffusion, NGLI_TYPE_F32)) < 0)
        return ret;
    return 0;
}

static int shapetriangle_init(struct ngl_node *node)
{
    int ret = shape_init(node);
    if (ret < 0)
        return ret;
    struct shape_priv *s = node->priv_data;
    struct shapetriangle_opts *o = node->opts;
    s->common_opts = &o->common;
    s->filter.name = "triangle";
    s->filter.code = shape_triangle_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_SHAPES;
    if ((ret = register_resource(&s->filter.resources, "radius", o->radius_node, &o->radius, NGLI_TYPE_F32)) < 0 ||
        (ret = register_resource(&s->filter.resources, "rounding", o->rounding_node, &o->rounding, NGLI_TYPE_F32)) < 0 ||
        (ret = register_resource(&s->filter.resources, "diffusion", o->common.diffusion_node, &o->common.diffusion, NGLI_TYPE_F32)) < 0)
        return ret;
    return 0;
}

static int shapecircle_init(struct ngl_node *node)
{
    int ret = shape_init(node);
    if (ret < 0)
        return ret;
    struct shape_priv *s = node->priv_data;
    struct shapecircle_opts *o = node->opts;
    s->common_opts = &o->common;
    s->filter.name = "circle";
    s->filter.code = shape_circle_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_SHAPES;
    if ((ret = register_resource(&s->filter.resources, "radius", o->radius_node, &o->radius, NGLI_TYPE_F32)) < 0 ||
        (ret = register_resource(&s->filter.resources, "diffusion", o->common.diffusion_node, &o->common.diffusion, NGLI_TYPE_F32)) < 0)
        return ret;
    return 0;
}

static int shapengon_init(struct ngl_node *node)
{
    int ret = shape_init(node);
    if (ret < 0)
        return ret;
    struct shape_priv *s = node->priv_data;
    struct shapengon_opts *o = node->opts;
    s->common_opts = &o->common;
    s->filter.name = "ngon";
    s->filter.code = shape_ngon_glsl;
    s->filter.helpers = NGLI_FILTER_HELPER_SHAPES;
    if ((ret = register_resource(&s->filter.resources, "n", o->n_node, &o->n, NGLI_TYPE_I32)) < 0 ||
        (ret = register_resource(&s->filter.resources, "radius", o->radius_node, &o->radius, NGLI_TYPE_F32)) < 0 ||
        (ret = register_resource(&s->filter.resources, "rounding", o->rounding_node, &o->rounding, NGLI_TYPE_F32)) < 0 ||
        (ret = register_resource(&s->filter.resources, "diffusion", o->common.diffusion_node, &o->common.diffusion, NGLI_TYPE_F32)) < 0)
        return ret;
    return 0;
}

static void shape_uninit(struct ngl_node *node)
{
    struct shape_priv *s = node->priv_data;
    ngli_darray_reset(&s->filter.resources);
}

#define DECLARE_SHAPE(type, cls_id, cls_opts_size, cls_name)  \
const struct node_class ngli_shape##type##_class = {          \
    .id        = cls_id,                                      \
    .name      = cls_name,                                    \
    .init      = shape##type##_init,                          \
    .update    = ngli_node_update_children,                   \
    .uninit    = shape_uninit,                                \
    .opts_size = cls_opts_size,                               \
    .priv_size = sizeof(struct shape_priv),                   \
    .params    = shape##type##_params,                        \
    .file      = __FILE__,                                    \
};

DECLARE_SHAPE(rectangle,  NGL_NODE_SHAPERECTANGLE, sizeof(struct shaperectangle_opts), "ShapeRectangle")
DECLARE_SHAPE(triangle,   NGL_NODE_SHAPETRIANGLE,  sizeof(struct shapetriangle_opts),  "ShapeTriangle")
DECLARE_SHAPE(circle,     NGL_NODE_SHAPECIRCLE,    sizeof(struct shapecircle_opts),    "ShapeCircle")
DECLARE_SHAPE(ngon,       NGL_NODE_SHAPENGON,      sizeof(struct shapengon_opts),      "ShapeNGon")
