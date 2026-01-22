#ifndef _OPENJKDF2_SDL2_HELPER_H
#define _OPENJKDF2_SDL2_HELPER_H

#ifdef SDL2_RENDER
//#ifndef ARCH_WASM

#ifdef MACOS
#define GL_SILENCE_DEPRECATION
#include <SDL.h>
#include <GL/glew.h>
#include <OpenGL/gl.h>
#elif defined(ARCH_WASM)

// emscripten.h doesn't like extern C
#ifdef __cplusplus
//}
#endif
#include <emscripten.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengles2.h>
#include <GLES3/gl3.h>
#include <GLES3/gl2ext.h>

//HACK
#define GL_UNSIGNED_SHORT_5_6_5_REV       0x8364
#define GL_UNSIGNED_SHORT_1_5_5_5_REV     0x8366

// emscripten.h doesn't like extern C
#ifdef __cplusplus
}
#endif
#elif defined(TARGET_ANDROID)
#include <SDL.h>
#include <SDL_main.h>
#include <android/log.h>

#define TAG "OpenJKDF2"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,    TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,     TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,     TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,    TAG, __VA_ARGS__)

#if defined(TARGET_ANDROID_NATIVE_GLES)
// Native GLES3 for Quest VR - no gl4es translation layer
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2ext.h>

// Compatibility defines for formats not in GLES
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5_REV
#define GL_UNSIGNED_SHORT_5_6_5_REV       0x8364
#endif
#ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
#define GL_UNSIGNED_SHORT_1_5_5_5_REV     0x8366
#endif

#else // GL4ES for standard Android

// gl4es provides standard GL headers that translate to GLES
#include <GL/gl.h>
#include <GL/glext.h>

// gl4es exports these functions but doesn't declare them in headers
// Framebuffer object functions
extern void glGenFramebuffers(GLsizei n, GLuint *framebuffers);
extern void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);
extern void glBindFramebuffer(GLenum target, GLuint framebuffer);
extern void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern GLenum glCheckFramebufferStatus(GLenum target);
extern void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers);
extern void glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers);
extern void glBindRenderbuffer(GLenum target, GLuint renderbuffer);
extern void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
extern void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
// Shader functions
extern GLuint glCreateShader(GLenum type);
extern void glDeleteShader(GLuint shader);
extern GLboolean glIsShader(GLuint shader);
extern GLboolean glIsProgram(GLuint program);
extern void glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
extern void glCompileShader(GLuint shader);
extern void glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
extern void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
extern GLuint glCreateProgram(void);
extern void glDeleteProgram(GLuint program);
extern void glAttachShader(GLuint program, GLuint shader);
extern void glLinkProgram(GLuint program);
extern void glGetProgramiv(GLuint program, GLenum pname, GLint *params);
extern void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
extern void glUseProgram(GLuint program);
extern GLint glGetUniformLocation(GLuint program, const GLchar *name);
extern GLint glGetAttribLocation(GLuint program, const GLchar *name);
extern void glUniform1i(GLint location, GLint v0);
extern void glUniform1f(GLint location, GLfloat v0);
extern void glUniform2f(GLint location, GLfloat v0, GLfloat v1);
extern void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
extern void glEnableVertexAttribArray(GLuint index);
extern void glDisableVertexAttribArray(GLuint index);
// VBO functions
extern void glGenBuffers(GLsizei n, GLuint *buffers);
extern void glDeleteBuffers(GLsizei n, const GLuint *buffers);
extern void glBindBuffer(GLenum target, GLuint buffer);
extern void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
extern void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
extern void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params);
// VAO functions
extern void glGenVertexArrays(GLsizei n, GLuint *arrays);
extern void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);
extern void glBindVertexArray(GLuint array);
// MRT functions
extern void glDrawBuffers(GLsizei n, const GLenum *bufs);
// Texture functions
extern void glActiveTexture(GLenum texture);
extern void glGenerateMipmap(GLenum target);
// Blend functions
extern void glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
extern void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);

// gl4es should provide these, but define if not
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif

#ifndef GL_UNSIGNED_SHORT_5_6_5_REV
#define GL_UNSIGNED_SHORT_5_6_5_REV       0x8364
#endif

#ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
#define GL_UNSIGNED_SHORT_1_5_5_5_REV     0x8366
#endif

#endif // TARGET_ANDROID_NATIVE_GLES

#else
#include <GL/glew.h>
#include <SDL.h>
#include <GL/gl.h>
#endif // MACOS ... else

#ifdef WIN32
#define GL_R8 GL_RED
#endif // WIN32

//#endif // !ARCH_WASM
#endif // SDL2_RENDER




#endif // _OPENJKDF2_SDL2_HELPER_H