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

#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define IPC_TAG(a,b,c,d) (((uint32_t)(a))<<24 | (b)<<16 | (c)<<8 | (d))
#define IPC_TAG_BUF(buf) IPC_TAG((buf)[0], (buf)[1], (buf)[2], (buf)[3])

enum ipc_tag {
    IPC_SCENE        = IPC_TAG('s','c','n','e'),
    IPC_FILE         = IPC_TAG('f','i','l','e'),
    IPC_DURATION     = IPC_TAG('d','u','r','t'),
    IPC_ASPECT_RATIO = IPC_TAG('r','t','i','o'),
    IPC_FRAMERATE    = IPC_TAG('r','a','t','e'),
    IPC_CLEARCOLOR   = IPC_TAG('c','c','l','r'),
    IPC_SAMPLES      = IPC_TAG('m','s','a','a'),
    IPC_INFO         = IPC_TAG('i','n','f','o'),
    IPC_RECONFIGURE  = IPC_TAG('r','c','f','g'),
};

void ipc_write_be32(uint8_t *buf, uint32_t v);
int ipc_readbuf(int fd, uint8_t *buf, ssize_t size);
int ipc_read_pkt_data(int fd, const uint8_t *header, uint8_t **dst_p, int *dstsize_p);

#endif
