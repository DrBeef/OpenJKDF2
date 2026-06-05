/**
 * From the OpenGL Programming wikibook: http://en.wikibooks.org/wiki/OpenGL_Programming
 * This file is in the public domain.
 * Contributors: Sylvain Beucler
 */

#ifdef SDL2_RENDER

#include "shader_utils.h"
#include "globals.h"

#include "SDL2_helper.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "stdPlatform.h"

#ifdef LINUX
#include "external/fcaseopen/fcaseopen.h"
#endif

#include "Platform/Common/stdEmbeddedRes.h"

/**
 * Display compilation errors from the OpenGL shader compiler
 */
void print_log(GLuint object) {
	GLint log_length = 0;
	if (glIsShader(object)) {
		glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
	} else if (glIsProgram(object)) {
		glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
	} else {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
					   "printlog: Not a shader or a program");
		return;
	}

	char* log = (char*)malloc(log_length);
	
	if (glIsShader(object))
		glGetShaderInfoLog(object, log_length, NULL, log);
	else if (glIsProgram(object))
		glGetProgramInfoLog(object, log_length, NULL, log);
	
	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, "%s\n", log);
	
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", log, NULL);
	
	free(log);
}

// When nonzero, force the next-loaded shader(s) to be compiled with MultiView enabled
// (used to load a multiview UI program for baking the HUD into the eye buffer).
int g_shaderForceMultiView = 0;

GLuint load_shader_file(const char* filepath, GLenum type)
{
    stdPlatform_Printf("std3D: Loading shader file: %s\n", filepath);
    char* shader_contents = stdEmbeddedRes_Load(filepath, NULL);
    stdPlatform_Printf("std3D: stdEmbeddedRes_Load returned %p\n", (void*)shader_contents);

    if (!shader_contents)
    {
    	char errtmp[256];
        snprintf(errtmp, 256, "std3D: Failed to load shader file `%s`!\n", filepath);
        stdPlatform_Printf("std3D: %s\n", errtmp);
        // Don't show message box on Android VR - it blocks
#ifndef TARGET_ANDROID
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", errtmp, NULL);
#endif
        return -1;
    }

    stdPlatform_Printf("std3D: Parse shader `%s`\n", filepath);

    // Only the 3D-scene shaders that render into the MultiView array FBO may declare
    // num_views=2 / use gl_ViewID_OVR. UI and menu shaders render to plain single-layer 2D
    // FBOs (HUD quad layer, menus), where a multiview shader is invalid on strict desktop
    // GL drivers (it renders nothing). Restrict the multiview injection accordingly.
    // g_shaderForceMultiView lets the caller load a multiview variant of an otherwise-mono
    // shader (used for the multiview UI program that bakes the HUD into the eye buffer).
    extern int g_shaderForceMultiView;
    int enableMultiView =
        g_shaderForceMultiView ||
        (strstr(filepath, "default") != NULL) ||
        (strstr(filepath, "crosshair") != NULL) ||
        (strstr(filepath, "vignette") != NULL);

    GLuint ret = create_shader(shader_contents, type, enableMultiView);
    stdPlatform_Printf("std3D: Shader compiled, result=%u\n", ret);
    free(shader_contents);

    return ret;
}

/**
 * Compile the shader from file 'filename', with error handling
 */
GLuint create_shader(const char* shader, GLenum type, int enableMultiView) {
	const GLchar* source = (const GLchar*)shader;
	(void)enableMultiView;  // Unused on non-VR / Android / WASM builds
	GLuint res = glCreateShader(type);

	// GLSL version
	const char* version = "";
	int profile;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);

	const char* extensions = "\n";
	const char* defines = "\n";
	//if (profile == SDL_GL_CONTEXT_PROFILE_ES)
	//	version = "#version 100\n";  // OpenGL ES 2.0
	//else
    //version = "#version 330 core\n";  // OpenGL 3.3
#ifdef MACOS
	version = "#version 330\n";
	extensions = "#extension GL_ARB_texture_gather : enable\n";
	defines = "#define CAN_BILINEAR_FILTER\n#define HAS_MIPS\n";
#else
    version = "#version 330\n";  // OpenGL ES 2.0
    extensions = "#extension GL_ARB_texture_gather : enable\n";
    defines = "#define CAN_BILINEAR_FILTER\n#define HAS_MIPS\n";
