/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2022 GoPro Inc.
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

#include <string.h>

#include "bindgroup.h"
#include "darray.h"
#include "gpu_ctx.h"
#include "gpu_limits.h"
#include "math_utils.h"
#include "memory.h"
#include "nopegl.h"
#include "pipeline_compat.h"
#include "utils.h"

#define NB_BINDGROUPS 16

struct pipeline_compat {
    struct gpu_ctx *gpu_ctx;
    int type; // any of NGLI_PIPELINE_TYPE_*
    struct pipeline_graphics graphics;
    const struct program *program;
    struct pipeline *pipeline;
    struct bindgroup_layout_params bindgroup_layout_params;
    struct bindgroup_layout *bindgroup_layout;
    struct darray bindgroups;
    struct bindgroup *cur_bindgroup;
    size_t cur_bindgroup_index;
    const struct buffer **vertex_buffers;
    size_t nb_vertex_buffers;
    struct texture_binding *textures;
    size_t nb_textures;
    struct buffer_binding *buffers;
    size_t nb_buffers;
    uint32_t dynamic_offsets[NGLI_MAX_DYNAMIC_OFFSETS];
    size_t nb_dynamic_offsets;
    int updated;
    int need_pipeline_recreation;
    const struct pgcraft_compat_info *compat_info;
    struct buffer *ubuffers[NGLI_PROGRAM_SHADER_NB];
    uint8_t *mapped_datas[NGLI_PROGRAM_SHADER_NB];
};

static int map_buffer(struct pipeline_compat *s, int stage)
{
    if (s->mapped_datas[stage])
        return 0;

    struct buffer *buffer = s->ubuffers[stage];
    return ngli_buffer_map(buffer, 0, buffer->size, (void **)&s->mapped_datas[stage]);
}

static void unmap_buffers(struct pipeline_compat *s)
{
    for (size_t i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
        if (s->mapped_datas[i]) {
            ngli_buffer_unmap(s->ubuffers[i]);
            s->mapped_datas[i] = NULL;
        }
    }
}

struct pipeline_compat *ngli_pipeline_compat_create(struct gpu_ctx *gpu_ctx)
{
    struct pipeline_compat *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->gpu_ctx = gpu_ctx;
    return s;
}

static int init_blocks_buffers(struct pipeline_compat *s, const struct pipeline_compat_params *params)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    for (size_t i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
        const size_t block_size = ngli_block_get_size(&s->compat_info->ublocks[i], 0);
        if (!block_size)
            continue;

        struct buffer *buffer = ngli_buffer_create(gpu_ctx);
        if (!buffer)
            return NGL_ERROR_MEMORY;
        s->ubuffers[i] = buffer;

        int ret = ngli_buffer_init(buffer,
                                   block_size,
                                   NGLI_BUFFER_USAGE_DYNAMIC_BIT |
                                   NGLI_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                   NGLI_BUFFER_USAGE_MAP_WRITE);
        if (ret < 0)
            return ret;

        if (gpu_ctx->features & NGLI_FEATURE_BUFFER_MAP_PERSISTENT) {
            ret = ngli_buffer_map(buffer, 0, buffer->size, (void **)&s->mapped_datas[i]);
            if (ret < 0)
                return ret;
        }

        ngli_pipeline_compat_update_buffer(s, s->compat_info->uindices[i], buffer, 0, buffer->size);
    }

    return 0;
}

static void free_bindgroup(void *user_arg, void *data)
{
    struct bindgroup **bindgroup = data;
    ngli_bindgroup_freep(bindgroup);
}

static int grow_bindgroup_array(struct pipeline_compat *s)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    size_t count = ngli_darray_count(&s->bindgroups);
    if (count == 0) {
        ngli_darray_init(&s->bindgroups, sizeof(struct bindgroup *), 0);
        ngli_darray_set_free_func(&s->bindgroups, free_bindgroup, NULL);
        count = NB_BINDGROUPS;
    }

    for (size_t i = 0; i < count; i++) {
        struct bindgroup *bindgroup = ngli_bindgroup_create(gpu_ctx);
        if (!bindgroup)
            return NGL_ERROR_MEMORY;

        struct bindgroup_params params = {
            .layout      = s->bindgroup_layout,
            .textures    = s->textures,
            .nb_textures = s->nb_textures,
            .buffers     = s->buffers,
            .nb_buffers  = s->nb_buffers,
        };

        int ret = ngli_bindgroup_init(bindgroup, &params);
        if (ret < 0) {
            ngli_bindgroup_freep(&bindgroup);
            return ret;
        }

        if (!ngli_darray_push(&s->bindgroups, &bindgroup)) {
            ngli_bindgroup_freep(&bindgroup);
            return NGL_ERROR_MEMORY;
        }
    }

    return 0;
}

