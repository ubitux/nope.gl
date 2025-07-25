/*
 * Copyright 2021-2022 GoPro Inc.
 * Copyright 2021-2022 Clément Bœsch <u pkh.me>
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

#include "animation.h"
#include "internal.h"
#include "math_utils.h"
#include "ngpu/type.h"
#include "node_animkeyframe.h"
#include "node_uniform.h"
#include "node_velocity.h"

struct velocity_opts {
    struct ngl_node *anim_node;
};

#define OFFSET(x) offsetof(struct velocity_opts, x)
static const struct node_param velocityfloat_params[] = {
    {"animation", NGLI_PARAM_TYPE_NODE, OFFSET(anim_node), .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMATEDFLOAT, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("1D animation to analyze the velocity from")},
    {NULL}
};

static const struct node_param velocityvec2_params[] = {
    {"animation", NGLI_PARAM_TYPE_NODE, OFFSET(anim_node), .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMATEDVEC2, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("2D animation to analyze the velocity from")},
    {NULL}
};

static const struct node_param velocityvec3_params[] = {
    {"animation", NGLI_PARAM_TYPE_NODE, OFFSET(anim_node), .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMATEDVEC3, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("3D animation to analyze the velocity from")},
    {NULL}
};

static const struct node_param velocityvec4_params[] = {
    {"animation", NGLI_PARAM_TYPE_NODE, OFFSET(anim_node), .flags=NGLI_PARAM_FLAG_NON_NULL,
                  .node_types=(const uint32_t[]){NGL_NODE_ANIMATEDVEC4, NGLI_NODE_NONE},
                  .desc=NGLI_DOCSTRING("4D animation to analyze the velocity from")},
    {NULL}
};

struct velocity_priv {
    struct variable_info var;
    float vector[4];
    struct animation anim;
    struct animation anim_eval;
};

NGLI_STATIC_ASSERT(offsetof(struct velocity_priv, var) == 0, "variable_info is first");

static void mix_velocity_float(void *user_arg, void *dst,
                               const struct animkeyframe_opts *kf0,
                               const struct animkeyframe_opts *kf1,
                               double ratio)
{
    float *dstf = dst;
    dstf[0] = (float)((kf1->scalar - kf0->scalar) * ratio);
}

static void cpy_velocity_float(void *user_arg, void *dst,
                               const struct animkeyframe_opts *kf)
{
    float *dstf = dst;
    dstf[0] = 0;
}

#define DECLARE_VELOCITY_FUNCS(len)                                     \
static void mix_velocity_vec##len(void *user_arg, void *dst,            \
                                  const struct animkeyframe_opts *kf0,  \
                                  const struct animkeyframe_opts *kf1,  \
                                  double ratio)                         \
{                                                                       \
    float *dstf = dst;                                                  \
    ngli_vec##len##_sub(dstf, kf1->value, kf0->value);                  \
    ngli_vec##len##_norm(dstf, dstf);                                   \
    ngli_vec##len##_scale(dstf, dstf, (float)ratio);                    \
}                                                                       \
                                                                        \
static void cpy_velocity_vec##len(void *user_arg, void *dst,            \
                                  const struct animkeyframe_opts *kf)   \
{                                                                       \
    memset(dst, 0, len * sizeof(*kf->value));                           \
}                                                                       \

DECLARE_VELOCITY_FUNCS(2)
DECLARE_VELOCITY_FUNCS(3)
DECLARE_VELOCITY_FUNCS(4)

static ngli_animation_mix_func_type get_mix_func(uint32_t node_class)
{
    switch (node_class) {
    case NGL_NODE_VELOCITYFLOAT: return mix_velocity_float;
    case NGL_NODE_VELOCITYVEC2:  return mix_velocity_vec2;
    case NGL_NODE_VELOCITYVEC3:  return mix_velocity_vec3;
    case NGL_NODE_VELOCITYVEC4:  return mix_velocity_vec4;
    }
    return NULL;
}

static ngli_animation_cpy_func_type get_cpy_func(uint32_t node_class)
{
    switch (node_class) {
    case NGL_NODE_VELOCITYFLOAT: return cpy_velocity_float;
    case NGL_NODE_VELOCITYVEC2:  return cpy_velocity_vec2;
    case NGL_NODE_VELOCITYVEC3:  return cpy_velocity_vec3;
    case NGL_NODE_VELOCITYVEC4:  return cpy_velocity_vec4;
    }
    return NULL;
}

/* Used for standalone evaluation (outside a context) */
int ngli_velocity_evaluate(struct ngl_node *node, void *dst, double t)
{
    struct velocity_priv *s = node->priv_data;
    const struct velocity_opts *o = node->opts;

    /*
     * The following check is required because NGLI_PARAM_FLAG_NON_NULL is
     * checked at the node init, but we are in a pass-through mode here (no
     * context) so the node is not initialized.
     */
    if (!o->anim_node)
        return NGL_ERROR_INVALID_USAGE;

    const struct variable_opts *anim = o->anim_node->opts;
    if (!anim->nb_animkf)
        return NGL_ERROR_INVALID_ARG;

    if (!s->anim_eval.kfs) {
        int ret = ngli_animation_init(&s->anim_eval, NULL,
                                      anim->animkf, anim->nb_animkf,
                                      get_mix_func(node->cls->id),
                                      get_cpy_func(node->cls->id));
        if (ret < 0)
            return ret;
    }

    struct animkeyframe_priv *kf0 = anim->animkf[0]->priv_data;
    if (!kf0->derivative) {
        for (size_t i = 0; i < anim->nb_animkf; i++) {
            int ret = anim->animkf[i]->cls->init(anim->animkf[i]);
            if (ret < 0)
                return ret;
        }
    }

    return ngli_animation_derivate(&s->anim_eval, dst, t - anim->time_offset);
}

