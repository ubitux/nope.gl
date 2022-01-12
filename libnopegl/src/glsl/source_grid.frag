float _draw_lines(float v, float n, float thickness, float res)
{
    if (res == 0.0)
        return 0.0;
    float odd = mod(thickness, 2.0);                        // alignment is different if the line thickness is odd or even
    float line = round(v * n) / n;                          // find exactly where the line is supposed to be, in continuous space (in [0;1])
    float r = (round(line * (res + odd)) - odd/2.0) / res;  // snap the line to the closest pixel center (if odd thickness), or between 2 pixels if even
    float s = sign(v - r);                                  // defines the direction from current point to the line (s<0 means v left to r, s>0 means v right to r)
    float z = r + s * thickness / res / 2.0;                // z is the point where the intensity goes back to 0
    return step(s * (v - z), 0.0);                          // when v is between r and z (included), the intensity is 1.0, 0.0 outside
}

float _draw_grid(vec2 pos, vec2 dim, float thickness, vec2 res)
{
    float cols = _draw_lines(pos.x, dim.x, thickness, res.x);
    float rows = _draw_lines(pos.y, dim.y, thickness, res.y);
    return min(cols + rows, 1.0);
}

#define USE_FRAGCOORD 0

vec4 source_grid()
{
#if USE_FRAGCOORD
    vec2 pos = (gl_FragCoord.xy - vp.xy) / vp.zw;
#else
    vec2 pos = uv;
#endif
    //pos = pos * 2.0 - 1.0;

    float grid = _draw_grid(pos, dimensions, thickness, resolution);
    return vec4(color, 1.0) * opacity * grid;
}
