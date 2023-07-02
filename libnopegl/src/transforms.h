/*
 * Copyright 2016-2022 GoPro Inc.
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

#ifndef TRANSFORMS_H
#define TRANSFORMS_H

#include "internal.h"

int ngli_transform_chain_check(const struct ngl_node *node);
void ngli_transform_chain_compute(const struct ngl_node *node, float *matrix);
void ngli_transform_draw(struct ngl_node *node);

struct transforms;

struct transforms *ngli_transforms_create(void);

int ngli_transforms_push_matrix_ref(struct transforms *s, const float *matrix);
int ngli_transforms_push_matrices_from_nodes(struct transforms *s, const struct ngl_node *node);

void ngli_transforms_compute(const struct transforms *s, float *matrix);

void ngli_transforms_freep(struct transforms **sp);

#endif
