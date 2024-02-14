/*
 * Copyright 2024 Clément Bœsch <u pkh.me>
 * Copyright 2024 Nope Forge
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

vec4 filter_rectangle(vec4 color, vec2 coords, vec2 size, vec4 rounding, float diffusion)
{
    coords = ngli_apply_shape_space(coords);
    float sd = ngli_sd_rectangle(coords, size, rounding);
    float alpha = ngli_alpha_from_sd(sd, diffusion);
    return color * alpha;
}
