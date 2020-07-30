/*
 * Copyright 2020 GoPro Inc.
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

#define _POSIX_C_SOURCE 200112L // for struct sockaddr with glibc

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>

#include <nodegl.h>

#include "common.h"
#include "ipc.h"
#include "opts.h"
#include "player.h"

struct ctx {
    /* options */
    const char *port;
    int log_level;
    struct ngl_config cfg;
    int aspect[2];
    int player_ui;

    int sock_fd;
    struct addrinfo *addr_info;
    struct addrinfo *addr;

    struct player p;
    int thread_started;
    pthread_mutex_t lock;
    pthread_t thread;
    int stop_order;
    int own_session_file;
};

#define OFFSET(x) offsetof(struct ctx, x)
static const struct opt options[] = {
    {"-p", "--port",          OPT_TYPE_STR,      .offset=OFFSET(port)},
    {"-l", "--loglevel",      OPT_TYPE_LOGLEVEL, .offset=OFFSET(log_level)},
    {"-b", "--backend",       OPT_TYPE_BACKEND,  .offset=OFFSET(cfg.backend)},
    {"-s", "--size",          OPT_TYPE_RATIONAL, .offset=OFFSET(cfg.width)},
    {"-a", "--aspect",        OPT_TYPE_RATIONAL, .offset=OFFSET(aspect)},
    {"-z", "--swap_interval", OPT_TYPE_INT,      .offset=OFFSET(cfg.swap_interval)},
    {"-c", "--clear_color",   OPT_TYPE_COLOR,    .offset=OFFSET(cfg.clear_color)},
    {"-m", "--samples",       OPT_TYPE_INT,      .offset=OFFSET(cfg.samples)},
    {"-u", "--disable-ui",    OPT_TYPE_TOGGLE,   .offset=OFFSET(player_ui)},
};

static int create_session_file(struct ctx *s)
{
    char path[256];
    snprintf(path, sizeof(path), "/tmp/ngl-desktop/session.%s", s->port);
    int ret = makedirs(path, 0750);
    if (ret < 0)
        return ret;
    int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0640);
    if (fd < 0) {
        perror("open");
        return NGL_ERROR_IO;
    }
    close(fd);
    s->own_session_file = 1;
    return 0;
}

static int remove_session_file(struct ctx *s)
{
    if (!s->own_session_file)
        return 0;
    char path[256];
    snprintf(path, sizeof(path), "/tmp/ngl-desktop/session.%s", s->port);
    int ret = unlink(path);
    if (ret < 0 && errno != EEXIST) {
        perror("unlink");
        return NGL_ERROR_IO;
    }
    return 0;
}

static int send_info(int fd, const struct ngl_config *cfg)
{
    uint8_t buf[8 + 256];

    static const char * const backend_map[] = {
        [NGL_BACKEND_OPENGL]   = "gl",
        [NGL_BACKEND_OPENGLES] = "gles",
    };
    if (cfg->backend < 0 || cfg->backend >= ARRAY_NB(backend_map))
        return NGL_ERROR_BUG;
    const char *backend_str = backend_map[cfg->backend];
    if (!backend_str)
        return NGL_ERROR_BUG;

    struct utsname name;
    int ret = uname(&name);
    if (ret < 0)
        return NGL_ERROR_GENERIC;

    memcpy(buf, "resp", 4);
    const int len = snprintf((char *)(buf + 8), sizeof(buf) - 8,
                             "backend=%s\nsystem=%s\n", backend_str, name.sysname);
    ipc_write_be32(buf + 4, len + 1);
    ssize_t n = write(fd, buf, 8 + len + 1);
    if (n != 8 + len + 1) {
        fprintf(stderr, "unable to write response\n");
        return NGL_ERROR_IO;
    }

    return 0;
}

static int handle_commands(struct ctx *s, int fd)
{
    for (;;) {
        pthread_mutex_lock(&s->lock);
        int stop = s->stop_order;
        pthread_mutex_unlock(&s->lock);
        if (stop)
            return -1;

        uint8_t cmd_header[8];
        int ret = ipc_readbuf(fd, cmd_header, sizeof(cmd_header));
        if (ret < 0)
            return 0;

        if (!memcmp(cmd_header, "info\0\0\0\0", 8)) {
            /* Note: we use the player ngl_config because it contains the
             * backend in use */
            ret = send_info(fd, &s->p.ngl_config);
            if (ret < 0)
                return ret;
            continue;
        }

        if (memcmp(cmd_header, "nglp", 4)) {
            fprintf(stderr, "invalid packet received\n");
            return NGL_ERROR_INVALID_DATA;
        }

        uint8_t *data;
        int size;
        ret = ipc_read_pkt_data(fd, cmd_header, &data, &size);
        if (ret < 0)
            return 0;

        SDL_Event event = {
            .user = {
                .type  = SDL_USEREVENT,
                .code  = IPC_TAG_BUF(cmd_header),
                .data1 = data,
                .data2 = (void *)(intptr_t)size,
            },
        };

        if (SDL_PushEvent(&event) != 1)
            free(data);
    }
}

