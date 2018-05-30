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

#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>

struct queue;

typedef void (*free_func_type)(void *user_arg, void *msg);

struct queue *ngli_queue_create(void);
int ngli_queue_init(struct queue *q,
                    int nb_msg, size_t msg_size,
                    free_func_type free_func, void *user_arg);
int ngli_queue_push(struct queue *q, void *msg);
int ngli_queue_pull(struct queue *q, void *msg);
void ngli_queue_set_push_err(struct queue *q, int err);
void ngli_queue_set_pull_err(struct queue *q, int err);
int ngli_queue_get_nb_msg(struct queue *q);
void ngli_queue_flush(struct queue *q);
void ngli_queue_freep(struct queue **qp);

#endif
