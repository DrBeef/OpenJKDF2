#ifdef MULTIVIEW_ENABLED
layout(num_views = 2) in;
// Per-eye HUD horizontal shift (native-eye NDC): forward-centering + depth convergence. The
// HUD is authored directly in native-eye NDC, so NO scale is applied (that would stretch its
// width and break aspect); just shift it per eye so it fuses at a comfortable depth, centred
// on the binocular straight-ahead.
uniform float u_vrHudOffset[2];
#endif

in vec3 coord3d;
in vec4 v_color;
in vec2 v_uv;
uniform mat4 mvp;
out vec4 f_color;
out vec2 f_uv;

void main(void)
{
    vec4 pos = mvp * vec4(coord3d, 1.0);
    pos.w = 1.0/(1.0-coord3d.z);
    pos.xyz *= pos.w;
#ifdef MULTIVIEW_ENABLED
    pos.x += u_vrHudOffset[gl_ViewID_OVR] * pos.w;
#endif
    gl_Position = pos;
    f_color = v_color;
    f_uv = v_uv;
}
