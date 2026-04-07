#ifdef MULTIVIEW_ENABLED
layout(num_views = 2) in;
#endif

in vec3 coord3d;
in vec4 v_color;
uniform mat4 mvp;
out vec4 f_color;

void main(void)
{
    gl_Position = mvp * vec4(coord3d, 1.0);
    f_color = v_color;
}