static int velocity_init(struct ngl_node *node)
{
    struct velocity_priv *s = node->priv_data;
    const struct velocity_opts *o = node->opts;
    const struct variable_opts *anim = o->anim_node->opts;
    s->var.dynamic = 1;
    return ngli_animation_init(&s->anim, NULL,
                               anim->animkf, anim->nb_animkf,
                               get_mix_func(node->cls->id),
                               get_cpy_func(node->cls->id));
}

static int velocity_update(struct ngl_node *node, double t)
{
    struct velocity_priv *s = node->priv_data;
    const struct velocity_opts *o = node->opts;
    const struct variable_opts *anim = o->anim_node->opts;
    return ngli_animation_derivate(&s->anim, s->var.data, t - anim->time_offset);
}

#define DEFINE_VELOCITY_CLASS(class_id, class_name, type, dtype, count)         \
static int velocity##type##_init(struct ngl_node *node)                         \
{                                                                               \
    struct velocity_priv *s = node->priv_data;                                  \
    s->var.data = s->vector;                                                    \
    s->var.data_size = count * sizeof(float);                                   \
    s->var.data_type = dtype;                                                   \
    return velocity_init(node);                                                 \
}                                                                               \
                                                                                \
const struct node_class ngli_velocity##type##_class = {                         \
    .id        = class_id,                                                      \
    .category  = NGLI_NODE_CATEGORY_VARIABLE,                                   \
    .name      = class_name,                                                    \
    .init      = velocity##type##_init,                                         \
    .update    = velocity_update,                                               \
    .opts_size = sizeof(struct velocity_opts),                                  \
    .priv_size = sizeof(struct velocity_priv),                                  \
    .params    = velocity##type##_params,                                       \
    .file      = __FILE__,                                                      \
};

DEFINE_VELOCITY_CLASS(NGL_NODE_VELOCITYFLOAT, "VelocityFloat", float, NGPU_TYPE_F32,   1)
DEFINE_VELOCITY_CLASS(NGL_NODE_VELOCITYVEC2,  "VelocityVec2",  vec2,  NGPU_TYPE_VEC2,  2)
DEFINE_VELOCITY_CLASS(NGL_NODE_VELOCITYVEC3,  "VelocityVec3",  vec3,  NGPU_TYPE_VEC3,  3)
DEFINE_VELOCITY_CLASS(NGL_NODE_VELOCITYVEC4,  "VelocityVec4",  vec4,  NGPU_TYPE_VEC4,  4)