static int create_pipeline(struct pipeline_compat *s)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    s->bindgroup_layout = ngli_bindgroup_layout_create(gpu_ctx);
    if (!s->bindgroup_layout)
        return NGL_ERROR_MEMORY;

    int ret = ngli_bindgroup_layout_init(s->bindgroup_layout, &s->bindgroup_layout_params);
    if (ret < 0)
        return ret;

    s->pipeline = ngli_pipeline_create(gpu_ctx);
    if (!s->pipeline)
        return NGL_ERROR_MEMORY;

    const struct pipeline_params pipeline_params = {
        .type     = s->type,
        .graphics = s->graphics,
        .program  = s->program,
        .layout   = {
            .bindgroup_layout = s->bindgroup_layout,
        }
    };

    ret = ngli_pipeline_init(s->pipeline, &pipeline_params);
    if (ret < 0)
        return ret;

    ret = grow_bindgroup_array(s);
    if (ret < 0)
        return ret;

    s->cur_bindgroup = *(struct bindgroup **)ngli_darray_get(&s->bindgroups, 0);
    s->cur_bindgroup_index = 0;

    return 0;
}

static void reset_pipeline(struct pipeline_compat *s)
{
    ngli_pipeline_freep(&s->pipeline);
    ngli_darray_clear(&s->bindgroups);
    s->cur_bindgroup = NULL;
    s->cur_bindgroup_index = 0;
    ngli_bindgroup_layout_freep(&s->bindgroup_layout);
}

int ngli_pipeline_compat_init(struct pipeline_compat *s, const struct pipeline_compat_params *params)
{
    s->type = params->type;

    int ret = ngli_pipeline_graphics_copy(&s->graphics, &params->graphics);
    if (ret < 0)
        return ret;

    s->program = params->program;

    NGLI_ARRAY_MEMDUP(&s->bindgroup_layout_params, &params->layout, textures);
    NGLI_ARRAY_MEMDUP(&s->bindgroup_layout_params, &params->layout, buffers);

    const struct pipeline_compat_resources *resources = &params->resources;
    NGLI_ARRAY_MEMDUP(s, resources, vertex_buffers);
    NGLI_ARRAY_MEMDUP(s, resources, buffers);
    NGLI_ARRAY_MEMDUP(s, resources, textures);

    s->compat_info = params->compat_info;
    ret = init_blocks_buffers(s, params);
    if (ret < 0)
        return ret;

    ret = create_pipeline(s);
    if (ret < 0)
        return ret;

    return 0;
}

int ngli_pipeline_compat_update_vertex_buffer(struct pipeline_compat *s, int32_t index, const struct buffer *buffer)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index >= 0 && index < s->nb_vertex_buffers);
    s->vertex_buffers[index] = buffer;
    return 0;
}

int ngli_pipeline_compat_update_uniform_count(struct pipeline_compat *s, int32_t index, const void *value, size_t count)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    const int32_t stage = index >> 16;
    const int32_t field_index = index & 0xffff;
    const struct block *block = &s->compat_info->ublocks[stage];
    const struct block_field *fields = ngli_darray_data(&block->fields);
    const struct block_field *field = &fields[field_index];
    if (value) {
        if (!(gpu_ctx->features & NGLI_FEATURE_BUFFER_MAP_PERSISTENT)) {
            int ret = map_buffer(s, stage);
            if (ret < 0)
                return ret;
        }
        uint8_t *dst = s->mapped_datas[stage] + field->offset;
        ngli_block_field_copy_count(field, dst, value, count);
    }

    return 0;
}

int ngli_pipeline_compat_update_uniform(struct pipeline_compat *s, int32_t index, const void *value)
{
    return ngli_pipeline_compat_update_uniform_count(s, index, value, 0);
}

