// MultiView vertex shader for single-pass stereo rendering (Quest VR)
// Uses GL_OVR_multiview2 extension to render both eyes in a single draw call

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

// Input vertex attributes
in vec3 coord3d;
in vec4 v_color;
in float v_light;
in vec2 v_uv;

// Output to fragment shader
out vec4 f_color;
out float f_light;
out vec2 f_uv;
out vec3 f_coord;

// View and projection matrices for both eyes (accessed via uniform buffers)
layout(std140) uniform ViewMatrices {
    mat4 u_viewMatrix[2];
};
layout(std140) uniform ProjectionMatrices {
    mat4 u_projMatrix[2];
};

// Model matrix (shared for both eyes)
uniform mat4 u_modelMatrix;

void main(void)
{
    // Select the view/projection matrix for the current eye
    int viewID = int(gl_ViewID_OVR);
    mat4 viewMatrix = u_viewMatrix[viewID];
    mat4 projMatrix = u_projMatrix[viewID];

    // Transform vertex to clip space
    // Note: The original shader uses a custom perspective transformation:
    //   pos.w = 1.0/(1.0-coord3d.z);
    //   pos.xyz *= pos.w;
    // This is for the game's software renderer compatibility.
    // For MultiView, we use standard MVP transformation.

    vec4 worldPos = u_modelMatrix * vec4(coord3d, 1.0);
    vec4 viewPos = viewMatrix * worldPos;
    vec4 clipPos = projMatrix * viewPos;

    // Apply the same custom w transformation as the original shader
    // This maintains compatibility with the game's coordinate system
    clipPos.w = 1.0 / (1.0 - coord3d.z);
    clipPos.xyz *= clipPos.w;

    gl_Position = clipPos;
    f_color = v_color.bgra;
    f_uv = v_uv;
    f_coord = coord3d;
    f_light = v_light;
}
