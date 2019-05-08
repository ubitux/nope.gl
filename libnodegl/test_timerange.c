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

#include <stdio.h>
#include <stddef.h>

#include "nodegl.h"
#include "timerange.h"
#include "utils.h"

struct segment {
    int node_class_id;
    double start_time;
    double render_time;
};

struct track {
    const struct segment **segments;
};

struct test {
    const char *label;
    const struct track **tracks;
};

#define NOOP(start)         &(const struct segment){NGL_NODE_TIMERANGEMODENOOP, start}
#define CONT(start)         &(const struct segment){NGL_NODE_TIMERANGEMODECONT, start}
#define ONCE(start, render) &(const struct segment){NGL_NODE_TIMERANGEMODEONCE, start, render}

#define TRACK(...) &(const struct track){.segments=(const struct segment *[]){__VA_ARGS__, NULL}}

#define TEST(label, ...) {label, .tracks = (const struct track *[]){__VA_ARGS__, NULL}}

static const struct test tests[] = {
    TEST("simple",
         TRACK(NOOP(0.2), CONT(0.3), NOOP(0.5)),
         TRACK(NOOP(0.1), CONT(0.4), NOOP(0.7))
    ),
    TEST("neg+multiple",
         TRACK(NOOP(-0.3), CONT(0.1), NOOP(0.2), CONT(0.3), NOOP(0.4)),
         TRACK(NOOP(0.15), CONT(0.35))
    ),
};

static struct ngl_node *node_from_segment(const struct segment *segment)
{
    switch (segment->node_class_id) {
        case NGL_NODE_TIMERANGEMODECONT:
        case NGL_NODE_TIMERANGEMODENOOP:
            return ngl_node_create(segment->node_class_id, segment->start_time);
        case NGL_NODE_TIMERANGEMODEONCE:
            return ngl_node_create(segment->node_class_id, segment->start_time, segment->render_time);
    }
    return NULL;
}

static const char * const tr_str[] = {
    [NGLI_TIMERANGE_ONCE] = "once",
    [NGLI_TIMERANGE_NOOP] = "noop",
    [NGLI_TIMERANGE_CONT] = "cont",
};

static void print_ranges(const struct darray *ranges)
{
    const struct timerangemode *ranges_data = ngli_darray_data(ranges);

    for (int i = 0; i < ngli_darray_count(ranges); i++) {
        printf("%s: %g", tr_str[ranges_data[i].type], ranges_data[i].start_time);
        if (ranges_data[i].type == NGLI_TIMERANGE_ONCE)
            printf(" @ %g", ranges_data[i].render_time);
        printf("\n");
    }
}

int main(void)
{
    struct ngl_node *dummy_child = ngl_node_create(NGL_NODE_GROUP);
    if (!dummy_child)
        return -1;

    for (int test_id = 0; test_id < NGLI_ARRAY_NB(tests); test_id++) {
        const struct test *test = &tests[test_id];
        struct darray range_nodes;

        ngli_darray_init(&range_nodes, sizeof(struct ngl_node *), 0);

        printf("test #%d/%d: %s\n", test_id + 1, NGLI_ARRAY_NB(tests), test->label);

        for (int track_id = 0; test->tracks[track_id]; track_id++) {
            const struct track *track = test->tracks[track_id];
            struct ngl_node *trf = ngl_node_create(NGL_NODE_TIMERANGEFILTER, dummy_child);
            for (int seg_id = 0; track->segments[seg_id]; seg_id++) {
                const struct segment *segment = track->segments[seg_id];
                struct ngl_node *trm = node_from_segment(segment);
                ngl_node_param_add(trf, "ranges", 1, &trm);
            }
            ngli_darray_push(&range_nodes, &trf);
        }

        struct timerange tr = {0};
        int ret = ngli_timerange_init(&tr, &range_nodes);
        if (ret < 0)
            return ret;

        print_ranges(&tr.ranges);

        ngli_timerange_reset(&tr);
    }

    return 0;
}
