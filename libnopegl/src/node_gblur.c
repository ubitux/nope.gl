/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
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

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "gpu_block.h"
#include "gpu_ctx.h"
#include "graphics_state.h"
#include "internal.h"
#include "log.h"
#include "math_utils.h"
#include "node_texture.h"
#include "node_uniform.h"
#include "nopegl.h"
#include "pgcraft.h"
#include "pipeline_compat.h"
#include "rendertarget.h"
#include "rtt.h"
#include "topology.h"
#include "utils.h"

/* GLSL shaders */
#include "blur_gaussian_vert.h"
#include "blur_gaussian_frag.h"

#define _CONSTANT_TO_STR(v) #v
#define CONSTANT_TO_STR(v) _CONSTANT_TO_STR(v)

#define MAX_KERNEL_SIZE 127
NGLI_STATIC_ASSERT(kernel_size_is_odd, MAX_KERNEL_SIZE & 0x1);

#define MAX_RADIUS_SIZE 126
NGLI_STATIC_ASSERT(radius_size, MAX_RADIUS_SIZE == (MAX_KERNEL_SIZE - 1));

struct direction_block {
    float direction[2];
};

struct kernel_block {
    float weights[2 * MAX_KERNEL_SIZE];
    int32_t nb_weights;
};

struct gblur_opts {
    struct ngl_node *source;
    struct ngl_node *destination;
    struct ngl_node *bluriness_node;
    float bluriness;
};

struct gblur_priv {
    int32_t width;
    int32_t height;
    float bluriness;

    /* Source image */
    struct image *image;
    size_t image_rev;

    /* Render the horizontal pass to a temporary destination */
    struct rendertarget_layout tmp_layout;
    struct rtt_ctx *tmp;

    /* Render the vertical pass to the destination */
    int dst_is_resizeable;
    struct rendertarget_layout dst_layout;
    struct rtt_ctx *dst_rtt_ctx;

    struct gpu_block direction;
    struct gpu_block kernel;
    struct pgcraft *crafter;
    struct pipeline_compat *pl_blur_h;
    struct pipeline_compat *pl_blur_v;
};

#define OFFSET(x) offsetof(struct gblur_opts, x)
static const struct node_param gblur_params[] = {
    {"source",            NGLI_PARAM_TYPE_NODE, OFFSET(source),
                          .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                          .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                          .desc=NGLI_DOCSTRING("source to use for the blur")},
    {"destination",       NGLI_PARAM_TYPE_NODE, OFFSET(destination),
                          .node_types=(const uint32_t[]){NGL_NODE_TEXTURE2D, NGLI_NODE_NONE},
                          .flags=NGLI_PARAM_FLAG_NON_NULL | NGLI_PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                          .desc=NGLI_DOCSTRING("destination to use for the blur")},
    {"bluriness",         NGLI_PARAM_TYPE_F32, OFFSET(bluriness_node), {.f32=0.03f},
                          .flags=NGLI_PARAM_FLAG_ALLOW_NODE,
                          .desc=NGLI_DOCSTRING("amount of bluriness in the range [0,1] "
                                               "where 1 is equivalent of a blur radius of " CONSTANT_TO_STR(MAX_RADIUS_SIZE) "px")},
    {NULL}
};

#define O(i) (2 * (i))
#define W(i) (2 * (i) + 1)

