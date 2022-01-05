vec4 filter_liftgammagain(vec4 color, vec2 coords,
                          vec3 lift, vec3 gamma, vec3 gain,
                          float lift_off, float gamma_off, float gain_off)
{
#if 0
    lift  = lift  - (lift.r  + lift.g  + lift.b)  / 3.0;
    gamma = gamma - (gamma.r + gamma.g + gamma.b) / 3.0;
    gain  = gain  - (gain.r  + gain.g  + gain.b)  / 3.0;
    vec3 lift_adjust = 0.0 + (lift + lift_off);
    vec3 gain_adjust = 1.0 + (gain + gain_off);
    vec3 mid_grey = 0.5 + gamma + gamma_off;
    vec3 gamma_adjust = log((0.5 - lift_adjust) / (gain_adjust - lift_adjust)) / log(mid_grey);
    color.rgb = clamp(pow(color.rgb, 1.0 / gamma_adjust), 0.0, 1.0);
    color.rgb = mix(lift_adjust, gain_adjust, color.rgb);
#else
    color.rgb = ngli_srgb2linear(mix(lift, vec3(1.0), color.rgb));
    color.rgb = color.rgb * (gain + 1.0);
    color.rgb = pow(color.rgb, 1.0 / (gamma + 1.0));
    color.rgb = ngli_linear2srgb(color.rgb);
#endif

    return color;
}
