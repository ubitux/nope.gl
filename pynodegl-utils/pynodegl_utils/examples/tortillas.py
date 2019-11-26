import array
import math

import pynodegl as ngl

from pynodegl_utils.misc import scene


vec_sub = lambda v1, v2: [a - b for a, b in zip(v1, v2)]
vec_add = lambda v1, v2: [a + b for a, b in zip(v1, v2)]
vec_mul = lambda v1, v2: [a * b for a, b in zip(v1, v2)]

def vec3_cross(v1, v2):
    return [v1[1]*v2[2] - v1[2]*v2[1],
            v1[2]*v2[0] - v1[0]*v2[2],
            v1[0]*v2[1] - v1[1]*v2[0]]

vec_len = lambda v: math.sqrt(sum(x*x for x in v))

def vec_norm(v):
    if tuple(v) == (0,0,0):
        return v
    f = 1. / vec_len(v);
    return [x*f for x in v]

vec3_normal = lambda v1, v2: vec_norm(vec3_cross(v1, v2))

mat3xvec3 = lambda m, v: [
    m[0]*v[0] + m[3]*v[1] + m[6]*v[2],
    m[1]*v[0] + m[4]*v[1] + m[7]*v[2],
    m[2]*v[0] + m[5]*v[1] + m[8]*v[2],
]

def vec3_rot(v, rot, anchor=(0,0,0)):
    x, y, z = rot

    if anchor != (0,0,0):
        v = vec_add(v, anchor)

    rotx = [
        1.,           0.,           0.,
        0.,  math.cos(x),  math.sin(x),
        0., -math.sin(x),  math.cos(x),
    ]
    v = mat3xvec3(rotx, v)
    roty = [
        math.cos(y), 0., -math.sin(y),
                 0., 1.,           0.,
        math.sin(y), 0.,  math.cos(y),
    ]
    v = mat3xvec3(roty, v)
    rotz = [
        math.cos(z), math.sin(z), 0.,
       -math.sin(z), math.cos(z), 0.,
                 0.,          0., 1.,
    ]
    v = mat3xvec3(rotz, v)

    if anchor != (0,0,0):
        v = vec_sub(v, anchor)

    return v

@scene(k=scene.Range(range=[3, 50]),
       r=scene.Range(range=[0.01, 2], unit_base=100),
       h=scene.Range(range=[0.01, 2], unit_base=100),
       x_angle=scene.Range(range=[-90, 90]),
       y_angle=scene.Range(range=[-90, 90]),
       twist=scene.Range(range=[-180, 180]))
def cylinder(cfg, k=8, r=.5, h=.9, x_angle=-10, y_angle=10, twist=90):

    cfg.duration = 10

    fragment = '''#version 100
precision mediump float;
varying vec3 var_normal;
void main(void) {
    gl_FragColor = vec4(vec3(0.5 + var_normal), 1.0);
}'''

    vertices_data = array.array('f')
    normals_data = array.array('f')
    step = 2. * math.pi / k
    for i in range(k):

        cur_angle = i * step
        cur_x = math.sin(cur_angle)
        cur_y = math.cos(cur_angle)

        nxt_angle = ((i + 1) % k) * step
        nxt_x = math.sin(nxt_angle)
        nxt_y = math.cos(nxt_angle)

        bottom_0 = [cur_x*r,    cur_y*r,    0]
        bottom_1 = [nxt_x*r,    nxt_y*r,    0]
        top_0    = [cur_x*r/2., cur_y*r/2., h]
        top_1    = [nxt_x*r/2., nxt_y*r/2., h]

        rot_vec = [
                x_angle * math.pi / 180.,
                y_angle * math.pi / 180.,
                twist   * math.pi / 180.,
        ]
        top_0 = vec3_rot(top_0, rot_vec)
        top_1 = vec3_rot(top_1, rot_vec)

        norm = vec3_normal(top_0, bottom_1)
        normals_data.extend(norm * 6)

        vertices_data.extend(bottom_0 + bottom_1 + top_0 +
                             bottom_1 + top_0 + top_1)

    vertices = ngl.BufferVec3(data=vertices_data)
    normals  = ngl.BufferVec3(data=normals_data)

    program = ngl.Program(fragment=fragment)
    geom = ngl.Geometry(vertices, normals=normals, topology='triangle_list')

    render = ngl.Render(geom, program)

    gc = ngl.GraphicConfig(render, depth_test=True)

    node = ngl.Translate(gc, (0, 0, -h/2))

    for i in range(3):
        rot_animkf = ngl.AnimatedFloat([ngl.AnimKeyFrameFloat(0,            0),
                                        ngl.AnimKeyFrameFloat(cfg.duration, 360 * (i + 1))])
        axis = [int(i == x) for x in range(3)]
        node = ngl.Rotate(node, axis=axis, anim=rot_animkf)

    camera = ngl.Camera(node)
    camera.set_eye(0.0, 0.0, 2.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio_float)
    camera.set_clipping(1.0, 10.0)

    return camera
