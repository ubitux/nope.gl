/*
 * Copyright 2018 GoPro Inc.
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

#include "internal.h"
#include "params.h"

struct userswitch_opts {
    struct ngl_node *child;
    struct livectl live;
};

#define OFFSET(x) offsetof(struct userswitch_opts, x)
static const struct node_param userswitch_params[] = {
    {"child",  NGLI_PARAM_TYPE_NODE, OFFSET(child),
               .flags=NGLI_PARAM_FLAG_NON_NULL,
               .desc=NGLI_DOCSTRING("scene to be rendered or not")},
    {"enabled", NGLI_PARAM_TYPE_BOOL, OFFSET(live.val.i), {.i32=1},
               .flags=NGLI_PARAM_FLAG_ALLOW_LIVE_CHANGE,
               .desc=NGLI_DOCSTRING("set if the scene should be rendered")},
    {"live_id",  NGLI_PARAM_TYPE_STR, OFFSET(live.id),
                 .desc=NGLI_DOCSTRING("live control identifier")},
    {NULL}
};

static int userswitch_init(struct ngl_node *node)
{
    const struct userswitch_opts *o = node->opts;
    return ngli_node_register_gate(node, o->child);
}

static void userswitch_set_gates(struct ngl_node *node, double t)
{
    const struct userswitch_opts *o = node->opts;
    const int enabled = o->live.val.i[0];
    ngli_node_set_gate_state(node, 0, enabled ? NGLI_GATE_STATE_OPENED : NGLI_GATE_STATE_CLOSED);
}

static int userswitch_update(struct ngl_node *node, double t)
{
    const struct userswitch_opts *o = node->opts;
    const int enabled = o->live.val.i[0];
    return enabled ? ngli_node_update(o->child, t) : 0;
}

static void userswitch_draw(struct ngl_node *node)
{
    const struct userswitch_opts *o = node->opts;
    const int enabled = o->live.val.i[0];
    if (enabled)
        ngli_node_draw(o->child);
}

const struct node_class ngli_userswitch_class = {
    .id             = NGL_NODE_USERSWITCH,
    .name           = "UserSwitch",
    .init           = userswitch_init,
    .set_gates      = userswitch_set_gates,
    .update         = userswitch_update,
    .draw           = userswitch_draw,
    .opts_size      = sizeof(struct userswitch_opts),
    .params         = userswitch_params,
    .flags          = NGLI_NODE_FLAG_LIVECTL,
    .livectl_offset = OFFSET(live),
    .file           = __FILE__,
};
