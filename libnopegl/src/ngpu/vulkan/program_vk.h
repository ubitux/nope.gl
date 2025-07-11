/*
 * Copyright 2018-2022 GoPro Inc.
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

#ifndef NGPU_PROGRAM_VK_H
#define NGPU_PROGRAM_VK_H

#include <vulkan/vulkan.h>

#include "ngpu/program.h"

struct ngpu_ctx;

struct ngpu_program_vk {
    struct ngpu_program parent;
    VkShaderModule shaders[NGPU_PROGRAM_STAGE_NB];
};

struct ngpu_program *ngpu_program_vk_create(struct ngpu_ctx *gpu_ctx);
int ngpu_program_vk_init(struct ngpu_program *s, const struct ngpu_program_params *params);
void ngpu_program_vk_freep(struct ngpu_program **sp);

#endif
