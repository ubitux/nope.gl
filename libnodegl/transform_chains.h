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

#ifndef TRANSFORM_CHAINS_H
#define TRANSFORM_CHAINS_H

#include "darray.h"

struct transform_chains {
    struct darray chains; /* array of matrices (4x4 floats) darrays */
    int chain_id;
};

void ngli_transform_chains_init(struct transform_chains *s);
int ngli_transform_chains_alloc_chains(struct transform_chains *s, struct ngl_node *node):
int ngli_transform_chains_update_chains(struct transform_chains *s, struct ngl_node *node);
void ngli_transform_chains_reset(struct transform_chains *s);

#endif
