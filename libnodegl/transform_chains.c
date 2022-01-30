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

#include <string.h>

#include "nodegl.h"
#include "transform_chains.h"

void ngli_transform_chains_init(struct transform_chains *s)
{
    memset(s, 0, sizeof(*s));
    ngli_darray_init(&s->chains, sizeof(struct darray), 0);
}

static int split(struct transform_chains *s, int n)
{
    for (int i = 0; i < n; i++) {
        struct darray *chain = ngli_darray_push(&s->chains, NULL);
        if (!chain)
            return NGL_ERROR_MEMORY;
        ngli_darray_init(chain, 16 * sizeof(float), 1);

        const struct darray *chains = ngli_darray_data(&s->chains);
        const struct darray *ref_chain = &chains[s->chain_id];
        for (int k = 0; k < ngli_darray_count(ref_chain); k++)
            if (!ngli_darray_push(chain, NULL))
                return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int grow_by_one(struct transform_chains *s)
{
    struct darray *chains = ngli_darray_data(&s->chains);
    if (!ngli_darray_push(&chains[s->chain_id], NULL))
        return NGL_ERROR_MEMORY;
    return 0;
}

void ngli_transform_chains_reset(struct transform_chains *s)
{
    struct darray *chains = ngli_darray_data(&s->chains);
    for (int i = 0; i < ngli_darray_count(&s->chains); i++)
        ngli_darray_reset(&chains[i]);
    ngli_darray_reset(&s->chains);
    memset(darray, 0, sizeof(*s));
}

static int alloc_chains_rec(struct transform_chains *s, struct ngl_node *node, int position)
{
    struct darray *children_array = &node->children;
    struct ngl_node **children = ngli_darray_data(children_array);

    int ret = ngli_darray_push(node->trf_indexes, &position);
    if (ret < 0)
        return ret;

    if (node->cls->category == NGLI_NODE_CATEGORY_TRANSFORM) {
        ret = grow_by_one(s);
    } else if (node->cls->category == NGLI_NODE_CATEGORY_SPLITTER) {
        ret = split(s, ngli_darray_count(children_array));
    } else if (node->cls->category == NGLI_NODE_CATEGORY_RENDER) {
        if (!ngli_darray_push(&node->trf_indexes, &s->chain_id))
            ret = NGL_ERROR_MEMORY;
    }
    if (ret < 0)
        return ret;

    const int inc_chain = node->cls->category == NGLI_NODE_CATEGORY_SPLITTER;
    for (int i = 0; i < ngli_darray_count(children_array); i++) {
        struct ngl_node *child = children[i];

        ret = alloc_chains_rec(child, position + 1);
        if (ret < 0)
            return ret;

        s->chain_id += inc_chain;
    }
    return 0;
}

int ngli_transform_chains_alloc_chains(struct transform_chains *s, struct ngl_node *node)
{
    return alloc_chains_rec(s, node, 0);
}

static int update_chains(struct transform_chains *s, struct ngl_node *node, double t, int position)
{
    struct darray *children_array = &node->children;
    struct ngl_node **children = ngli_darray_data(children_array);

    int ret = ngli_darray_push(node->trf_indexes, &position);
    if (ret < 0)
        return ret;

    if (node->cls->category == NGLI_NODE_CATEGORY_TRANSFORM) {

    } else if (node->cls->category == NGLI_NODE_CATEGORY_SPLITTER) {

    } else if (node->cls->category == NGLI_NODE_CATEGORY_RENDER) {

    }
    if (ret < 0)
        return ret;

    const int inc_chain = node->cls->category == NGLI_NODE_CATEGORY_SPLITTER;
    for (int i = 0; i < ngli_darray_count(children_array); i++) {
        struct ngl_node *child = children[i];

        ret = update_chains(child, t, position + 1);
        if (ret < 0)
            return ret;

        s->chain_id += inc_chain;
    }
    return 0;
}

int ngli_transform_chains_update_chains(struct transform_chains *s, struct ngl_node *node)
{

}
