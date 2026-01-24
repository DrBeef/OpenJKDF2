uniform sampler2D tex;
uniform sampler2D worldPalette;
uniform sampler2D displayPalette;
uniform int rgb_mode; // Added: 0=indexed palette, 1=direct RGB
in vec4 f_color;
in vec2 f_uv;
out vec4 fragColor;

void main(void)
{
    vec4 sampled = texture(tex, f_uv);
    vec4 sampled_color = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 vertex_color = f_color;
    vec4 blend = vec4(1.0, 1.0, 1.0, 1.0);
    float transparency = 1.0;

    // Added: RGB mode for full color video playback
    if (rgb_mode == 1) {
        // Direct RGB mode - texture contains RGB data
        if (sampled.r == 0.0 && sampled.g == 0.0 && sampled.b == 0.0)
            discard;
        sampled_color = vec4(sampled.r, sampled.g, sampled.b, transparency);
    } else {
        // Indexed palette mode (original behavior)
        float index = sampled.r;
        vec4 palval = texture(worldPalette, vec2(index, 0.5));
        vec4 palvald = texture(displayPalette, vec2(index, 0.5));

        if (index == 0.0)
            discard;
        sampled_color = vec4(palvald.r, palvald.g, palvald.b, transparency);
    }

    fragColor = sampled_color * vertex_color * blend;
}