static int update_texture(struct pipeline_compat *s, int32_t index, const struct texture_binding *binding)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index >= 0 && index < s->nb_textures);

    if (s->textures[index].immutable_sampler != binding->immutable_sampler) {
        struct bindgroup_layout_entry *entry = &s->bindgroup_layout_params.textures[index];
        entry->immutable_sampler = binding->immutable_sampler;
        s->need_pipeline_recreation = 1;
    }

    s->textures[index] = *binding;
    s->updated = 1;

    return 0;
}

int ngli_pipeline_compat_update_texture(struct pipeline_compat *s, int32_t index, const struct texture *texture)
{
    const struct texture_binding binding = {.texture = texture};
    return update_texture(s, index, &binding);
}

int ngli_pipeline_compat_update_dynamic_offsets(struct pipeline_compat *s, const uint32_t *offsets, size_t nb_offsets)
{
    ngli_assert(s->bindgroup_layout->nb_dynamic_offsets == nb_offsets);
    memcpy(s->dynamic_offsets, offsets, nb_offsets * sizeof(*s->dynamic_offsets));
    s->nb_dynamic_offsets = nb_offsets;
    return 0;
}

void ngli_pipeline_compat_apply_reframing_matrix(struct pipeline_compat *s, int32_t index, const struct image *image, const float *reframing)
{
    if (index == -1)
        return;

    ngli_assert(index >= 0 && index < s->compat_info->nb_texture_infos);
    const struct pgcraft_texture_info *info = &s->compat_info->texture_infos[index];
    const struct pgcraft_texture_info_field *fields = info->fields;

    if (fields[NGLI_INFO_FIELD_COORDINATE_MATRIX].index == -1)
        return;

    /* Scale up from normalized [0,1] UV to centered [-1,1], swapping y-axis */
    static const NGLI_ALIGNED_MAT(remap_uv_to_centered) = {
        2.f,  0.f, 0.f, 0.f,
        0.f, -2.f, 0.f, 0.f,
        0.f,  0.f, 1.f, 0.f,
       -1.f,  1.f, 0.f, 1.f,
    };

    /* Scale down from centered [-1,1] to normalized [0,1] UV, swapping y-axis */
    static const NGLI_ALIGNED_MAT(remap_centered_to_uv) = {
        .5f,  0.f, 0.f, 0.f,
        0.f, -.5f, 0.f, 0.f,
        0.f,  0.f, 1.f, 0.f,
        .5f,  .5f, 0.f, 1.f,
    };

    NGLI_ALIGNED_MAT(inverse_reframing);
    ngli_mat4_inverse(inverse_reframing, reframing);

    NGLI_ALIGNED_MAT(matrix);
    ngli_mat4_mul(matrix, remap_uv_to_centered, image->coordinates_matrix);
    ngli_mat4_mul(matrix, inverse_reframing, matrix);
    ngli_mat4_mul(matrix, remap_centered_to_uv, matrix);

    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_COORDINATE_MATRIX].index, matrix);
}

