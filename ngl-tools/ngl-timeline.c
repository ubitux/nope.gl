/*
 * Copyright 2017 GoPro Inc.
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
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <nodegl.h>

static struct ngl_node *get_scene(const char *filename)
{
    struct ngl_node *scene = NULL;
    char *buf = NULL;
    struct stat st;

    int fd = open(filename, O_RDONLY);
    if (fd == -1)
        goto end;

    if (fstat(fd, &st) == -1)
        goto end;

    buf = malloc(st.st_size + 1);
    if (!buf)
        goto end;

    int n = read(fd, buf, st.st_size);
    buf[n] = 0;

    scene = ngl_node_deserialize(buf);

end:
    if (fd != -1)
        close(fd);
    free(buf);
    return scene;
}

int main(int ac, char **av)
{
    if (ac < 2) {
        fprintf(stderr, "Usage: %s <scene.ngl>\n", av[0]);
        return -1;
    }

    struct ngl_node *scene = get_scene(av[1]);
    if (!scene)
        return -1;

    char *s = ngl_node_graph_timeline(scene);
    printf("%s\n", s);
    ngl_node_unrefp(&scene);
    return 0;
}
