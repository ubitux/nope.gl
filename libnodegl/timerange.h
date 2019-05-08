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

#ifndef TIMERANGE_H
#define TIMERANGE_H

#include "darray.h"

enum {
    NGLI_TIMERANGE_ONCE,
    NGLI_TIMERANGE_NOOP,
    NGLI_TIMERANGE_CONT,
};

struct timerangemode {
    int type;
    double start_time;
    double render_time; // once only
    double prefetch_time;
    double max_idle_time;

    // state
    int updated;
};

struct timerange {
    struct darray ranges;
    int is_active;
    int current_range;
};

int ngli_timerange_init(struct timerange *s, const struct darray *range_nodes);
void ngli_timerange_reset(struct timerange *s);

#endif
