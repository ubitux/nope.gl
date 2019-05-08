/*
 * Copyright 2019 GoPro Inc.
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
#include <float.h>

#include "nodes.h"
#include "timerange.h"
#include "utils.h"

/*
 * dst: destination array
 * cur: current ranges (takes over sub)
 * sub: subtitute where applicable: a sub segment can disable existing segments
 * in cur but cannot re-enable something disabled.
 */
static int timerange_sub(struct darray *dst, const struct darray *cur, const struct darray *sub)
{
    const struct timerangemode *cur_data = ngli_darray_data(cur);
    const struct timerangemode *sub_data = ngli_darray_data(sub);

    const int n_cur = ngli_darray_count(cur);
    const int n_sub = ngli_darray_count(sub);

    int cur_i = 0, sub_i = 0;
    const struct timerangemode *last_cur = NULL, *last_sub = NULL;

    while (cur_i < n_cur || sub_i < n_sub) {

        const double next_cur_start = cur_i == n_cur ? DBL_MAX : cur_data[cur_i].start_time;
        const double next_sub_start = sub_i == n_sub ? DBL_MAX : sub_data[sub_i].start_time;
        const int insert_cur = next_cur_start < next_sub_start;

        if (insert_cur) {
            last_cur = &cur_data[cur_i++];

            struct timerangemode *seg = ngli_darray_push(dst, last_cur);
            if (!seg)
                return -1;

            if (last_sub) {
                if (last_sub->type == NGLI_TIMERANGE_NOOP) {
                    seg->type = NGLI_TIMERANGE_NOOP;
                } else if (last_sub->type == NGLI_TIMERANGE_ONCE &&
                           seg->type != NGLI_TIMERANGE_NOOP) {
                    seg->type = NGLI_TIMERANGE_ONCE;
                    seg->render_time = last_sub->render_time;
                }
            }

        } else {
            last_sub = &sub_data[sub_i++];

            struct timerangemode *seg = ngli_darray_push(dst, last_sub);
            if (!seg)
                return -1;

            if (last_cur) {
                if (last_cur->type == NGLI_TIMERANGE_NOOP) {
                    seg->type = NGLI_TIMERANGE_NOOP;
                } else if (last_cur->type == NGLI_TIMERANGE_ONCE &&
                           seg->type != NGLI_TIMERANGE_NOOP) {
                    seg->type = NGLI_TIMERANGE_ONCE;
                    //seg->render_time = last_cur->render_time;
                }
            }
        }
    }

    return 0;
}

static int timerange_squash(struct darray *dst, const struct darray *src)
{
    const struct timerangemode *src_data = ngli_darray_data(src);
    const struct timerangemode *last = NULL;

    for (int i = 0; i < ngli_darray_count(src); i++) {
        const struct timerangemode *cur = &src_data[i];
        if (last && last->type == cur->type &&
            (last->type != NGLI_TIMERANGE_ONCE || last->render_time == cur->render_time))
            continue;
        if (!ngli_darray_push(dst, cur))
            return -1;
        last = cur;
    }

    return 0;
}

static int timerange_squash_inplace(struct darray *dst)
{
    struct darray tmp;
    ngli_darray_init(&tmp, sizeof(struct timerangemode), 0);
    int ret = timerange_squash(&tmp, dst);
    ngli_darray_reset(dst);
    *dst = tmp;
    return ret;
}

static int timerange_sub_from_node(struct darray *cur, const struct ngl_node *range_node)
{
    struct darray sub;

    ngli_assert(range_node->class->id == NGL_NODE_TIMERANGEFILTER);
    int ret = ngli_node_timerangefilter_nodes_to_segments(range_node, &sub);
    if (ret < 0) {
        ngli_darray_reset(&sub);
        return ret;
    }

    struct darray dst;
    ngli_darray_init(&dst, sizeof(struct timerangemode), 0);
    ret = timerange_sub(&dst, cur, &sub);

    ngli_darray_reset(&sub);
    ngli_darray_reset(cur);
    *cur = dst;
    return ret;
}

int ngli_timerange_init(struct timerange *s, const struct darray *range_nodes_array)
{
    ngli_darray_init(&s->ranges, sizeof(struct timerangemode), 0);

    const struct ngl_node **range_nodes = ngli_darray_data(range_nodes_array);
    for (int j = 0; j < ngli_darray_count(range_nodes_array); j++) {
        int ret = timerange_sub_from_node(&s->ranges, range_nodes[j]);
        if (ret < 0)
            return ret;
    }
    return timerange_squash_inplace(&s->ranges);
}

void ngli_timerange_reset(struct timerange *s)
{
    ngli_darray_reset(&s->ranges);
    memset(s, 0, sizeof(*s));
}
