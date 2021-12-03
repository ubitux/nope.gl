import colorsys
import pynodegl as ngl
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.misc import scene

_VERT = '''
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    uv = ngl_uvcoord;
}
'''

_GRID_HELPERS = '''
float draw_lines(float v, float n, float thickness, float res)
{
    float px = 1.0 / res;                                   // normalized length of a pixel
    float odd = mod(thickness, 2.0);                        // alignment is different if the line thickness is odd or even
    float line = round(v * n) / n;                          // find exactly where the line is supposed to be, in continuous space (in [0;1])
    float r = (round(line * (res + odd)) - odd/2.0) / res;  // snap the line to the closest pixel center (if odd thickness), or between 2 pixels if even
    float s = sign(v - r);                                  // defines the direction from current point to the line (s<0 means v left to r, s>0 means v right to r)
    float z = r + s * thickness * px / 2.0;                 // z is the point where the intensity goes back to 0
    return step(s * (v - z), 0.0);                          // when v is between r and z (included), the intensity is 1.0, 0.0 outside
}

float draw_grid(vec2 pos, vec2 dim, float thickness, vec2 res)
{
    float cols = draw_lines(pos.x, dim.x, thickness, res.x);
    float rows = draw_lines(pos.y, dim.y, thickness, res.y);
    return min(cols + rows, 1.0);
}
'''

def _render(frag, **frag_params):
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=_VERT, fragment=frag)
    program.update_vert_out_vars(uv=ngl.IOVec2())
    render = ngl.Render(quad, program)
    render.update_frag_resources(**frag_params)
    return render


@scene()
def grid(cfg):
    cfg.aspect_ratio = (1, 1)
    frag = _GRID_HELPERS + '''
#define USE_FRAGCOORD 0
#define DEBUG_LINES 0

vec4 pretty_grid(vec2 pos, vec2 res)
{
    float g0 = draw_grid(pos, vec2(20.0, 20.0), 1.0, res);
    float g1 = draw_grid(pos, vec2(4.0, 4.0), 2.0, res);
    float alpha = min(g0 + g1, 1.0);
    return vec4(vec3(max(g0 * 0.2, g1 * 0.5)), alpha);
}

void main()
{
    vec4 vp = ngl_resolution;
#if USE_FRAGCOORD
    vec2 pos = (gl_FragCoord.xy - vp.xy) / vp.zw;
#else
    vec2 pos = uv;
#endif

#if DEBUG_LINES
    vec3 color = mix(vec3(0.2), vec3(0.8), lines(pos.y, 10.0, 2.0, vp[3]));
#else
    vec4 grid = pretty_grid(pos, vp.zw);
    vec3 color = mix(vec3(0.1), grid.rgb, grid.a);
#endif
    ngl_out_color = vec4(color, 1.0);
}
'''
    return _render(frag)


_srgb_linear_helpers = '''
vec3 srgb_to_linear(vec3 color)
{
    return mix(
        color / 12.92,
        pow((color + .055) / 1.055, vec3(2.4)),
        step(vec3(0.04045), color)
    );
}

vec3 linear_to_srgb(vec3 color)
{
    return mix(
        color * 12.92,
        1.055 * pow(color, vec3(1./2.4)) - .055,
        step(vec3(0.0031308), color)
    );
}
'''

_gradient_helpers = _srgb_linear_helpers + '''
vec3 gradient(vec3 tl, vec3 tr, vec3 br, vec3 bl, vec2 uv)
{
    return mix(mix(tl, tr, uv.x), mix(bl, br, uv.x), uv.y);
}

vec3 gradient_linear(vec3 tl, vec3 tr, vec3 br, vec3 bl, vec2 uv)
{
    return linear_to_srgb(
        gradient(
            srgb_to_linear(tl),
            srgb_to_linear(tr),
            srgb_to_linear(br),
            srgb_to_linear(bl),
            uv
        )
    );
}
'''

