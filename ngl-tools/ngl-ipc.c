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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <nodegl.h>

#include "common.h"
#include "ipc.h"
#include "opts.h"

struct ctx {
    const char *host;
    const char *port;
    const char *scene;
    int show_info;
    const char *uploadfile;
    double duration;
    int aspect[2];
    int framerate[2];
    float clear_color[4];
    int samples;
    int reconfigure;
};

#define OFFSET(x) offsetof(struct ctx, x)
static const struct opt options[] = {
    {"-x", "--host",          OPT_TYPE_STR,      .offset=OFFSET(host)},
    {"-p", "--port",          OPT_TYPE_STR,      .offset=OFFSET(port)},
    {"-f", "--scene",         OPT_TYPE_STR,      .offset=OFFSET(scene)},
    {"-?", "--info",          OPT_TYPE_TOGGLE,   .offset=OFFSET(show_info)},
    {"-u", "--uploadfile",    OPT_TYPE_STR,      .offset=OFFSET(uploadfile)},
    {"-t", "--duration",      OPT_TYPE_TIME,     .offset=OFFSET(duration)},
    {"-a", "--aspect",        OPT_TYPE_RATIONAL, .offset=OFFSET(aspect)},
    {"-r", "--framerate",     OPT_TYPE_RATIONAL, .offset=OFFSET(framerate)},
    {"-c", "--clearcolor",    OPT_TYPE_COLOR,    .offset=OFFSET(clear_color)},
    {"-m", "--samples",       OPT_TYPE_INT,      .offset=OFFSET(samples)},
    {"-g", "--reconfigure",   OPT_TYPE_TOGGLE,   .offset=OFFSET(reconfigure)},
};

static int pack(uint8_t **dst_p, int *buf_size, uint32_t tag, const void *data, int datalen)
{
    uint8_t *dst = realloc(*dst_p, *buf_size + 8 + datalen);
    if (!dst)
        return NGL_ERROR_MEMORY;
    *dst_p = dst;
    ipc_write_be32(dst + *buf_size,     tag);
    ipc_write_be32(dst + *buf_size + 4, datalen);
    if (data)
        memcpy(dst + *buf_size + 8, data, datalen);
    *buf_size += 8 + datalen;
    return 0;
}

static int craft_packet(uint8_t **dst_p, int *dstsize_p, const struct ctx *s)
{
    int ret = 0;

    *dstsize_p = 8;
    *dst_p = malloc(*dstsize_p);
    if (!*dst_p)
        return NGL_ERROR_MEMORY;

    memcpy(*dst_p, "nglp\0\0\0\0", *dstsize_p); // 'p' stands for player/packet/pack/protocol

    if (s->scene) {
        char *serial_scene = get_text_file_content(strcmp(s->scene, "-") ? s->scene : NULL);
        if (!serial_scene)
            return -1;
        ret = pack(dst_p, dstsize_p, IPC_SCENE, serial_scene, strlen(serial_scene) + 1);
        free(serial_scene);
        if (ret < 0)
            goto err;
    }

    if (s->uploadfile) {
        /* get remote destination filename */
        char name[512];
        const size_t name_len = strcspn(s->uploadfile, "=");
        if (s->uploadfile[name_len] != '=') {
            fprintf(stderr, "upload file does not match \"remotename=localname\" format\n");
            ret = NGL_ERROR_INVALID_ARG;
            goto err;
        }
        if (name_len >= sizeof(name)) {
            fprintf(stderr, "remote file name too long %zd >= %zd\n", name_len, sizeof(name));
            ret = NGL_ERROR_MEMORY;
            goto err;
        }
        snprintf(name, sizeof(name), "%.*s", (int)name_len, s->uploadfile); // XXX: check print
        const size_t name_size = name_len + 1;

        /* get local file content */
        int data_size;
        uint8_t *data;
        ret = get_file_data(s->uploadfile + name_size, &data, &data_size);
        if (ret < 0)
            goto err;

        /* craft chunk */
        const int total_size = 8 + name_size + data_size;
        ret = pack(dst_p, dstsize_p, IPC_FILE, NULL, total_size);
        if (ret < 0) {
            free(data);
            goto err;
        }
        uint8_t *dst = *dst_p + *dstsize_p - total_size;
        ipc_write_be32(dst,     name_size);
        ipc_write_be32(dst + 4, data_size);
        memcpy(dst + 8, name, name_size);
        memcpy(dst + 8 + name_size, data, data_size);
        free(data);
    }

    if (s->duration >= 0.) {
        ret = pack(dst_p, dstsize_p, IPC_DURATION, &s->duration, sizeof(s->duration));
        if (ret < 0)
            goto err;
    }

    if (s->aspect[0] > 0) {
        ret = pack(dst_p, dstsize_p, IPC_ASPECT_RATIO, NULL, sizeof(s->aspect));
        if (ret < 0)
            goto err;
        uint8_t *dst = *dst_p + *dstsize_p - 8;
        ipc_write_be32(dst,     s->aspect[0]);
        ipc_write_be32(dst + 4, s->aspect[1]);
    }

    if (s->framerate[0] > 0) {
        ret = pack(dst_p, dstsize_p, IPC_FRAMERATE, NULL, sizeof(s->framerate));
        if (ret < 0)
            goto err;
        uint8_t *dst = *dst_p + *dstsize_p - 8;
        ipc_write_be32(dst,     s->framerate[0]);
        ipc_write_be32(dst + 4, s->framerate[1]);
    }

    if (s->clear_color[0] >= 0.) {
        ret = pack(dst_p, dstsize_p, IPC_CLEARCOLOR, s->clear_color, sizeof(s->clear_color));
        if (ret < 0)
            goto err;
    }

    if (s->samples >= 0) {
        ret = pack(dst_p, dstsize_p, IPC_SAMPLES, NULL, 1);
        if (ret < 0)
            goto err;
        uint8_t *dst = *dst_p + *dstsize_p - 1;
        *dst = s->samples;
    }

    if (s->reconfigure) {
        ret = pack(dst_p, dstsize_p, IPC_RECONFIGURE, NULL, 0);
        if (ret < 0)
            goto err;
    }

    ipc_write_be32(*dst_p + 4, *dstsize_p - 8);
    return 0;

err:
    free(*dst_p);
    *dst_p = NULL;
    return ret;
}