static int update_kernel(struct ngl_node *node)
{
    struct gblur_priv *s = node->priv_data;
    struct gblur_opts *o = node->opts;

    const float bluriness = *(float *)ngli_node_get_data_ptr(o->bluriness_node, &o->bluriness);
    if (bluriness < 0.0)
        return NGL_ERROR_INVALID_ARG;

    if (s->bluriness == bluriness)
        return 0;

    s->bluriness = bluriness;

    const float radius_f = NGLI_CLAMP(bluriness, 0.f, 1.f) * (float)MAX_RADIUS_SIZE;
    const int32_t radius_i = (int32_t)ceil(radius_f);
    const int32_t radius = NGLI_MIN(radius_i, MAX_RADIUS_SIZE);

    /*
     * Compute sigma for a given precision (1e-3f should be fine for up to
     * 10-bit image formats).
     * See:
     * - https://en.wikipedia.org/wiki/Talk%3AGaussian_blur#Radius_again
     * - https://en.wikipedia.org/wiki/68%E2%80%9395%E2%80%9399.7_rule
     */
    const float sigma = (radius_f + 1.f) / sqrtf(-2.f * logf(1e-3f));

    /*
     * Compute the weights for the interval [-radius, radius].
     *
     * Instead of evaluating the gaussian function to compute the weights, we
     * use an approximation of its integral based on the error function. This
     * avoids errors and undersampling for small sigma (< 0.8).
     * See:
     * - https://en.wikipedia.org/wiki/Error_function#Applications
     * - https://bartwronski.com/2021/10/31/practical-gaussian-filter-binomial-filter-and-small-sigma-gaussians
     */
    float weights[2 * MAX_KERNEL_SIZE];
    size_t nb_weights = 0;

    float sum = 0.0;
    const float sig = sigma * sqrtf(2.f);
    for (int i = -radius; i <= radius; i++) {
        const float p1 = erff(((float)i - 0.5f) / sig);
        const float p2 = erff(((float)i + 0.5f) / sig);
        const float w = (p2 - p1) / 2.f;
        weights[nb_weights++] = w;
        sum += w;
    }
    for (size_t i = 0; i < nb_weights; i++)
        weights[i] /= (float)sum;

    /*
     * Compute offsets and weights to take advantage of hw filtering to reduce
     * the number of texture fetches from (2*radius + 1) to (radius + 1). The
     * resulting offsets and weights are stored in a vec2.
     */
    struct kernel_block kernel = {0};
    for (int i = -radius; i < radius; i += 2) {
        const float w0 = weights[i + radius + 0];
        const float w1 = weights[i + radius + 1];
        const float w = w0 + w1;
        kernel.weights[O(kernel.nb_weights)] = w > 0 ? (float)i + w1 / w : (float)i;
        kernel.weights[W(kernel.nb_weights)] = w;
        kernel.nb_weights++;
    }
    kernel.weights[O(kernel.nb_weights)] = (float)radius;
    kernel.weights[W(kernel.nb_weights)] = weights[radius + radius];
    kernel.nb_weights++;

    int ret = ngli_gpu_block_update(&s->kernel, 0, &kernel);
    if (ret < 0)
        return ret;

    return 0;
}

static int setup_pipeline(struct pgcraft *crafter, struct pipeline_compat *pipeline, const struct rendertarget_layout *layout)
{
    const struct pipeline_compat_params params = {
        .type         = NGLI_PIPELINE_TYPE_GRAPHICS,
        .graphics     = {
            .topology = NGLI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .state    = NGLI_GRAPHICS_STATE_DEFAULTS,
            .rt_layout    = *layout,
            .vertex_state = ngli_pgcraft_get_vertex_state(crafter),
        },
        .program      = ngli_pgcraft_get_program(crafter),
        .layout       = ngli_pgcraft_get_pipeline_layout(crafter),
        .resources    = ngli_pgcraft_get_pipeline_resources(crafter),
        .compat_info  = ngli_pgcraft_get_compat_info(crafter),
    };

    int ret = ngli_pipeline_compat_init(pipeline, &params);
    if (ret < 0)
        return ret;

    const int32_t index = ngli_pgcraft_get_uniform_index(crafter, "tex_coord_matrix", NGLI_PROGRAM_SHADER_VERT);
    ngli_assert(index >= 0);

    NGLI_ALIGNED_MAT(tmp_coord_matrix) = NGLI_MAT4_IDENTITY;
    ngli_pipeline_compat_update_uniform(pipeline, index, tmp_coord_matrix);

    return 0;
}

