#ifdef MULTIVIEW_ENABLED
layout(num_views = 2) in;
// Per-eye HUD horizontal shift (native-eye NDC): forward-centering + depth convergence. The
// 2D UI is authored directly in native-eye NDC, so NO scale is applied (that would stretch it
// horizontally and break aspect, e.g. the weapon/force wheel); just shift it per eye so it
// fuses at a comfortable depth, centred on the binocular straight-ahead.
// Altered: this MUST live in a UBO. Indexing a default-block uniform array by gl_ViewID_OVR
// crashes Pico's Adreno GLSL linker (libllvm-qgl null deref); UBO-array indexing is the
// proven-safe pattern (same as default_v.glsl / crosshair_v.glsl).
layout(std140) uniform HudOffsets {
    vec4 u_vrHudOffset[2];   // [eye].x = per-eye NDC horizontal shift
};
#endif

in vec3 coord3d;
in vec4 v_color;
in vec2 v_uv;
uniform mat4 mvp;
out vec4 f_color;
out vec2 f_uv;
out vec3 f_coord;

void main(void)
{
    vec4 pos = mvp * vec4(coord3d, 1.0);
    pos.w = 1.0/(1.0-coord3d.z);
    pos.xyz *= pos.w;
#ifdef MULTIVIEW_ENABLED
    pos.x += u_vrHudOffset[gl_ViewID_OVR].x * pos.w;
#endif
    gl_Position = pos;
    f_color = v_color;
    f_uv = v_uv;
    f_coord = coord3d;
}