static int get_response(int fd)
{
    uint8_t resp[8];
    int ret = ipc_readbuf(fd, resp, sizeof(resp));
    if (ret < 0)
        return 0;

    if (memcmp(resp, "resp", 4)) {
        fprintf(stderr, "invalid response received\n");
        return NGL_ERROR_INVALID_DATA;
    }

    uint8_t *data;
    int size;
    ret = ipc_read_pkt_data(fd, resp, &data, &size);
    if (ret < 0)
        return 0;

    if (data && data[size - 1] == 0) {
        printf("%s", data);
        fflush(stdout);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct ctx s = {
        .host           = "127.0.0.1",
        .port           = "1234",
        .duration       = -1.,
        .aspect[0]      = -1,
        .framerate[0]   = -1,
        .clear_color[0] = -1.,
        .samples        = -1,
    };

    int ret = opts_parse(argc, argc, argv, options, ARRAY_NB(options), &s);
    if (ret < 0 || ret == OPT_HELP) {
        opts_print_usage(argv[0], options, ARRAY_NB(options), NULL);
        return ret == OPT_HELP ? 0 : EXIT_FAILURE;
    }

    struct addrinfo *addr_info = NULL;
    int fd = -1;
    uint8_t *pkt = NULL;
    int pkt_len;
    ret = craft_packet(&pkt, &pkt_len, &s);
    if (ret < 0)
        goto end;

    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };

    ret = getaddrinfo(s.host, s.port, &hints, &addr_info);
    if (ret) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        ret = EXIT_FAILURE;
        goto end;
    }

    struct addrinfo *rp;
    for (rp = addr_info; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;

        ret = connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (ret != -1)
            break;

        close(fd);
    }

    if (!rp) {
        fprintf(stderr, "unable to connect to %s\n", s.host);
        ret = EXIT_FAILURE;
        goto end;
    }

    const ssize_t n = write(fd, pkt, pkt_len);
    if (n != pkt_len) {
        fprintf(stderr, "unable write packet (%zd/%d sent)\n", n, pkt_len);
        ret = EXIT_FAILURE;
        goto end;
    }

    if (s.show_info) {
        uint8_t query[8] = "info\0\0\0\0";
        const ssize_t n = write(fd, query, sizeof(query));
        if (n != sizeof(query)) {
            fprintf(stderr, "unable write info query (%zd/%zd sent)\n", n, sizeof(query));
            ret = EXIT_FAILURE;
            goto end;
        }

        ret = get_response(fd);
        if (ret < 0) {
            ret = EXIT_FAILURE;
            goto end;
        }
    }

end:
    if (addr_info)
        freeaddrinfo(addr_info);
    free(pkt);
    if (fd != -1)
        close(fd);
    return ret;
}