#endif

#if defined(WIN64_STANDALONE)
    version = "#version 330\n";
    extensions = "#extension GL_ARB_texture_gather : enable\n";
    defines = "#define CAN_BILINEAR_FILTER\n#define HAS_MIPS\n";
#endif

#if defined(ARCH_WASM)
    version = "#version 300 es\n";
    extensions = "\n";
    defines = "#define CAN_BILINEAR_FILTER\n";
#endif

#if defined(TARGET_ANDROID)
    version = "#version 300 es\n";
#if defined(PLATFORM_VR)
    // RazeXR-parity multiview injection (works on Pico AND Quest Adreno):
    //  - Enable GL_OVR_multiview2 ONLY in the VERTEX stage of shaders that actually use it.
    //    A vertex-only extension declared in the fragment stage is a divergence from the
    //    reference and risks strict Adreno linkers.
    //  - Define MULTIVIEW_ENABLED only for multiview shaders (so the mono menu/ui programs and
    //    the post-process shaders compile genuinely single-view).
    //  - Keep CAN_BILINEAR_FILTER + the SHADER_SESSION_ID cache-buster on ALL shaders (the
    //    session id forces recompiles, bypassing Pico's shader-cache bug).
    // The MultiView extension declaration must come BEFORE any other (non-#version) code.
    static uint32_t shaderSessionId = 0;
    static char definesBuffer[256];
    if (shaderSessionId == 0) {
        shaderSessionId = (uint32_t)time(NULL);
    }
    if (enableMultiView) {
        extensions = (type == GL_VERTEX_SHADER)
            ? "#extension GL_OVR_multiview2 : enable\n"
            : "\n";
        snprintf(definesBuffer, sizeof(definesBuffer),
            "#define CAN_BILINEAR_FILTER\n#define MULTIVIEW_ENABLED\n#define SHADER_SESSION_ID %u\n",
            shaderSessionId);
    } else {
        extensions = "\n";
        snprintf(definesBuffer, sizeof(definesBuffer),
            "#define CAN_BILINEAR_FILTER\n#define SHADER_SESSION_ID %u\n",
            shaderSessionId);
    }
    defines = definesBuffer;
#else
    extensions = "\n";
    defines = "#define CAN_BILINEAR_FILTER\n";
#endif
#endif

#if defined(PLATFORM_VR) && !defined(TARGET_ANDROID) && !defined(ARCH_WASM)
    // Desktop PCVR: enable single-pass MultiView stereo, but ONLY for the 3D-scene shaders
    // that render into the multiview array FBO (enableMultiView). default_v.glsl (and the
    // crosshair/vignette shaders) branch on MULTIVIEW_ENABLED and use
    // layout(num_views=2)/gl_ViewID_OVR, which require the GL_OVR_multiview2 extension and a
    // multiview FBO. UI/menu shaders render to plain 2D FBOs (HUD quad layer, menus) and
    // must stay mono, so they keep the non-multiview extensions/defines set above.
    // Version stays #version 330.
    if (enableMultiView) {
        extensions = "#extension GL_ARB_texture_gather : enable\n#extension GL_OVR_multiview2 : enable\n";
        defines = "#define CAN_BILINEAR_FILTER\n#define HAS_MIPS\n#define MULTIVIEW_ENABLED\n";
    }
#endif

	// GLES2 precision specifiers
	const char* precision;
	precision =
		"#ifdef GL_ES                        \n"
		"#  ifdef GL_FRAGMENT_PRECISION_HIGH \n"
		"     precision highp float;         \n"
		"#  else                             \n"
		"     precision mediump float;       \n"
		"#  endif                            \n"
		"#else                               \n"
		// Ignore unsupported precision specifiers
		"#  define lowp                      \n"
		"#  define mediump                   \n"
		"#  define highp                     \n"
		"#endif                              \n";

	const GLchar* sources[] = {
		version,
		extensions,
		defines,
		precision,
		source
	};
	glShaderSource(res, 5, sources, NULL);
	
	glCompileShader(res);
	GLint compile_ok = GL_FALSE;
	glGetShaderiv(res, GL_COMPILE_STATUS, &compile_ok);
	if (compile_ok == GL_FALSE) {
		print_log(res);
		glDeleteShader(res);
		return 0;
	}
	
	return res;
}
#endif // LINUX