static void *server_start(void *arg)
{
    struct ctx *s = arg;

    int run = 1;
    while (run) {
        const int conn_fd = accept(s->sock_fd, s->addr->ai_addr, &s->addr->ai_addrlen);
        if (conn_fd < 0) {
            perror("accept");
            goto end;
        }
        run = handle_commands(s, conn_fd) != -1;
        close(conn_fd);
    }

end:
    return NULL;
}

static void stop_server(struct ctx *s)
{
    pthread_mutex_lock(&s->lock);
    s->stop_order = 1;
    pthread_mutex_unlock(&s->lock);
}

static struct ngl_node *get_default_scene(const char *port)
{
    char subtext_buf[64];
    snprintf(subtext_buf, sizeof(subtext_buf), "Listening on port %s", port);
    static const float fg_color[]  = {1.0, 2/3., 0.0, 1.0};
    static const float subtext_h[] = {0.0, 0.5, 0.0};

    struct ngl_node *group = ngl_node_create(NGL_NODE_GROUP);
    struct ngl_node *texts[] = {
        ngl_node_create(NGL_NODE_TEXT),
        ngl_node_create(NGL_NODE_TEXT),
    };

    if (!group || !texts[0] || !texts[1])
        return NULL;
    ngl_node_param_set(texts[0], "text", "No scene");
    ngl_node_param_set(texts[0], "fg_color", fg_color);
    ngl_node_param_set(texts[1], "text", subtext_buf);
    ngl_node_param_set(texts[1], "box_height", subtext_h);
    ngl_node_param_add(group, "children", 2, texts);
    return group;
}

static int setup_network(struct ctx *s)
{
    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags    = AI_PASSIVE,
    };

    int ret = getaddrinfo(NULL, s->port, &hints, &s->addr_info);
    if (ret) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

    struct addrinfo *rp;
    for (rp = s->addr_info; rp; rp = rp->ai_next) {
        s->sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s->sock_fd < 0)
            continue;

        ret = bind(s->sock_fd, rp->ai_addr, rp->ai_addrlen);
        if (ret != -1)
            break;

        close(s->sock_fd);
        s->sock_fd = -1;
    }

    if (!rp) {
        fprintf(stderr, "unable to bind\n");
        return -1;
    }

    s->addr = rp;

    if (listen(s->sock_fd, 1) < 0) {
        perror("listen");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct ctx s = {
        .port               = "1234",
        .cfg.width          = DEFAULT_WIDTH,
        .cfg.height         = DEFAULT_HEIGHT,
        .log_level          = NGL_LOG_INFO,
        .aspect[0]          = 1,
        .aspect[1]          = 1,
        .cfg.swap_interval  = -1,
        .cfg.clear_color[3] = 1.f,
        .lock               = PTHREAD_MUTEX_INITIALIZER,
        .sock_fd            = -1,
        .player_ui          = 1,
    };

    int ret = opts_parse(argc, argc, argv, options, ARRAY_NB(options), &s);
    if (ret < 0 || ret == OPT_HELP) {
        opts_print_usage(argv[0], options, ARRAY_NB(options), NULL);
        return ret == OPT_HELP ? 0 : EXIT_FAILURE;
    }

    ngl_log_set_min_level(s.log_level);
    get_viewport(s.cfg.width, s.cfg.height, s.aspect, s.cfg.viewport);

    if ((ret = setup_network(&s)) < 0 ||
        (ret = create_session_file(&s)) < 0 ||
        (ret = pthread_create(&s.thread, NULL, server_start, &s)) < 0)
        goto end;
    s.thread_started = 1;

    struct ngl_node *scene = get_default_scene(s.port);
    ret = player_init(&s.p, "ngl-desktop", scene, &s.cfg, 0, s.player_ui);
    ngl_node_unrefp(&scene);
    if (ret < 0)
        goto end;

    player_main_loop();

end:
    remove_session_file(&s);

    if (s.sock_fd != -1) {
        shutdown(s.sock_fd, SHUT_RDWR);
        close(s.sock_fd);
    }

    if (s.addr_info)
        freeaddrinfo(s.addr_info);

    if (s.thread_started) {
        stop_server(&s);
        pthread_join(s.thread, NULL);
    }
    pthread_mutex_destroy(&s.lock);

    player_uninit();

    return ret;
}
