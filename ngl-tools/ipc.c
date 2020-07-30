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

#include <stdlib.h>
#include <unistd.h>

#include <nodegl.h>

#include "ipc.h"

void ipc_write_be32(uint8_t *buf, uint32_t v)
{
    buf[0] = v >> 24;
    buf[1] = v >> 16 & 0xff;
    buf[2] = v >>  8 & 0xff;
    buf[3] = v       & 0xff;
}

int ipc_readbuf(int fd, uint8_t *buf, ssize_t size)
{
    ssize_t nr = 0;
    while (nr != size) {
        const ssize_t n = read(fd, buf + nr, size - nr);
        if (n <= 0)
            return -1;
        nr += n;
    }
    return 0;
}

int ipc_read_pkt_data(int fd, const uint8_t *header, uint8_t **dst_p, int *dstsize_p)
{
    uint8_t *data = NULL;
    const int size = IPC_TAG_BUF(header + 4);
    if (!size) { // valid but empty packet
        *dst_p = NULL;
        *dstsize_p = size;
        return 0;
    }

    data = malloc(size);
    if (!data)
        return NGL_ERROR_MEMORY;
    int ret = ipc_readbuf(fd, data, size);
    if (ret < 0) {
        free(data);
        return ret;
    }

    *dst_p = data;
    *dstsize_p = size;
    return 0;
}
