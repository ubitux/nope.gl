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

#include "utils.h"
#include "queue.h"

struct sender_data {
    int id;
    pthread_t tid;
    int workload;
    struct queue *queue;
};

/* same as sender_data but shuffled for testing purpose */
struct receiver_data {
    pthread_t tid;
    int workload;
    int id;
    struct queue *queue;
};

struct message {
    char *str;
    // we add some junk in the message to make sure the message size is >
    // sizeof(void*)
    int magic;
};

#define MAGIC 0xdeadc0de
#define ERR_ENDOFWORK -1234

static void free_msg(void *user_arg, void *arg)
{
    struct message *msg = arg;
    printf("free %s msg containing \"%s\"\n", (const char *)user_arg, msg->str);
    ngli_assert(msg->magic == MAGIC);
    free(msg->str);
}

static void *sender_thread(void *arg)
{
    int ret = 0;
    struct sender_data *wd = arg;

    printf("sender #%d: workload=%d\n", wd->id, wd->workload);
    for (int i = 0; i < wd->workload; i++) {
        if (rand() % wd->workload < wd->workload / 10) {
            printf("sender #%d: flushing the queue, "
                   "discarding %d message(s)\n", wd->id,
                   ngli_queue_get_nb_msg(wd->queue));
            ngli_queue_flush(wd->queue);
        } else {
            struct message msg = {
                .magic = MAGIC,
                .str = ngli_asprintf("msg %d/%d from sender %d",
                                     i + 1, wd->workload, wd->id),
            };
            if (!msg.str) {
                ret = -1;
                break;
            }

            printf("sender #%d: sending my work (%d/%d)\n",
                   wd->id, i + 1, wd->workload);
            ret = ngli_queue_push(wd->queue, &msg);
            if (ret < 0) {
                free_msg("sender", &msg);
                break;
            }
        }
    }
    printf("sender #%d: my work is done here (ret=%d)\n",
           wd->id, ret);
    ngli_queue_set_pull_err(wd->queue, ret < 0 ? ret : ERR_ENDOFWORK);
    return NULL;
}

static void *receiver_thread(void *arg)
{
    int i, ret = 0;
    struct receiver_data *rd = arg;

    for (i = 0; i < rd->workload; i++) {
        if (rand() % rd->workload < rd->workload / 10) {
            printf("receiver #%d: flushing the queue, "
                   "discarding %d message(s)\n", rd->id,
                   ngli_queue_get_nb_msg(rd->queue));
            ngli_queue_flush(rd->queue);
        } else {
            struct message msg;

            ret = ngli_queue_pull(rd->queue, &msg);
            if (ret < 0)
                break;
            ngli_assert(msg.magic == MAGIC);
            printf("got \"%s\"\n", msg.str);
            free_msg("receiver", &msg);
        }
    }

    printf("consumed enough (%d/%d), stop\n", i, rd->workload);
    ngli_queue_set_push_err(rd->queue, ret < 0 ? ret : ERR_ENDOFWORK);

    return NULL;
}

static int get_workload(int minv, int maxv)
{
    return maxv == minv ? maxv : rand() % (maxv - minv) + minv;
}

int main(int ac, char **av)
{
    int ret = 0;

    struct queue *queue = ngli_queue_create();
    if (!queue)
        return -1;

    if (ac != 8) {
        fprintf(stderr, "%s <max_queue_size> "
               "<nb_senders> <sender_min_send> <sender_max_send> "
               "<nb_receivers> <receiver_min_recv> <receiver_max_recv>\n", av[0]);
        return 1;
    }

    const int max_queue_size    = atoi(av[1]);
    const int nb_senders        = atoi(av[2]);
    const int sender_min_load   = atoi(av[3]);
    const int sender_max_load   = atoi(av[4]);
    const int nb_receivers      = atoi(av[5]);
    const int receiver_min_load = atoi(av[6]);
    const int receiver_max_load = atoi(av[7]);

    if (max_queue_size <= 0 ||
        nb_senders <= 0 || sender_min_load <= 0 || sender_max_load <= 0 ||
        nb_receivers <= 0 || receiver_min_load <= 0 || receiver_max_load <= 0) {
        fprintf(stderr, "negative values not allowed\n");
        return 1;
    }

    printf("qsize:%d / %d senders sending [%d-%d] / "
           "%d receivers receiving [%d-%d]\n", max_queue_size,
           nb_senders, sender_min_load, sender_max_load,
           nb_receivers, receiver_min_load, receiver_max_load);

    struct sender_data *senders = calloc(nb_senders, sizeof(*senders));
    struct receiver_data *receivers = calloc(nb_receivers, sizeof(*receivers));
    if (!senders || !receivers) {
        ret = -1;
        goto end;
    }

    ret = ngli_queue_init(queue, max_queue_size, sizeof(struct message),
                          free_msg, "root");
    if (ret < 0)
        goto end;

#define SPAWN_THREADS(type) do {                                                \
    for (int i = 0; i < nb_##type##s; i++) {                                    \
        struct type##_data *td = &type##s[i];                                   \
                                                                                \
        td->id = i;                                                             \
        td->queue = queue;                                                      \
        td->workload = get_workload(type##_min_load, type##_max_load);          \
                                                                                \
        ret = pthread_create(&td->tid, NULL, type##_thread, td);                \
        if (ret) {                                                              \
            fprintf(stderr, "Unable to start " #type " thread: %d\n", ret);     \
            goto end;                                                           \
        }                                                                       \
    }                                                                           \
} while (0)

#define WAIT_THREADS(type) do {                                                 \
    for (int i = 0; i < nb_##type##s; i++) {                                    \
        struct type##_data *td = &type##s[i];                                   \
                                                                                \
        ret = pthread_join(td->tid, NULL);                                      \
        if (ret) {                                                              \
            fprintf(stderr, "Unable to join " #type " thread: %d\n", ret);      \
            goto end;                                                           \
        }                                                                       \
    }                                                                           \
} while (0)

    SPAWN_THREADS(receiver);
    SPAWN_THREADS(sender);

    WAIT_THREADS(sender);
    WAIT_THREADS(receiver);

end:
    ngli_queue_freep(&queue);
    free(senders);
    free(receivers);

    if (ret < 0 && ret != ERR_ENDOFWORK) {
        fprintf(stderr, "Error: %d\n", ret);
        return 1;
    }
    return 0;
}
