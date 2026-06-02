/**
 * From the OpenGL Programming wikibook: http://en.wikibooks.org/wiki/OpenGL_Programming
 * This file is in the public domain.
 * Contributors: Sylvain Beucler
 */
#ifdef SDL2_RENDER

#ifndef _CREATE_SHADER_H
#define _CREATE_SHADER_H
#include "SDL2_helper.h"
//#include <GL/glew.h>

extern void print_log(GLuint object);
GLuint load_shader_file(const char* filepath, GLenum type);
// enableMultiView: when nonzero, inject GL_OVR_multiview2 + MULTIVIEW_ENABLED so the shader
// can render single-pass stereo to the multiview array FBO. Only the 3D-scene shaders that
// target that FBO (default/crosshair/vignette) should set this; UI/menu shaders render to
// plain 2D FBOs and must stay mono (a num_views=2 shader on a non-array FBO is invalid).
extern GLuint create_shader(const char* shaderSource, GLenum type, int enableMultiView);

#endif
#endif // SDL2_RENDER