@scene(
    #c_tl=scene.Color(),
    #c_tr=scene.Color(),
    #c_br=scene.Color(),
    #c_bl=scene.Color(),
    linear=scene.Bool(),
)
def gradient(cfg,
    #c_tl=COLORS.cyan,
    #c_tr=COLORS.magenta,
    #c_br=COLORS.yellow,
    #c_bl=COLORS.black,
    c_tl=(1., .5, 0., 1.),
    c_tr=(0., 1., 0., 1.),
    c_br=(0., .5, 1., 1.),
    c_bl=(1., 0., 1., 1.),
    linear=True,
):
    frag = _gradient_helpers + '''
void main()
{
    ngl_out_color = vec4(
        linear ? gradient_linear(c_tl.rgb, c_tr.rgb, c_br.rgb, c_bl.rgb, uv)
               : gradient(c_tl.rgb, c_tr.rgb, c_br.rgb, c_bl.rgb, uv),
        1.0);
}
'''
    return _render(
        frag,
        c_bl=ngl.UniformVec4(c_bl),
        c_br=ngl.UniformVec4(c_br),
        c_tr=ngl.UniformVec4(c_tr),
        c_tl=ngl.UniformVec4(c_tl),
        linear=ngl.UniformBool(linear),
    )

@scene(linear=scene.Bool())
def gradient_anim(cfg, linear=True):
    frag = _gradient_helpers + '''
#define PI 3.14159265358979323846

float color_comp_value(float t, float off)
{
    return cos((t*2.0+off)*PI)*.5+.5;
}

void main()
{
    vec3 tl = vec3(
        color_comp_value(time, 0.0/3.0 + 0.0/4.0),
        color_comp_value(time, 1.0/3.0 + 0.0/4.0),
        color_comp_value(time, 2.0/3.0 + 0.0/4.0)
    );
    vec3 tr = vec3(
        color_comp_value(time, 0.0/3.0 + 1.0/4.0),
        color_comp_value(time, 1.0/3.0 + 1.0/4.0),
        color_comp_value(time, 2.0/3.0 + 1.0/4.0)
    );
    vec3 br = vec3(
        color_comp_value(time, 0.0/3.0 + 2.0/4.0),
        color_comp_value(time, 1.0/3.0 + 2.0/4.0),
        color_comp_value(time, 2.0/3.0 + 2.0/4.0)
    );
    vec3 bl = vec3(
        color_comp_value(time, 0.0/3.0 + 3.0/4.0),
        color_comp_value(time, 1.0/3.0 + 3.0/4.0),
        color_comp_value(time, 2.0/3.0 + 3.0/4.0)
    );

    ngl_out_color = vec4(
        linear ? gradient_linear(tl, tr, br, bl, uv)
               : gradient(tl, tr, br, bl, uv),
        1.0);
}
'''

    cfg.duration = 3.0
    return _render(
        frag,
        time=ngl.AnimatedFloat(keyframes=(
            ngl.AnimKeyFrameFloat(0, 0),
            ngl.AnimKeyFrameFloat(cfg.duration, 1),
        )),
        linear=ngl.UniformBool(linear),
    )


