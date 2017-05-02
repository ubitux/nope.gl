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

#include <inttypes.h>
#include <string.h>

#include "bstr.h"
#include "log.h"
#include "nodes.h"
#include "nodegl.h"

extern const struct node_param ngli_base_node_params[];

struct serial_ctx {
    struct ngl_node **nodes;
    int nb_nodes;
};

static void register_node(struct serial_ctx *sctx,
                          const struct ngl_node *node)
{
    struct ngl_node **new_nodes = realloc(sctx->nodes, (sctx->nb_nodes + 1) * sizeof(*new_nodes));
    if (!new_nodes)
        return;
    new_nodes[sctx->nb_nodes++] = (struct ngl_node *)node;
    sctx->nodes = new_nodes;
}

static int get_node_id(const struct serial_ctx *sctx,
                       const struct ngl_node *node)
{
    for (int i = 0; i < sctx->nb_nodes; i++)
        if (sctx->nodes[i] == node)
            return i;
    return -1;
}

static void serialize(struct serial_ctx *sctx,
                      struct bstr *b,
                      const struct ngl_node *node);

static void serialize_children(struct serial_ctx *sctx,
                               struct bstr *b,
                               const struct ngl_node *node,
                               uint8_t *priv,
                               const struct node_param *p)
{
    while (p && p->key) {
        switch (p->type) {
            case PARAM_TYPE_NODE: {
                const struct ngl_node *child = *(struct ngl_node **)(priv + p->offset);
                if (child)
                    serialize(sctx, b, child);
                break;
            }
            case PARAM_TYPE_NODELIST: {
                struct ngl_node **children = *(struct ngl_node ***)(priv + p->offset);
                const int nb_children = *(int *)(priv + p->offset + sizeof(struct ngl_node **));

                for (int i = 0; i < nb_children; i++)
                    serialize(sctx, b, children[i]);
                break;
            }
        }
        p++;
    }
}

static void append(char **dstp, size_t *dst_lenp, const char *s, size_t len)
{
    char *new, *dst = *dstp;
    size_t dst_len = *dst_lenp;

    if (!len)
        return;

    dst_len += len;
    new = realloc(dst, dst_len + 1);
    if (!new)
        return;
    dst = new;
    strncpy(dst + (*dst_lenp), s, len);
    dst[dst_len] = 0;
    *dstp = dst;
    *dst_lenp = dst_len;
}

static char *strreplace(const char *s, const char *pattern, const char *replace)
{
    char *dst = NULL;
    size_t dst_len = 0;
    const size_t replace_len = strlen(replace);
    const size_t pattern_len = strlen(pattern);

    for (;;) {
        const char *p = strstr(s, pattern);
        if (!p)
            break;
        size_t len = p - s;
        append(&dst, &dst_len, s, len);
        append(&dst, &dst_len, replace, replace_len);
        s += len + pattern_len;
    }
    append(&dst, &dst_len, s, strlen(s));
    return dst;
}

/* FIXME: duplicated */
#define PREFETCH_TIME 1.0
#define MAX_IDLE_TIME (PREFETCH_TIME + 3.0)

static void insert_segment(struct bstr *b, int rid, double duration, double scene_duration)
{
    static const char * const segment_types[] = {
        [NGL_NODE_RENDERRANGENORENDER]   = "norender",
        [NGL_NODE_RENDERRANGECONTINUOUS] = "continuous",
        [NGL_NODE_RENDERRANGEONCE]       = "once",
    };
    const double width = duration / scene_duration * 100.;
    ngli_bstr_print(b, "<span class=\"segment %s\" style=\"width:%g%%;\"></span>\n",
                    segment_types[rid], width);
}

static void serialize(struct serial_ctx *sctx,
                      struct bstr *b,
                      const struct ngl_node *node)
{
    const int node_id = get_node_id(sctx, node);
    if (node_id != -1)
        return;

    if (node->nb_ranges) {
        char *name = strreplace(node->name, " ", "&nbsp;");
        ngli_bstr_print(b, "<tr><td class=\"nodename\">%s</td><td class=\"bar\">\n", name);
        free(name);

        const double scene_duration = 40.; // FIXME

        const struct renderrange *fr = node->ranges[0]->priv_data;
        if (fr->start_time)
            insert_segment(b, NGL_NODE_RENDERRANGECONTINUOUS, fr->start_time, scene_duration);

        for (int i = 1; i <= node->nb_ranges; i++) {
            const struct ngl_node *r = node->ranges[i - 1];
            const int rid = r->class->id;
            const struct renderrange *rr = r->priv_data;
            const struct renderrange *next_rr = i < node->nb_ranges ? node->ranges[i]->priv_data : NULL;
            const double next_time = next_rr ? next_rr->start_time : scene_duration;
            const double duration = next_time - rr->start_time;

            if (rid == NGL_NODE_RENDERRANGECONTINUOUS) {

            }

            insert_segment(b, rid, duration, scene_duration);
        }

        ngli_bstr_print(b, "</td></tr>\n");

    } else {
        serialize_children(sctx, b, node, (uint8_t *)node, ngli_base_node_params);
        serialize_children(sctx, b, node, node->priv_data, node->class->params);
    }
    register_node(sctx, node);
}

static const char *header =
"<!doctype html><html>\n"
"    <head>\n"
"        <style>\n"
"            body               { background-color:black; color:white; }\n"
"            table              { border-collapse: collapse; }\n"
"            table td           { padding:0; }\n"
"            td.bar             { width:100%; }\n"
"            td.nodename        { padding: 5px; }\n"
"            span.segment       { display:block; height:30px; float:left; }\n"
"            span.prefetch      { background-color:#55ff55; }\n"
"            span.norender      { background-color:#ff5555; }\n"
"            span.continuous    { background-color:#5555ff; }\n"
"            span.once          { background-color:#555555; }\n"
"        </style>\n"
"    </head>\n"
"    <body>\n"
"        <table>";

static const char *footer =
"        </table>\n"
"    </body>\n"
"</html>";

char *ngl_node_graph_timeline(const struct ngl_node *node)
{
    struct serial_ctx sctx = {0};
    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NULL;
    ngli_bstr_print(b, "%s\n", header);
    serialize(&sctx, b, node);
    ngli_bstr_print(b, "%s\n", footer);
    free(sctx.nodes);
    char *s = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    return s;
}
