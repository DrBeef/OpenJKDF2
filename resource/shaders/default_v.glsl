#ifdef MULTIVIEW_ENABLED
#define NUM_VIEWS 2
layout(num_views = NUM_VIEWS) in;

// Per-eye view matrices (like Doom3Quest). [3][0] holds the IPD parallax coefficient.
layout(std140) uniform ViewMatrices {
    mat4 u_viewMatrices[NUM_VIEWS];
};
// Per-eye asymmetric-frustum remap: .x = horizontal scale, .y = horizontal offset. Maps the
// combined-frustum render into this eye's NATIVE asymmetric projection. (1,0) = no remap.
uniform vec2 u_vrEyeRemap[NUM_VIEWS];
#endif

in vec3 coord3d;
in vec4 v_color;
in float v_light;
in vec2 v_uv;

uniform mat4 mvp;

out vec4 f_color;
out float f_light;
out vec2 f_uv;
out vec3 f_coord;

void main(void)
{
    // Standard MVP transform
    vec4 pos = mvp * vec4(coord3d, 1.0);

    // Custom perspective correction (game's coordinate system)
    pos.w = 1.0/(1.0-coord3d.z);
    pos.xyz *= pos.w;

#ifdef MULTIVIEW_ENABLED
    // Per-eye asymmetric-frustum remap + stereo parallax. The scene is CPU-projected once to the
    // COMBINED frustum (pos in homogeneous clip space, pos.x = ndc_combined*pos.w). Remap to this
    // eye's NATIVE asymmetric frustum and add the IPD convergence parallax:
    //     ndc_eye = scale * (ndc_combined + parallax*(1-z)) + offset
    // -> in homogeneous space (since (1-z)*pos.w == 1):
    //     pos.x = scale * (pos.x + parallax) + offset * pos.w
    // The submit uses each eye's NATIVE per-eye FOV + per-eye pose to match this (required for
    // correct convergence on Oculus). For a symmetric headset scale=1/offset=0. Direct array
    // indexing (not if-else) avoids tiled-GPU issues on Pico.
    float scale_e    = u_vrEyeRemap[gl_ViewID_OVR].x;
    float offset_e   = u_vrEyeRemap[gl_ViewID_OVR].y;
    float parallax_e = u_viewMatrices[gl_ViewID_OVR][3][0];
    pos.x = scale_e * (pos.x + parallax_e) + offset_e * pos.w;
#endif

    gl_Position = pos;
    f_color = v_color.bgra;
    f_uv = v_uv;
    f_coord = coord3d;
    f_light = v_light;
}