@scene(linear=scene.Bool())
def gradient4(cfg, linear=True):
    frag = '''
#define linear2srgb(x) ((x) < 0.0031308 ? (x)*12.92 : 1.055*pow((x),1.0/2.4)-0.055)
#define srgb2linear(x) ((x) < 0.04045   ? (x)/12.92 : pow((x+.055)/1.055, 2.4))

#define linear_clr(x) vec3(srgb2linear((x).r), srgb2linear((x).g), srgb2linear((x).b))
#define srgb_clr(x)   vec3(linear2srgb((x).r), linear2srgb((x).g), linear2srgb((x).b))

#define PI 3.14159265358979323846

void main()
{
    vec2 p = uv*2.0-1.0;

#if 1
    vec2 a = sin(time * vec2( 0.307,-0.190) + vec2( 0.703,-0.957));
    vec2 b = sin(time * vec2(-0.236, 0.218) + vec2(-0.851,-0.904));
    vec2 c = sin(time * vec2(-0.615,-0.165) + vec2(-0.162,-0.647));
    vec2 d = sin(time * vec2( 0.085,-0.654) + vec2( 0.163, 0.494));
#else
    vec2 a = vec2(-1,-1);
    vec2 b = vec2( 1,-1);
    vec2 c = vec2(-1, 1);
    vec2 d = vec2( 1, 1);
#endif

    vec2 pa = p - a;
    vec2 pb = p - b;
    vec2 pc = p - c;
    vec2 pd = p - d;

    vec3 c0 = vec3(1., .5, 0.);
    vec3 c1 = vec3(0., 1., 0.);
    vec3 c2 = vec3(1., 0., 1.);
    vec3 c3 = vec3(0., .5, 1.);

#if 1
    float pw = 3.0;
    float da = pow(1./length(pa), pw);
    float db = pow(1./length(pb), pw);
    float dc = pow(1./length(pc), pw);
    float dd = pow(1./length(pd), pw);
    float nf = da + db + dc + dd;
    vec3 clr = da/nf*linear_clr(c0)
             + db/nf*linear_clr(c1)
             + dc/nf*linear_clr(c2)
             + dd/nf*linear_clr(c3);
    clr = srgb_clr(clr);


#elif 1
#define det(a,b) ((a).x*(b).y - (a).y*(b).x)

#define orient(a,b,c) (((b).y - (a).y)*((c).x - (b).x) - ((c).y - (b).y)*((b).x - (a).x))
#define swap2(a,b) vec2 x = a; a = b; b = x
#define swap3(a,b) vec3 y = a; a = b; b = y

    float o_abc = orient(a,b,c);
    float o_acd = orient(a,c,d);
    float o_abd = orient(a,b,d);

    if (o_abc < 0.0) {
        if (o_acd < 0.0) {
            if (o_abd < 0.0) {
                swap2(b, d);
                //swap3(c1, c3);
            }
        } else {
            if (o_abd < 0.0) {
                swap2(a, b);
                //swap3(c0, c1);
            } else {
                swap2(b, c);
                //swap3(c1, c2);
            }
        }
    } else {
        if (o_acd < 0.0) {
            if (o_abd < 0.0) {
                swap2(a, d);
                //swap3(c0, c3);
            } else {
                swap2(c, d);
                //swap3(c2, c3);
            }
        }
    }

    vec2 sa = a - p;
    vec2 sb = b - p;
    vec2 sc = c - p;
    vec2 sd = d - p;
    vec4 r = vec4(length(sa), length(sb), length(sc), length(sd));
    vec4 A = vec4(det(sa, sb), det(sb, sc), det(sc, sd), det(sd, sa));
    vec4 D = vec4(dot(sa, sb), dot(sb, sc), dot(sc, sd), dot(sd, sa));
    float w0 = (r[3] - D[3]/r[0])/A[3] + (r[1] - D[0]/r[0])/A[0];
    float w1 = (r[0] - D[0]/r[1])/A[0] + (r[2] - D[1]/r[1])/A[1];
    float w2 = (r[1] - D[1]/r[2])/A[1] + (r[3] - D[2]/r[2])/A[2];
    float w3 = (r[2] - D[2]/r[3])/A[2] + (r[0] - D[3]/r[3])/A[3];
    vec3 clr = w0 * linear_clr(c0)
             + w1 * linear_clr(c1)
             + w2 * linear_clr(c2)
             + w3 * linear_clr(c3);
    clr /= (w0 + w1 + w2 + w3);

    clr = srgb_clr(clr);

#elif 0
    float la = 1.-length(pa)/(4./s);
    float lb = 1.-length(pb)/(4./s);
    float lc = 1.-length(pc)/(4./s);
    float ld = 1.-length(pd)/(4./s);

    vec3 clr = la*linear_clr(c0)
             + lb*linear_clr(c1)
             + lc*linear_clr(c2)
             + ld*linear_clr(c3);
    clr /= 4;

    clr = srgb_clr(clr);
#else
    vec3 clr = smoothstep(.15, .13, length(pa))*c0
             + smoothstep(.15, .13, length(pb))*c1
             + smoothstep(.15, .13, length(pc))*c2
             + smoothstep(.15, .13, length(pd))*c3;
#endif

    ngl_out_color = vec4(clr, 1.0);
}
'''

    cfg.duration = 15.0
    return _render(
        frag,
        #time=ngl.Time(),
        time=ngl.AnimatedFloat(keyframes=(
            ngl.AnimKeyFrameFloat(0, 0),
            ngl.AnimKeyFrameFloat(cfg.duration, cfg.duration*3.0),
        )),
        linear=ngl.UniformBool(linear),
    )




