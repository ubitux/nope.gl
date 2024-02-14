/*
 * Copyright 2024 Clément Bœsch <u pkh.me>
 * Copyright 2024 Nope Forge
 * Copyright 2015-2020 Inigo Quilez
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

vec2 ngli_apply_shape_space(vec2 coords)
{
    coords = ngli_sat(ar_trf_shape.xy * coords + ar_trf_shape.zw);

    coords = coords * 2.0 - 1.0; // [0,1] -> [-1,-1]
    coords.y = -coords.y; // make y-axis going up

    coords = (inverse(shape_matrix) * vec4(coords, 0.0, 1.0)).xy;

    return coords;
}

float ngli_alpha_from_sd(float sd, float diffusion)
{
    // sd functions are negative inside, positive outside
    // we switch that to negative outside, positive inside
    sd = -sd;
    // Simplifications of linearstep(0,diffuse,sd), to work inside the shape
    // (because of the geometry boundaries)
    // See ngli_aa() explanations
    return diffusion > 0.0 ? ngli_sat(sd / diffusion) : ngli_sat(sd / fwidth(sd));
}

float ngli_sd_rectangle(vec2 p, vec2 b, vec4 r)
{
    r.yz = (p.x > 0.0) ? r.yz : r.xw;
    r.y  = (p.y > 0.0) ? r.y  : r.z;
    vec2 q = abs(p) - b + r.y;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.y;
}

float ngli_sd_triangle(vec2 p, float r)
{
    const float k = sqrt(3.0);
    p.x = abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0)
        p = vec2(p.x - k * p.y, -k * p.x - p.y) / 2.0;
    p.x -= clamp(p.x, -2.0 * r, 0.0);
    return -length(p) * sign(p.y);
}

float ngli_sd_circle(vec2 p, float r)
{
    return length(p) - r;
}

// https://www.shadertoy.com/view/7tSXzt
float ngli_sd_ngon(vec2 p, lowp int n, float r)
{
    float an = ngli_pi / float(n);
    vec2 acs = vec2(cos(an), sin(an));
    float bn = mod(atan(p.x, p.y), 2.0 * an) - an;
    p = length(p) * vec2(cos(bn), abs(sin(bn)));
    p -= r * acs;
    p.y += clamp(-p.y, 0.0, r * acs.y);
    return length(p) * sign(p.x);
}
