#ifdef MULTIVIEW_ENABLED
layout(num_views = 2) in;
layout(std140) uniform ViewMatrices {
    mat4 u_viewMatrices[2];
};
layout(std140) uniform ProjMatrices {
    mat4 u_projMatrices[2];
};
#endif

in vec3 coord3d;
in vec4 v_color;
uniform mat4 mvp;
out vec4 f_color;

void main(void)
{
#ifdef MULTIVIEW_ENABLED
    // MultiView: use per-eye view/projection from UBOs
    gl_Position = u_projMatrices[gl_ViewID_OVR] * u_viewMatrices[gl_ViewID_OVR] * vec4(coord3d, 1.0);
#else
    // Per-eye: MVP is pre-multiplied on CPU
    gl_Position = mvp * vec4(coord3d, 1.0);
#endif
    f_color = v_color;
}
