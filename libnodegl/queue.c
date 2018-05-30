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

#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

struct queue {
    uint8_t *msg_buf;
    size_t msg_size;
    int nb_max_msg;
    int nb_msg;
    int msg_pos; // beginning position
    free_func_type free_func;
    void *user_arg;
    pthread_mutex_t lock;
    pthread_cond_t cond_push;
    pthread_cond_t cond_pull;
    int push_err;
    int pull_err;
};

struct queue *ngli_queue_create(void)
{
    struct queue *q = calloc(1, sizeof(*q));
    return q;
}

int ngli_queue_init(struct queue *q,
                     int nb_msg, size_t msg_size,
                     free_func_type free_func, void *user_arg)
{
    int ret;

    if ((ret = pthread_mutex_init(&q->lock, NULL)) ||
        (ret = pthread_cond_init(&q->cond_push, NULL)) ||
        (ret = pthread_cond_init(&q->cond_pull, NULL)))
        return ret;

    q->msg_size = msg_size;
    q->nb_max_msg = nb_msg;
    q->msg_buf = calloc(nb_msg, msg_size);
    if (!q->msg_buf)
        return -1;

    q->free_func = free_func;
    q->user_arg = user_arg;
    return 0;
}

int ngli_queue_push(struct queue *q, void *msg)
{
    pthread_mutex_lock(&q->lock);

    while (q->nb_msg == q->nb_max_msg && !q->push_err)
        pthread_cond_wait(&q->cond_push, &q->lock);

    int ret = q->push_err;
    if (!ret) {
        const int push_pos = (q->msg_pos + q->nb_msg) % q->nb_max_msg;
        memcpy(q->msg_buf + push_pos*q->msg_size, msg, q->msg_size);
        q->nb_msg++;
        pthread_cond_signal(&q->cond_pull);
    }

    pthread_mutex_unlock(&q->lock);
    return ret;
}

int ngli_queue_pull(struct queue *q, void *msg)
{
    pthread_mutex_lock(&q->lock);

    while (!q->nb_msg && !q->pull_err)
        pthread_cond_wait(&q->cond_pull, &q->lock);

    int ret = q->pull_err;
    if (!ret) {
        memcpy(msg, q->msg_buf + q->msg_pos*q->msg_size, q->msg_size);
        q->nb_msg--;
        q->msg_pos = (q->msg_pos + 1) % q->nb_max_msg;
        pthread_cond_signal(&q->cond_push);
    }

    pthread_mutex_unlock(&q->lock);
    return ret;
}

void ngli_queue_set_push_err(struct queue *q, int err)
{
    pthread_mutex_lock(&q->lock);
    q->push_err = err;
    pthread_cond_broadcast(&q->cond_push);
    pthread_mutex_unlock(&q->lock);
}

void ngli_queue_set_pull_err(struct queue *q, int err)
{
    pthread_mutex_lock(&q->lock);
    q->pull_err = err;
    pthread_cond_broadcast(&q->cond_pull);
    pthread_mutex_unlock(&q->lock);
}

int ngli_queue_get_nb_msg(struct queue *q)
{
    pthread_mutex_lock(&q->lock);
    int ret = q->nb_msg;
    pthread_mutex_unlock(&q->lock);
    return ret;
}

void ngli_queue_flush(struct queue *q)
{
    pthread_mutex_lock(&q->lock);
    for (int i = 0; i < q->nb_msg; i++) {
        const int pos = (q->msg_pos + i) % q->nb_max_msg;
        q->free_func(q->user_arg, q->msg_buf + pos*q->msg_size);
    }
    q->nb_msg = 0;
    pthread_cond_broadcast(&q->cond_push);
    pthread_mutex_unlock(&q->lock);
}

void ngli_queue_freep(struct queue **qp)
{
    struct queue *q = *qp;
    if (!q)
        return;
    ngli_queue_flush(q);
    free(q->msg_buf);
    pthread_cond_destroy(&q->cond_push);
    pthread_cond_destroy(&q->cond_pull);
    pthread_mutex_destroy(&q->lock);
    free(q);
    *qp = NULL;
}
