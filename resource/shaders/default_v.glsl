#ifdef MULTIVIEW_ENABLED
#define NUM_VIEWS 2
layout(num_views = NUM_VIEWS) in;

// Per-eye view matrices (like Doom3Quest)
layout(std140) uniform ViewMatrices {
    mat4 u_viewMatrices[NUM_VIEWS];
};
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

#ifdef MULTIVIEW_ENABLED
    // Save the original w from MVP - this contains depth info before custom projection
    float mvpDepth = pos.w;
#endif

    // Custom perspective correction (game's coordinate system)
    pos.w = 1.0/(1.0-coord3d.z);
    pos.xyz *= pos.w;

#ifdef MULTIVIEW_ENABLED
    // Apply stereo parallax using MVP's original depth
    // Parallax should be inversely proportional to depth (near=more, far=less)
    // Use direct array indexing (not if-else) to avoid tiled GPU issues on Pico
    float eyeOffset = u_viewMatrices[gl_ViewID_OVR][3][0];
    // Divide by mvpDepth for proper parallax (larger depth = smaller offset)
    pos.x += eyeOffset / (mvpDepth + 0.0001);
#endif

    gl_Position = pos;
    f_color = v_color.bgra;
    f_uv = v_uv;
    f_coord = coord3d;
    f_light = v_light;
}
