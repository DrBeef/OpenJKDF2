#ifdef MULTIVIEW_ENABLED
#define NUM_VIEWS 2
layout(num_views = NUM_VIEWS) in;

// Per-eye REAL view + projection matrices (proper OVR_multiview, same as crosshair_v.glsl).
// u_viewMatrices[eye] carries the engine->GL axis swap (A) and the per-eye pose delta relative
// to the center/HMD view the scene was rendered from. u_projMatrices[eye] is the real
// asymmetric OpenXR per-eye frustum. The GPU does the full projection per eye -> correct
// convergence and perspective on all platforms, no CPU-baked projection, no remap warp.
layout(std140) uniform ViewMatrices {
    mat4 u_viewMatrices[NUM_VIEWS];
};
layout(std140) uniform ProjectionMatrices {
    mat4 u_projMatrices[NUM_VIEWS];
};
#endif

in vec3 coord3d;
in vec4 v_color;
in float v_light;
in vec2 v_uv;

uniform mat4 mvp;
uniform vec2 iResolution;

out vec4 f_color;
out float f_light;
out vec2 f_uv;
out vec3 f_coord;

void main(void)
{
#ifdef MULTIVIEW_ENABLED
    // Proper per-eye GPU projection. coord3d is engine view-space (right, forward, up).
    vec4 pos = u_projMatrices[gl_ViewID_OVR] * u_viewMatrices[gl_ViewID_OVR] * vec4(coord3d, 1.0);

    // Reconstruct per-eye screen-space pixel coords for the SSAO position buffer / parallax
    // mapping (default_f.glsl uses f_coord.xy/iResolution; f_coord.z is unused there).
    vec2 ndc = pos.xy / pos.w;
    f_coord = vec3((ndc * 0.5 + 0.5) * iResolution, pos.z / pos.w);
#else
    // Legacy non-VR path: geometry is already CPU-projected to screen space; mvp is an ortho
    // pixels->NDC matrix and w is reconstructed for perspective-correct texturing.
    vec4 pos = mvp * vec4(coord3d, 1.0);
    pos.w = 1.0/(1.0-coord3d.z);
    pos.xyz *= pos.w;
    f_coord = coord3d;
#endif

    gl_Position = pos;
    f_color = v_color.bgra;
    f_uv = v_uv;
    f_light = v_light;
}