void ngli_pipeline_compat_update_image(struct pipeline_compat *s, int32_t index, const struct image *image)
{
    if (index == -1)
        return;

    ngli_assert(index >= 0 && index < s->compat_info->nb_texture_infos);
    const struct pgcraft_texture_info *info = &s->compat_info->texture_infos[index];
    const struct pgcraft_texture_info_field *fields = info->fields;

    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_COORDINATE_MATRIX].index, image->coordinates_matrix);
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_COLOR_MATRIX].index, image->color_matrix);
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_TIMESTAMP].index, &image->ts);

    if (image->params.layout) {
        const float dimensions[] = {(float)image->params.width, (float)image->params.height, (float)image->params.depth};
        ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_DIMENSIONS].index, dimensions);
    }

    struct texture_binding bindings[NGLI_INFO_FIELD_NB] = {0};
    switch (image->params.layout) {
    case NGLI_IMAGE_LAYOUT_DEFAULT:
        bindings[NGLI_INFO_FIELD_SAMPLER_0].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_0].immutable_sampler = image->samplers[0];
        break;
    case NGLI_IMAGE_LAYOUT_NV12:
        bindings[NGLI_INFO_FIELD_SAMPLER_0].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_0].immutable_sampler = image->samplers[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_1].texture = image->planes[1];
        bindings[NGLI_INFO_FIELD_SAMPLER_1].immutable_sampler = image->samplers[1];
        break;
    case NGLI_IMAGE_LAYOUT_NV12_RECTANGLE:
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_0].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_0].immutable_sampler = image->samplers[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_1].texture = image->planes[1];
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_1].immutable_sampler = image->samplers[1];
        break;
    case NGLI_IMAGE_LAYOUT_MEDIACODEC:
        bindings[NGLI_INFO_FIELD_SAMPLER_OES].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_OES].immutable_sampler = image->samplers[0];
        break;
    case NGLI_IMAGE_LAYOUT_YUV:
        bindings[NGLI_INFO_FIELD_SAMPLER_0].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_0].immutable_sampler = image->samplers[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_1].texture = image->planes[1];
        bindings[NGLI_INFO_FIELD_SAMPLER_1].immutable_sampler = image->samplers[1];
        bindings[NGLI_INFO_FIELD_SAMPLER_2].texture = image->planes[2];
        bindings[NGLI_INFO_FIELD_SAMPLER_2].immutable_sampler = image->samplers[2];
        break;
    case NGLI_IMAGE_LAYOUT_RECTANGLE:
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_0].texture = image->planes[0];
        bindings[NGLI_INFO_FIELD_SAMPLER_RECT_0].immutable_sampler = image->samplers[0];
        break;
    default:
        break;
    }

    static const int samplers[] = {
        NGLI_INFO_FIELD_SAMPLER_0,
        NGLI_INFO_FIELD_SAMPLER_1,
        NGLI_INFO_FIELD_SAMPLER_2,
        NGLI_INFO_FIELD_SAMPLER_OES,
        NGLI_INFO_FIELD_SAMPLER_RECT_0,
        NGLI_INFO_FIELD_SAMPLER_RECT_1,
    };

    int ret = 1;
    for (size_t i = 0; i < NGLI_ARRAY_NB(samplers); i++) {
        const int sampler = samplers[i];
        const int32_t binding_index = fields[sampler].index;
        const struct texture_binding *binding = &bindings[sampler];
        ret &= update_texture(s, binding_index, binding);
    };

    const int layout = ret < 0 ? NGLI_IMAGE_LAYOUT_NONE : image->params.layout;
    ngli_pipeline_compat_update_uniform(s, fields[NGLI_INFO_FIELD_SAMPLING_MODE].index, &layout);
}

int ngli_pipeline_compat_update_buffer(struct pipeline_compat *s, int32_t index, const struct buffer *buffer, size_t offset, size_t size)
{
    if (index == -1)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index >= 0 && index < s->nb_buffers);
    s->buffers[index] = (struct buffer_binding) {
        .buffer = buffer,
        .offset = offset,
        .size   = size ? size : buffer->size,
    };
    s->updated = 1;
    return 0;
}

static int select_next_available_bindgroup(struct pipeline_compat *s)
{
    /* If current bindgroup is not in use, select it */
    if (s->cur_bindgroup->rc.count == 1)
        return 0;

    /* Otherwhise, check if next bindgroup is available  */
    size_t bindgroup_index = (s->cur_bindgroup_index + 1) % ngli_darray_count(&s->bindgroups);
    struct bindgroup *bindgroup = *(struct bindgroup **)ngli_darray_get(&s->bindgroups, bindgroup_index);
    if (bindgroup->rc.count == 1) {
        s->cur_bindgroup = bindgroup;
        s->cur_bindgroup_index = bindgroup_index;
        return 0;
    }

    /*
     * If it is not, save next newly-allocated bind group index and increase
     * our bindgroup pool size
     */
    bindgroup_index = ngli_darray_count(&s->bindgroups);

    int ret = grow_bindgroup_array(s);
    if (ret < 0)
        return ret;

    /* Select bindgroup and assert that it is not in use */
    s->cur_bindgroup = *(struct bindgroup **)ngli_darray_get(&s->bindgroups, bindgroup_index);
    s->cur_bindgroup_index = bindgroup_index;
    ngli_assert(s->cur_bindgroup->rc.count == 1);

    return 0;
}

