/*
 * Copyright 2023 Nope Project
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

//#include <inttypes.h>
//#include <math.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>
#include <SDL.h>
//#include <SDL_syswm.h>
//#include <sys/types.h>
//#include <sys/stat.h>

// #include "common.h"
#include "memory.h"
// #include "player.h"
#include "wsi/wsi.h"

struct ngl_player {

};

struct ngl_player *ngl_player_create(const struct ngl_player_config *cfg)
{
    struct ngl_player *s = ngli_calloc(1, sizeof(*s));
    return s;
}

int ngl_player_init(struct ngl_player *s)
{
    return 0;
}

int ngl_player_set_scene(struct ngl_player *s, struct ngl_scene *scene)
{
    
    return 0;
}

int ngl_player_start(struct ngl_player *s)
{
    
    return 0;
}

int ngl_player_stop(struct ngl_player *s)
{
    
    return 0;
}

int ngl_player_seek(struct ngl_player *s)
{
    
    return 0;
}

void ngl_player_main_loop(struct ngl_player *s)
{
    
}

void ngl_player_freep(struct ngl_player **s)
{
    
}