@scene(
    a=scene.Range(range=(-1, 1), unit_base=100),
    b=scene.Range(range=(-1, 1), unit_base=100),
    c=scene.Range(range=(-1, 1), unit_base=100),
    enable_f0=scene.Bool(),
    f0=scene.Text(),
    enable_f1=scene.Bool(),
    f1=scene.Text(),
    enable_f2=scene.Bool(),
    f2=scene.Text(),
    enable_f3=scene.Bool(),
    f3=scene.Text(),
    enable_f4=scene.Bool(),
    f4=scene.Text(),
    enable_f5=scene.Bool(),
    f5=scene.Text(),
)
def graphtoy(cfg,
    a=0.0, b=0.0, c=0.0,
    f0="4.0 + 4.0*smoothstep(0.0,0.7,sin(x+t))", enable_f0=True,
    f1="sqrt(sq(9.0)-sq(x))", enable_f1=True,
    f2="3.0*sin(x)/x", enable_f2=True,
    f3="2.0*cos(3.0*x+t)+f2(x,t)", enable_f3=True,
    f4="(t + floor(x-t))/2.0 - 5.0", enable_f4=True,
    f5="sin(f4(x,t)) - 5.0", enable_f5=True,
):
    cfg.aspect_ratio = (1, 1)
    frag = _GRID_HELPERS + '''
#define PI 3.14159265358979323846
#define sq(x) ((x)*(x))

const float eps = 0.000001;
const float thickness = 0.02;
const vec3 bg = vec3(0.2) / 6.0;

float f0(float x, float t) { return F0_EXPR; }
float f1(float x, float t) { return F1_EXPR; }
float f2(float x, float t) { return F2_EXPR; }
float f3(float x, float t) { return F3_EXPR; }
float f4(float x, float t) { return F4_EXPR; }
float f5(float x, float t) { return F5_EXPR; }

float df(float fxp, float fxn)
{
    return (fxn - fxp) / (2.0 * eps);
}

float curve(float y, float fx, float fxp, float fxn)
{
    vec4 vp = ngl_resolution;
    float factor = vp.z / vp.w;
    float dx = df(fxp, fxn) / factor;
    float de = abs(fx - y) / sqrt(1.0 + dx*dx);
    return step(de, thickness);
}

vec4 colored_curves(vec2 p)
{
    return mix(vec4(0.0), c0, curve(p.y, f0(p.x, time), f0(p.x-eps, time), f0(p.x+eps, time)))
         + mix(vec4(0.0), c1, curve(p.y, f1(p.x, time), f1(p.x-eps, time), f1(p.x+eps, time)))
         + mix(vec4(0.0), c2, curve(p.y, f2(p.x, time), f2(p.x-eps, time), f2(p.x+eps, time)))
         + mix(vec4(0.0), c3, curve(p.y, f3(p.x, time), f3(p.x-eps, time), f3(p.x+eps, time)))
         + mix(vec4(0.0), c4, curve(p.y, f4(p.x, time), f4(p.x-eps, time), f4(p.x+eps, time)))
         + mix(vec4(0.0), c5, curve(p.y, f5(p.x, time), f5(p.x-eps, time), f5(p.x+eps, time)));
}

vec4 pretty_grid(vec2 pos, vec2 res)
{
    float g0 = draw_grid(pos, vec2(20.0, 20.0), 1.0, res);
    float g1 = draw_grid(pos, vec2(4.0, 4.0), 2.0, res);
    float alpha = min(g0 + g1, 1.0);
    return vec4(vec3(max(g0 * 0.2, g1 * 0.5)), alpha);
}

void main()
{
    vec4 vp = ngl_resolution;
    vec2 pos = vec2(uv.x, 1.0 - uv.y) * 16.0 - 8.0;

    vec4 grid = pretty_grid(uv, vp.zw);
    vec3 bg = mix(vec3(0.1), grid.rgb, grid.a);
    vec4 curves = colored_curves(pos);
    vec3 color = mix(bg, curves.rgb, curves.a);

    ngl_out_color = vec4(color, 1.0);
}
'''

    params = dict(
        a=ngl.UniformFloat(a),
        b=ngl.UniformFloat(b),
        c=ngl.UniformFloat(c),
    )
    funcs = (f0, f1, f2, f3, f4, f5)
    enabled_funcs = (enable_f0, enable_f1, enable_f2, enable_f3, enable_f4, enable_f5)
    for i, (f, enabled) in enumerate(zip(funcs, enabled_funcs)):
        frag = frag.replace(f'F{i}_EXPR', f)
        color = colorsys.hls_to_rgb(i / 6.0, 0.7, 1.0)
        params[f'c{i}'] = ngl.UniformVec4(color + (1.0 if enabled else 0.0,))

    return _render(
        frag,
        time=ngl.Time(),
        noise_signal=ngl.NoiseFloat(),
        **params,
    )