static int prepare_bindgroup(struct pipeline_compat *s)
{
    if (!s->updated)
        return 0;

    s->updated = 0;

    if (s->need_pipeline_recreation) {
        s->need_pipeline_recreation = 0;
        reset_pipeline(s);
        int ret = create_pipeline(s);
        if (ret < 0)
            return ret;
    }

    int ret = select_next_available_bindgroup(s);
    if (ret < 0)
        return ret;

    for (size_t i = 0; i < s->nb_textures; i++) {
        ret = ngli_bindgroup_update_texture(s->cur_bindgroup, (int32_t)i, &s->textures[i]);
        if (ret < 0)
            return ret;
    }

    for (size_t i = 0; i < s->nb_buffers; i++) {
        ret = ngli_bindgroup_update_buffer(s->cur_bindgroup, (int32_t)i, &s->buffers[i]);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int prepare_pipeline(struct pipeline_compat *s)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    if (!(gpu_ctx->features & NGLI_FEATURE_BUFFER_MAP_PERSISTENT))
       unmap_buffers(s);

    int ret = prepare_bindgroup(s);
    if (ret < 0)
        return ret;

    return 0;
}

void ngli_pipeline_compat_draw(struct pipeline_compat *s, int nb_vertices, int nb_instances)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    int ret = prepare_pipeline(s);
    if (ret < 0)
        return;

    ngli_gpu_ctx_set_pipeline(gpu_ctx, s->pipeline);
    for (size_t i = 0; i < s->nb_vertex_buffers; i++)
        ngli_gpu_ctx_set_vertex_buffer(gpu_ctx, (uint32_t)i, s->vertex_buffers[i]);
    ngli_gpu_ctx_set_bindgroup(gpu_ctx, s->cur_bindgroup, s->dynamic_offsets, s->nb_dynamic_offsets);
    ngli_gpu_ctx_draw(gpu_ctx, nb_vertices, nb_instances);
}

void ngli_pipeline_compat_draw_indexed(struct pipeline_compat *s, const struct buffer *indices, int indices_format, int nb_indices, int nb_instances)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    int ret = prepare_pipeline(s);
    if (ret < 0)
        return;

    ngli_gpu_ctx_set_pipeline(gpu_ctx, s->pipeline);
    for (size_t i = 0; i < s->nb_vertex_buffers; i++)
        ngli_gpu_ctx_set_vertex_buffer(gpu_ctx, (uint32_t)i, s->vertex_buffers[i]);
    ngli_gpu_ctx_set_index_buffer(gpu_ctx, indices, indices_format);
    ngli_gpu_ctx_set_bindgroup(gpu_ctx, s->cur_bindgroup, s->dynamic_offsets, s->nb_dynamic_offsets);
    ngli_gpu_ctx_draw_indexed(gpu_ctx, nb_indices, nb_instances);
}

void ngli_pipeline_compat_dispatch(struct pipeline_compat *s, uint32_t nb_group_x, uint32_t nb_group_y, uint32_t nb_group_z)
{
    struct gpu_ctx *gpu_ctx = s->gpu_ctx;

    int ret = prepare_pipeline(s);
    if (ret < 0)
        return;

    ngli_gpu_ctx_set_pipeline(gpu_ctx, s->pipeline);
    ngli_gpu_ctx_set_bindgroup(gpu_ctx, s->cur_bindgroup, s->dynamic_offsets, s->nb_dynamic_offsets);
    ngli_gpu_ctx_dispatch(gpu_ctx, nb_group_x, nb_group_y, nb_group_z);
}

void ngli_pipeline_compat_freep(struct pipeline_compat **sp)
{
    struct pipeline_compat *s = *sp;
    if (!s)
        return;

    reset_pipeline(s);
    ngli_darray_reset(&s->bindgroups);

    ngli_pipeline_graphics_reset(&s->graphics);

    ngli_freep(&s->bindgroup_layout_params.textures);
    ngli_freep(&s->bindgroup_layout_params.buffers);

    ngli_freep(&s->vertex_buffers);
    ngli_freep(&s->textures);
    ngli_freep(&s->buffers);

    if (s->compat_info) {
        for (size_t i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
            if (s->ubuffers[i]) {
                if (s->mapped_datas[i])
                    ngli_buffer_unmap(s->ubuffers[i]);
                ngli_buffer_freep(&s->ubuffers[i]);
            }
        }
    }
    ngli_freep(sp);
}
