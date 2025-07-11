/*
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

#ifndef GLSLANG_UTILS_H
#define GLSLANG_UTILS_H

#include <stddef.h>

#include <glslang/build_info.h>
#include <glslang/Include/glslang_c_interface.h>

#include "ngpu/program.h"

int ngli_glslang_init(void);
int ngli_glslang_compile(enum ngpu_program_stage stage, const char *src, int debug, void **datap, size_t *sizep);
void ngli_glslang_uninit(void);

#endif