static int gblur_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gblur_priv *s = node->priv_data;
    struct gblur_opts *o = node->opts;

    struct texture_info *src_info = o->source->priv_data;
    s->image = &src_info->image;
    s->image_rev = SIZE_MAX;

    /* Disable direct rendering */
    src_info->supported_image_layouts = 1U << NGLI_IMAGE_LAYOUT_DEFAULT;

    /* Override texture params */
    src_info->params.min_filter = NGLI_FILTER_LINEAR;
    src_info->params.mag_filter = NGLI_FILTER_LINEAR;
    src_info->params.wrap_s     = NGLI_WRAP_MIRRORED_REPEAT,
    src_info->params.wrap_t     = NGLI_WRAP_MIRRORED_REPEAT,

    s->tmp_layout.colors[s->tmp_layout.nb_colors].format = src_info->params.format;
    s->tmp_layout.nb_colors++;

    struct texture_info *dst_info = o->destination->priv_data;
    dst_info->params.usage |= NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

    s->dst_is_resizeable = (dst_info->params.width == 0 && dst_info->params.height == 0);
    s->dst_layout.colors[0].format = dst_info->params.format;
    s->dst_layout.nb_colors = 1;

    const struct gpu_block_field direction_fields[] = {
        NGLI_GPU_BLOCK_FIELD(struct direction_block, direction, NGLI_TYPE_VEC2, 0),
    };
    const struct gpu_block_params direction_params = {
        .count     = 2,
        .fields    = direction_fields,
        .nb_fields = NGLI_ARRAY_NB(direction_fields),
    };
    int ret = ngli_gpu_block_init(gpu_ctx, &s->direction, &direction_params);
    if (ret < 0)
        return ret;
    ngli_gpu_block_update(&s->direction, 0, &(struct direction_block){.direction = {1.f, 0.f}});
    ngli_gpu_block_update(&s->direction, 1, &(struct direction_block){.direction = {0.f, 1.f}});

    const struct gpu_block_field kernel_fields[] = {
        NGLI_GPU_BLOCK_FIELD(struct kernel_block, weights,    NGLI_TYPE_VEC2, MAX_KERNEL_SIZE),
        NGLI_GPU_BLOCK_FIELD(struct kernel_block, nb_weights, NGLI_TYPE_I32,  0),
    };
    const struct gpu_block_params kernel_params = {
        .fields    = kernel_fields,
        .nb_fields = NGLI_ARRAY_NB(kernel_fields),
    };
    ngli_gpu_block_init(gpu_ctx, &s->kernel, &kernel_params);

    const struct pgcraft_iovar vert_out_vars[] = {
        {.name = "tex_coord", .type = NGLI_TYPE_VEC2},
    };

    const struct pgcraft_texture textures[] = {
        {
            .name      = "tex",
            .type      = NGLI_PGCRAFT_SHADER_TEX_TYPE_2D,
            .precision = NGLI_PRECISION_HIGH,
            .stage     = NGLI_PROGRAM_SHADER_FRAG,
        },
    };

    const struct pgcraft_block crafter_blocks[] = {
        {
            .name          = "direction",
            .type          = NGLI_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .stage         = NGLI_PROGRAM_SHADER_FRAG,
            .block         = &s->direction.block,
            .buffer        = {
                .buffer = s->direction.buffer,
                .size   = s->direction.block_size,
            },
        }, {
            .name          = "kernel",
            .type          = NGLI_TYPE_UNIFORM_BUFFER,
            .stage         = NGLI_PROGRAM_SHADER_FRAG,
            .block         = &s->kernel.block,
            .buffer        = {
                .buffer = s->kernel.buffer,
                .size   = s->kernel.block_size,
            },
        },
    };

    const struct pgcraft_params crafter_params = {
        .program_label    = "nopegl/gaussian-blur",
        .vert_base        = blur_gaussian_vert,
        .frag_base        = blur_gaussian_frag,
        .textures         = textures,
        .nb_textures      = NGLI_ARRAY_NB(textures),
        .blocks           = crafter_blocks,
        .nb_blocks        = NGLI_ARRAY_NB(crafter_blocks),
        .vert_out_vars    = vert_out_vars,
        .nb_vert_out_vars = NGLI_ARRAY_NB(vert_out_vars),
    };
    s->crafter = ngli_pgcraft_create(ctx);
    if (!s->crafter)
        return NGL_ERROR_MEMORY;

    ret = ngli_pgcraft_craft(s->crafter, &crafter_params);
    if (ret < 0)
        return ret;

    s->pl_blur_h = ngli_pipeline_compat_create(gpu_ctx);
    s->pl_blur_v = ngli_pipeline_compat_create(gpu_ctx);
    if (!s->pl_blur_h || !s->pl_blur_v)
        return NGL_ERROR_MEMORY;

    if ((ret = setup_pipeline(s->crafter, s->pl_blur_h, &s->tmp_layout)) < 0 ||
        (ret = setup_pipeline(s->crafter, s->pl_blur_v, &s->dst_layout)) < 0)
        return ret;

    return 0;
}

