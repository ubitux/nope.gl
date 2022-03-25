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

#include <math.h>

#include "darray.h"
#include "memory.h"
#include "timeline.h"

struct timeline {
    struct gate root;
};

struct timeline *ngli_timeline_create(void)
{
    struct timeline *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    return s;
}

static void init_gate(struct gate *gate, struct ngl_node *node)
{
    new_gate->node = node;
    ngli_darray_init(&new_gate->children, sizeof(struct gate), 0);
    ngli_darray_init(&new_gate->activity_nodes, sizeof(struct ngl_node *), 0);
    new_gate->visit_time = -1.;
}

static int build_gate_tree(struct timeline *s, struct gate *gate, struct ngl_node *node)
{
    if (node->cls->set_gates) {
        struct gate *new_gate = ngli_darray_push(&gate->children);
        if (!new_gate)
            return NGL_ERROR_MEMORY;
        init_gate(new_gate, node);

        const struct darray *children_array = &node->children;
        const struct ngl_node **children = ngli_darray_data(children_array);
        for (int i = 0; i < ngli_darray_count(children_array); i++) {
            const struct ngl_node *child = children[i];
            int ret = build_gate_tree(s, new_gate, child);
            if (ret < 0)
                return ret;
        }
    } else if (node->cls->prefetch || node->cls->release) {
        if (!ngli_darray_push(&gate->activity_nodes, &node))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

int ngli_timeline_build(struct timeline *s, struct ngl_node *node)
{
    init_gate(&s->root, NULL);
    return build_gate_tree(s, &s->root, node);
}

static void delete_gate_tree(struct gate *gate)
{
    const struct darray *children_array = &gate->children;
    const struct gate *children = ngli_darray_data(children_array);
    for (int i = 0; i < ngli_darray_count(children_array); i++)
        delete_gate_tree(children[i]);
    ngli_darray_reset(&gate->children);
    ngli_darray_reset(&gate->activity_nodes);
}

void ngli_timeline_freep(struct timeline **sp)
{
    struct timeline *s = *sp;
    if (!s)
        return;
    delete_gate_tree(&s->root);
    ngli_freep(sp);
}
