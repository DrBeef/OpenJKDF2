#include "stdEmbeddedRes.h"

#include "globals.h"
#include "stdPlatform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FS_POSIX
#include "external/fcaseopen/fcaseopen.h"
#endif

#ifdef SDL2_RENDER
#include "SDL2_helper.h"
#ifdef TARGET_ANDROID
#include "SDL_system.h"
#endif
#endif
//#define stdEmbeddedRes_errmsg(_msg) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", _msg, NULL)
//#else
#define stdEmbeddedRes_errmsg(_msg) stdPlatform_Printf("stdEmbeddedRes: %s\n", _msg)
//#endif

#ifdef TARGET_TWL
#include <nds.h>
#include <sys/stat.h>
#endif

char* stdEmbeddedRes_LoadOnlyInternal(const char* filepath, size_t* pOutSz)
{
#ifdef TARGET_TWL
    struct stat statstuff;
    int exists = 0;
#endif
    FILE* f = NULL;
    char* base_path = NULL;
    char* file_contents = NULL;
    char tmp_filepath[256];
    strncpy(tmp_filepath, "resource/", 256-1);
    strncat(tmp_filepath, filepath, 256-1);

    if (pOutSz) {
        *pOutSz = 0;
    }
    
#ifdef WIN32
for (int i = 0; i < strlen(tmp_filepath); i++)
{
    if (tmp_filepath[i] == '/') {
        tmp_filepath[i] = '\\';
    }
}
#endif

#ifdef TARGET_TWL
    exists = stat(tmp_filepath, &statstuff) >= 0;
    if (!exists) {
        goto skip_fopen;
    }
#endif
    
#if defined(MACOS) && defined(SDL2_RENDER)
    base_path = SDL_GetBasePath();
    strncpy(tmp_filepath, base_path, 256-1);
    strncat(tmp_filepath, "Contents/Resources/", 256-1);
    strncat(tmp_filepath, filepath, 256-1);
    SDL_free(base_path);
#endif

    f = fopen(tmp_filepath, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        size_t len = ftell(f);
        rewind(f);

        file_contents = (char*)malloc(len+1);
        if (!file_contents) {
            if (pOutSz) {
                *pOutSz = 0;
            }
            fclose(f);
            return NULL;
        }

        size_t bytes_read = fread(file_contents, 1, len, f);
        if (bytes_read == 0 && len > 0)
        {
            char errtmp[256];
            snprintf(errtmp, 256, "Failed to read file `%s`!\n", filepath);
            stdEmbeddedRes_errmsg(errtmp);
            free(file_contents);
            fclose(f);
            return NULL;
        }
        file_contents[bytes_read] = 0;

        fclose(f);

        if (pOutSz) {
            *pOutSz = bytes_read+1;
        }
        return file_contents;
    }

skip_fopen:
    strncpy(tmp_filepath, filepath, 256-1);
    
    for (int i = 0; i < strlen(tmp_filepath); i++)
    {
        if (tmp_filepath[i] == '\\') {
            tmp_filepath[i] = '/';
        }
    }

    for (size_t i = 0; i < embeddedResource_aFiles_num; i++)
    {
        if (!strcmp(embeddedResource_aFiles[i].fpath, tmp_filepath)) {
            file_contents = (char*)malloc(embeddedResource_aFiles[i].data_len+1);
            if (!file_contents) {
                if (pOutSz) {
                    *pOutSz = 0;
                }
                return NULL;
            }
            memcpy(file_contents, embeddedResource_aFiles[i].data, embeddedResource_aFiles[i].data_len);
            file_contents[embeddedResource_aFiles[i].data_len] = 0;

            if (pOutSz) {
                *pOutSz = embeddedResource_aFiles[i].data_len+1;
            }

            break;
        }
    }

    return file_contents;
}