static int resize(struct ngl_node *node)
{
    int ret = 0;
    struct ngl_ctx *ctx = node->ctx;
    struct gblur_priv *s = node->priv_data;
    struct gblur_opts *o = node->opts;

    ngli_node_draw(o->source);

    struct texture_info *src_info = o->source->priv_data;
    const int32_t width = src_info->image.params.width;
    const int32_t height = src_info->image.params.height;
    if (s->width == width && s->height == height)
        return 0;

    /* Assert that the source texture format does not change */
    ngli_assert(src_info->params.format == s->tmp_layout.colors[0].format);

    /* Assert that the destination texture format does not change */
    struct texture_info *dst_info = o->destination->priv_data;
    ngli_assert(dst_info->params.format == s->dst_layout.colors[0].format);

    struct rtt_ctx *tmp = NULL;

    struct texture *dst = NULL;
    struct rtt_ctx *dst_rtt_ctx = NULL;

    const struct texture_params texture_params = {
        .type          = NGLI_TEXTURE_TYPE_2D,
        .format        = src_info->params.format,
        .width         = width,
        .height        = height,
        .min_filter    = NGLI_FILTER_LINEAR,
        .mag_filter    = NGLI_FILTER_LINEAR,
        .wrap_s        = NGLI_WRAP_MIRRORED_REPEAT,
        .wrap_t        = NGLI_WRAP_MIRRORED_REPEAT,
        .usage         = NGLI_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                         NGLI_TEXTURE_USAGE_SAMPLED_BIT,
    };

    tmp = ngli_rtt_create(ctx);
    if (!tmp) {
        ret = NGL_ERROR_MEMORY;
        goto fail;
    }

    ret = ngli_rtt_from_texture_params(tmp, &texture_params);
    if (ret < 0)
        goto fail;

    dst = dst_info->texture;
    if (s->dst_is_resizeable) {
        dst = ngli_texture_create(ctx->gpu_ctx);
        if (!dst) {
            ret = NGL_ERROR_MEMORY;
            goto fail;
        }

        struct texture_params params = dst_info->params;
        params.width = width;
        params.height = height;
        ret = ngli_texture_init(dst, &params);
        if (ret < 0)
            goto fail;
    }

    ngli_rtt_freep(&s->tmp);
    s->tmp = tmp;

    if (s->dst_is_resizeable) {
        ngli_texture_freep(&dst_info->texture);
        dst_info->texture = dst;
        dst_info->image.params.width = dst->params.width;
        dst_info->image.params.height = dst->params.height;
        dst_info->image.planes[0] = dst;
        dst_info->image.rev = dst_info->image_rev++;
    }

    dst_rtt_ctx = ngli_rtt_create(ctx);
    if (!dst_rtt_ctx) {
        ret = NGL_ERROR_MEMORY;
        goto fail;
    }

    const struct rtt_params rtt_params = (struct rtt_params) {
        .width  = dst->params.width,
        .height = dst->params.height,
        .nb_colors = 1,
        .colors[0] = {
            .attachment = dst,
            .load_op = NGLI_LOAD_OP_CLEAR,
            .store_op = NGLI_STORE_OP_STORE,
        },
    };

    ret = ngli_rtt_init(dst_rtt_ctx, &rtt_params);
    if (ret < 0)
        goto fail;

    ngli_rtt_freep(&s->dst_rtt_ctx);
    s->dst_rtt_ctx = dst_rtt_ctx;

    s->width = width;
    s->height = height;

    /* Trigger a kernel update on resolution change */
    s->bluriness = -1.f;

    return 0;

fail:
    ngli_rtt_freep(&tmp);

    ngli_rtt_freep(&dst_rtt_ctx);
    if (s->dst_is_resizeable)
        ngli_texture_freep(&dst);

    LOG(ERROR, "failed to resize blur: %dx%d", width, height);
    return ret;
}

