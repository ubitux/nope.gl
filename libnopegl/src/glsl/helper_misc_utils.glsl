/*
 * Copyright 2022 GoPro Inc.
 * Copyright 2022 Clément Bœsch <u pkh.me>
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

#define ngli_sat(x) clamp(x, 0.0, 1.0)
#define ngli_linear(a, b, x) (((x) - (a)) / ((b) - (a)))
#define ngli_linearstep(a, b, x) ngli_sat(ngli_linear(a, b, x))

/*
 * Centered Anti-aliasing approximation of a signed distance which is negative
 * outside and positive inside.
 *
 * For a centered diffuse, we want:
 *    linearstep(-diffuse/2, diffuse/2, x)
 * This simplifies down to:
 *    sat(x/diffuse + 1/2)
 * To have anti-aliasing "pixel-based diffuse", the diffuse needs to match the
 * pixel size. For some reason, the gradient length (fwidth) is a good
 * approximation of that.
 */
float ngli_aa(float x) { return ngli_sat(x / fwidth(x) + 0.5); }

const vec3 ngli_luma_weights = vec3(.2126, .7152, .0722); // BT.709
const float ngli_pi = 3.14159265358979323846;
const float ngli_tau = 6.28318530717958647692;