char* stdEmbeddedRes_Load(const char* filepath, size_t* pOutSz)
{
#ifdef TARGET_TWL
    struct stat statstuff;
    int exists = 0;
#endif
    FILE* f = NULL;
    char* base_path = NULL;
    char* file_contents = NULL;
    char tmp_filepath[256];
    strncpy(tmp_filepath, "resource/", 256-1);
    strncat(tmp_filepath, filepath, 256-1);

    if (pOutSz) {
        *pOutSz = 0;
    }
    
#ifdef WIN32
for (int i = 0; i < strlen(tmp_filepath); i++)
{
    if (tmp_filepath[i] == '/') {
        tmp_filepath[i] = '\\';
    }
}
#endif

#ifdef FS_POSIX
    char *r = (char*)malloc(strlen(tmp_filepath) + 16);
    if (casepath(tmp_filepath, r))
    {
        strcpy(tmp_filepath, r);
    }
    free(r);
#endif


#ifdef TARGET_TWL
    exists = stat(tmp_filepath, &statstuff) >= 0;
    if (!exists) {
        goto skip_fopen;
    }
#endif

    f = fopen(tmp_filepath, "rb");
    if (f)
    {
retry_file:
        fseek(f, 0, SEEK_END);
        size_t len = ftell(f);
        rewind(f);

        file_contents = (char*)malloc(len+1);
        if (!file_contents) {
            if (pOutSz) {
                *pOutSz = 0;
            }
            fclose(f);
            return NULL;
        }

        size_t bytes_read = fread(file_contents, 1, len, f);
        if (bytes_read == 0 && len > 0)
        {
            char errtmp[256];
            snprintf(errtmp, 256, "Failed to read file `%s`!\n", filepath);
            stdEmbeddedRes_errmsg(errtmp);
            free(file_contents);
            fclose(f);
            return NULL;
        }
        file_contents[bytes_read] = 0; // Null terminate at actual read length

        fclose(f);

        if (pOutSz) {
            *pOutSz = bytes_read+1;
        }
    }
    else
    {
#if defined(MACOS) && defined(SDL2_RENDER)
        base_path = SDL_GetBasePath();
        strncpy(tmp_filepath, base_path, 256-1);
        strncat(tmp_filepath, "Contents/Resources/", 256-1);
        strncat(tmp_filepath, filepath, 256-1);
        SDL_free(base_path);

        f = fopen(tmp_filepath, "rb");
        if (f)
            goto retry_file;
#endif

// Added: Try executable's base path on Windows
#if defined(WIN32) && defined(SDL2_RENDER)
        base_path = SDL_GetBasePath();
        if (base_path) {
            strncpy(tmp_filepath, base_path, 256-1);
            strncat(tmp_filepath, "resource\\", 256-1);
            strncat(tmp_filepath, filepath, 256-1);
            // Convert forward slashes to backslashes
            for (int i = 0; i < strlen(tmp_filepath); i++) {
                if (tmp_filepath[i] == '/') {
                    tmp_filepath[i] = '\\';
                }
            }
            SDL_free(base_path);

            f = fopen(tmp_filepath, "rb");
            if (f)
                goto retry_file;
        }
#endif

// Added: Try Android APK assets first (using SDL_RWops for Asset Manager access)
#if defined(TARGET_ANDROID) && defined(SDL2_RENDER)
        // Try loading from APK assets - assets are at "assets/<filepath>" but SDL
        // internally looks in assets/ directory, so we just use the filepath directly
        stdPlatform_Printf("stdEmbeddedRes: Trying Android APK assets: %s\n", filepath);
        SDL_RWops* rw = SDL_RWFromFile(filepath, "rb");
        if (rw) {
            Sint64 len = SDL_RWsize(rw);
            if (len > 0) {
                file_contents = (char*)malloc((size_t)len + 1);
                if (file_contents) {
                    size_t bytes_read = SDL_RWread(rw, file_contents, 1, (size_t)len);
                    SDL_RWclose(rw);
                    if (bytes_read > 0) {
                        file_contents[bytes_read] = 0;
                        stdPlatform_Printf("stdEmbeddedRes: Loaded %zu bytes from APK assets\n", bytes_read);
                        if (pOutSz) {
                            *pOutSz = bytes_read + 1;
                        }
                        return file_contents;
                    }
                    free(file_contents);
                    file_contents = NULL;
                } else {
                    SDL_RWclose(rw);
                }
            } else {
                SDL_RWclose(rw);
            }
        }
        stdPlatform_Printf("stdEmbeddedRes: Not found in APK assets, trying filesystem paths\n");

        // Try external storage path
        const char* ext_path = SDL_AndroidGetExternalStoragePath();
        if (ext_path) {
            stdPlatform_Printf("stdEmbeddedRes: Trying Android external path: %s\n", ext_path);
            strncpy(tmp_filepath, ext_path, 256-1);
            strncat(tmp_filepath, "/resource/", 256-1);
            strncat(tmp_filepath, filepath, 256-1);
            stdPlatform_Printf("stdEmbeddedRes: Full path: %s\n", tmp_filepath);

            f = fopen(tmp_filepath, "rb");
            if (f) {
                stdPlatform_Printf("stdEmbeddedRes: File found at: %s\n", tmp_filepath);
                goto retry_file;
            }
            stdPlatform_Printf("stdEmbeddedRes: File not found at: %s\n", tmp_filepath);
        }

        // Also try internal storage path
        const char* int_path = SDL_AndroidGetInternalStoragePath();
        if (int_path) {
            stdPlatform_Printf("stdEmbeddedRes: Trying Android internal path: %s\n", int_path);
            strncpy(tmp_filepath, int_path, 256-1);
            strncat(tmp_filepath, "/resource/", 256-1);
            strncat(tmp_filepath, filepath, 256-1);
            stdPlatform_Printf("stdEmbeddedRes: Full path: %s\n", tmp_filepath);

            f = fopen(tmp_filepath, "rb");
            if (f) {
                stdPlatform_Printf("stdEmbeddedRes: File found at: %s\n", tmp_filepath);
                goto retry_file;
            }
            stdPlatform_Printf("stdEmbeddedRes: File not found at: %s\n", tmp_filepath);
        }
#endif

skip_fopen:
        strncpy(tmp_filepath, filepath, 256-1);
        
        for (int i = 0; i < strlen(tmp_filepath); i++)
        {
            if (tmp_filepath[i] == '\\') {
                tmp_filepath[i] = '/';
            }
        }

        for (size_t i = 0; i < embeddedResource_aFiles_num; i++)
        {
            if (!strcmp(embeddedResource_aFiles[i].fpath, tmp_filepath)) {
                file_contents = (char*)malloc(embeddedResource_aFiles[i].data_len+1);
                if (!file_contents) {
                    if (pOutSz) {
                        *pOutSz = 0;
                    }
                    return NULL;
                }
                memcpy(file_contents, embeddedResource_aFiles[i].data, embeddedResource_aFiles[i].data_len);
                file_contents[embeddedResource_aFiles[i].data_len] = 0;

                if (pOutSz) {
                    *pOutSz = embeddedResource_aFiles[i].data_len+1;
                }

                break;
            }
        }
    }

    return file_contents;
}