static void gblur_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    struct gblur_priv *s = node->priv_data;

    int ret = resize(node);
    if (ret < 0)
        return;

    ret = update_kernel(node);
    if (ret < 0)
        return;

    ngli_rtt_begin(s->tmp);
    ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
    ctx->render_pass_started = 1;
    uint32_t offset = 0;
    ngli_pipeline_compat_update_dynamic_offsets(s->pl_blur_h, &offset, 1);
    if (s->image_rev != s->image->rev) {
        ngli_pipeline_compat_update_image(s->pl_blur_h, 0, s->image);
        s->image_rev = s->image->rev;
    }
    ngli_pipeline_compat_draw(s->pl_blur_h, 3, 1);
    ngli_rtt_end(s->tmp);

    ngli_rtt_begin(s->dst_rtt_ctx);
    ngli_gpu_ctx_begin_render_pass(gpu_ctx, ctx->current_rendertarget);
    ctx->render_pass_started = 1;
    offset = (uint32_t)s->direction.block_size;
    ngli_pipeline_compat_update_dynamic_offsets(s->pl_blur_v, &offset, 1);
    ngli_pipeline_compat_update_texture(s->pl_blur_v, 0, ngli_rtt_get_texture(s->tmp, 0));
    ngli_pipeline_compat_draw(s->pl_blur_v, 3, 1);
    ngli_rtt_end(s->dst_rtt_ctx);
}

static void gblur_release(struct ngl_node *node)
{
    struct gblur_priv *s = node->priv_data;

    ngli_rtt_freep(&s->tmp);
    ngli_rtt_freep(&s->dst_rtt_ctx);
}

static void gblur_uninit(struct ngl_node *node)
{
    struct gblur_priv *s = node->priv_data;

    ngli_gpu_block_reset(&s->direction);
    ngli_gpu_block_reset(&s->kernel);
    ngli_pipeline_compat_freep(&s->pl_blur_h);
    ngli_pipeline_compat_freep(&s->pl_blur_v);
    ngli_pgcraft_freep(&s->crafter);
}

const struct node_class ngli_gblur_class = {
    .id        = NGL_NODE_GAUSSIANBLUR,
    .name      = "GaussianBlur",
    .init      = gblur_init,
    .prepare   = ngli_node_prepare_children,
    .update    = ngli_node_update_children,
    .draw      = gblur_draw,
    .release   = gblur_release,
    .uninit    = gblur_uninit,
    .opts_size = sizeof(struct gblur_opts),
    .priv_size = sizeof(struct gblur_priv),
    .params    = gblur_params,
    .file      = __FILE__,
};
