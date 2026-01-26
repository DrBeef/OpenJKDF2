# MP4 Video Player Integration Plan

## Overview

This document outlines how to integrate modern MP4 video playback into OpenJKDF2, replacing or supplementing the existing Smacker (.SMK) and SMUSH (.SAN) video decoders.

## Current Video System Architecture

### File Locations
- `src/Main/jkCutscene.c` - Main video playback controller
- `src/Main/jkCutscene.h` - Public interface
- `src/Main/jkSmack.c` - Video trigger/invocation system
- `src/external/libsmacker/` - Smacker video decoder
- `src/external/libsmusher/` - SMUSH video decoder

### Existing Decoder Interface Pattern

Both existing decoders follow this pattern:
```c
// Open
smk smk_open_file(const char* filename, uint8_t mode);
smush_ctx* smush_from_fpath(const char* fpath);

// Get Info
smk_info_video(smk, &width, &height, &y_scale);
smk_info_all(smk, &frame, &frame_count, &usf);

// Get Frame Data
const uint8_t* smk_get_video(smk);       // 8-bit palette indexed
const uint8_t* smk_get_palette(smk);     // 256*3 RGB
const uint8_t* smk_get_audio(smk, track);
uint32_t smk_get_audio_size(smk, track);

// Advance Frame
char smk_next(smk);  // Returns SMK_DONE, SMK_MORE, or SMK_ERROR
smush_frame(ctx);
smush_done(ctx);

// Close
void smk_close(smk);
void smush_destroy(smush_ctx*);
```

### Current Rendering Pipeline

1. Video frames are decoded to **8-bit palette-indexed** format
2. Palette is copied to `stdDisplay_masterPalette` (256*3 bytes)
3. Frame data is copied to `jkCutscene_frameBuf` (VBuffer)
4. VBuffer is blitted to `Video_pMenuBuffer`
5. Screen flip via `stdDisplay_DDrawGdiSurfaceFlip()`

### Current Audio Pipeline

1. Audio callback queues PCM chunks to ring buffer
2. `jkCutscene_smacker_smusher_audio_queue()` processes queue
3. Audio buffers created via `stdSound_BufferCreate(stereo, sampleRate, bitDepth, size)`
4. Playback via `stdSound_BufferQueueAfterAnother()`

---

## MP4 Integration Approach

### Option 1: FFmpeg-based (Recommended)

**Pros:**
- Handles MP4/H.264/H.265/AAC natively
- Cross-platform (Windows, Linux, macOS, Android, WebAssembly)
- Well-documented, widely used
- Can output to any pixel format

**Cons:**
- Large dependency (~20-50MB depending on configuration)
- Requires linking libavcodec, libavformat, libavutil, libswscale, libswresample

**Required FFmpeg Libraries:**
- `libavformat` - Container demuxing (MP4)
- `libavcodec` - Video/audio decoding (H.264, AAC)
- `libswscale` - Pixel format conversion (YUV→RGB)
- `libswresample` - Audio resampling (if needed)

### Option 2: pl_mpeg (Lightweight Alternative)

**Pros:**
- Single-header library (~4000 lines)
- No external dependencies
- Very small footprint

**Cons:**
- Only supports MPEG-1 Video + MP2 Audio
- Users must convert MP4→MPEG-1 (quality loss)
- No modern codec support

### Option 3: SDL2 + FFmpeg Hybrid

Since the project already uses SDL2, leverage SDL_mixer or a dedicated FFmpeg wrapper that outputs to SDL textures.

---

## Recommended Implementation: FFmpeg Integration

### New Files to Create

```
src/external/libmp4/
├── mp4_player.h        # Public interface matching existing pattern
├── mp4_player.c        # FFmpeg-based implementation
└── CMakeLists.txt      # Build configuration
```

### Proposed Interface (mp4_player.h)

```c
#ifndef MP4_PLAYER_H
#define MP4_PLAYER_H

#include <stdint.h>
#include <stddef.h>

// Opaque context
typedef struct mp4_ctx mp4_ctx;

// Return codes
#define MP4_DONE    0
#define MP4_MORE    1
#define MP4_ERROR  -1

// Audio callback type (same as SMUSH)
typedef void (*mp4_audio_callback_t)(const uint8_t* data, size_t len);

// Open/Close
mp4_ctx* mp4_open_file(const char* filename);
void mp4_destroy(mp4_ctx* ctx);

// Video Info
uint32_t mp4_video_width(mp4_ctx* ctx);
uint32_t mp4_video_height(mp4_ctx* ctx);
double mp4_video_fps(mp4_ctx* ctx);
uint32_t mp4_num_frames(mp4_ctx* ctx);

// Audio Info
int mp4_audio_channels(mp4_ctx* ctx);
int mp4_audio_sample_rate(mp4_ctx* ctx);
int mp4_audio_bits_per_sample(mp4_ctx* ctx);

// Decode next frame
int mp4_frame(mp4_ctx* ctx);

// Get decoded data
// Option A: 8-bit palette (for compatibility with existing pipeline)
const uint8_t* mp4_get_video_indexed(mp4_ctx* ctx);
const uint8_t* mp4_get_palette(mp4_ctx* ctx);

// Option B: Direct RGB/RGBA output (preferred for modern rendering)
const uint8_t* mp4_get_video_rgb(mp4_ctx* ctx);    // RGB24
const uint8_t* mp4_get_video_rgba(mp4_ctx* ctx);   // RGBA32

// Audio (via callback or direct)
void mp4_set_audio_callback(mp4_ctx* ctx, mp4_audio_callback_t callback);
void mp4_audio_flush(mp4_ctx* ctx);

// Check completion
int mp4_done(mp4_ctx* ctx);

// Seek (optional)
int mp4_seek_frame(mp4_ctx* ctx, uint32_t frame);

#endif // MP4_PLAYER_H
```

### Implementation Skeleton (mp4_player.c)

```c
#include "mp4_player.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

struct mp4_ctx {
    // FFmpeg contexts
    AVFormatContext* format_ctx;
    AVCodecContext* video_codec_ctx;
    AVCodecContext* audio_codec_ctx;
    struct SwsContext* sws_ctx;
    struct SwrContext* swr_ctx;

    // Stream indices
    int video_stream_idx;
    int audio_stream_idx;

    // Decoded frame buffers
    AVFrame* video_frame;
    AVFrame* audio_frame;

    // Output buffers
    uint8_t* rgb_buffer;      // RGB24 output
    uint8_t* indexed_buffer;  // 8-bit indexed (for legacy compatibility)
    uint8_t palette[256 * 3]; // Quantized palette

    // Audio
    mp4_audio_callback_t audio_callback;
    uint8_t* audio_buffer;
    size_t audio_buffer_size;

    // State
    int width, height;
    double fps;
    int done;
};

mp4_ctx* mp4_open_file(const char* filename) {
    mp4_ctx* ctx = calloc(1, sizeof(mp4_ctx));
    if (!ctx) return NULL;

    // Open file
    if (avformat_open_input(&ctx->format_ctx, filename, NULL, NULL) < 0) {
        free(ctx);
        return NULL;
    }

    // Find stream info
    if (avformat_find_stream_info(ctx->format_ctx, NULL) < 0) {
        avformat_close_input(&ctx->format_ctx);
        free(ctx);
        return NULL;
    }

    // Find video stream
    ctx->video_stream_idx = -1;
    ctx->audio_stream_idx = -1;

    for (unsigned i = 0; i < ctx->format_ctx->nb_streams; i++) {
        AVStream* stream = ctx->format_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && ctx->video_stream_idx < 0) {
            ctx->video_stream_idx = i;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && ctx->audio_stream_idx < 0) {
            ctx->audio_stream_idx = i;
        }
    }

    if (ctx->video_stream_idx < 0) {
        avformat_close_input(&ctx->format_ctx);
        free(ctx);
        return NULL;
    }

    // Initialize video decoder
    AVStream* video_stream = ctx->format_ctx->streams[ctx->video_stream_idx];
    const AVCodec* video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    ctx->video_codec_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(ctx->video_codec_ctx, video_stream->codecpar);
    avcodec_open2(ctx->video_codec_ctx, video_codec, NULL);

    ctx->width = ctx->video_codec_ctx->width;
    ctx->height = ctx->video_codec_ctx->height;
    ctx->fps = av_q2d(video_stream->avg_frame_rate);

    // Initialize audio decoder (if present)
    if (ctx->audio_stream_idx >= 0) {
        AVStream* audio_stream = ctx->format_ctx->streams[ctx->audio_stream_idx];
        const AVCodec* audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
        ctx->audio_codec_ctx = avcodec_alloc_context3(audio_codec);
        avcodec_parameters_to_context(ctx->audio_codec_ctx, audio_stream->codecpar);
        avcodec_open2(ctx->audio_codec_ctx, audio_codec, NULL);
    }

    // Allocate frames
    ctx->video_frame = av_frame_alloc();
    ctx->audio_frame = av_frame_alloc();

    // Setup scaler for RGB output
    ctx->sws_ctx = sws_getContext(
        ctx->width, ctx->height, ctx->video_codec_ctx->pix_fmt,
        ctx->width, ctx->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    // Allocate output buffer
    ctx->rgb_buffer = malloc(ctx->width * ctx->height * 3);
    ctx->indexed_buffer = malloc(ctx->width * ctx->height);

    return ctx;
}

int mp4_frame(mp4_ctx* ctx) {
    if (ctx->done) return MP4_DONE;

    AVPacket packet;
    while (av_read_frame(ctx->format_ctx, &packet) >= 0) {
        if (packet.stream_index == ctx->video_stream_idx) {
            // Decode video
            avcodec_send_packet(ctx->video_codec_ctx, &packet);
            if (avcodec_receive_frame(ctx->video_codec_ctx, ctx->video_frame) == 0) {
                // Convert to RGB
                uint8_t* dest[1] = { ctx->rgb_buffer };
                int dest_linesize[1] = { ctx->width * 3 };
                sws_scale(ctx->sws_ctx,
                    (const uint8_t* const*)ctx->video_frame->data,
                    ctx->video_frame->linesize, 0, ctx->height,
                    dest, dest_linesize);

                av_packet_unref(&packet);
                return MP4_MORE;
            }
        } else if (packet.stream_index == ctx->audio_stream_idx && ctx->audio_callback) {
            // Decode audio and call callback
            avcodec_send_packet(ctx->audio_codec_ctx, &packet);
            while (avcodec_receive_frame(ctx->audio_codec_ctx, ctx->audio_frame) == 0) {
                // Convert to 16-bit PCM and call callback
                // ... (audio resampling code)
            }
        }
        av_packet_unref(&packet);
    }

    ctx->done = 1;
    return MP4_DONE;
}

const uint8_t* mp4_get_video_rgb(mp4_ctx* ctx) {
    return ctx->rgb_buffer;
}

uint32_t mp4_video_width(mp4_ctx* ctx) { return ctx->width; }
uint32_t mp4_video_height(mp4_ctx* ctx) { return ctx->height; }
double mp4_video_fps(mp4_ctx* ctx) { return ctx->fps; }

void mp4_destroy(mp4_ctx* ctx) {
    if (!ctx) return;

    if (ctx->sws_ctx) sws_freeContext(ctx->sws_ctx);
    if (ctx->video_frame) av_frame_free(&ctx->video_frame);
    if (ctx->audio_frame) av_frame_free(&ctx->audio_frame);
    if (ctx->video_codec_ctx) avcodec_free_context(&ctx->video_codec_ctx);
    if (ctx->audio_codec_ctx) avcodec_free_context(&ctx->audio_codec_ctx);
    if (ctx->format_ctx) avformat_close_input(&ctx->format_ctx);

    free(ctx->rgb_buffer);
    free(ctx->indexed_buffer);
    free(ctx);
}
```

---

## Integration into jkCutscene.c

### Changes Required

1. Add include for mp4_player.h
2. Add new static variable `static mp4_ctx* jkCutscene_pMp4 = NULL;`
3. Modify `jkCutscene_sub_421310()` to detect .mp4 files
4. Create new `jkCutscene_mp4_process()` function
5. Update `jkCutscene_smack_related_loops()` to handle MP4

### Modified jkCutscene_sub_421310() (excerpt)

```c
int jkCutscene_sub_421310(char* fpath)
{
    // ... existing code ...

    // Try MP4 first (new code)
    if (strstr(tmp, ".mp4") || strstr(tmp, ".MP4")) {
        jkCutscene_pMp4 = mp4_open_file(tmp);
        if (jkCutscene_pMp4) {
            // Setup for MP4 playback
            jkCutscene_smk_w = mp4_video_width(jkCutscene_pMp4);
            jkCutscene_smk_h = mp4_video_height(jkCutscene_pMp4);
            jkCutscene_smk_usf = 1000000.0 / mp4_video_fps(jkCutscene_pMp4);

            // Create RGB VBuffer instead of palette-indexed
            stdVBufferTexFmt texFmt;
            texFmt.width = jkCutscene_smk_w;
            texFmt.height = jkCutscene_smk_h;
            texFmt.format.bpp = 24;  // RGB24 instead of 8-bit
            jkCutscene_frameBuf = stdDisplay_VBufferNew(&texFmt, 1, 0, NULL);

            // Setup audio buffers
            int channels = mp4_audio_channels(jkCutscene_pMp4);
            int sampleRate = mp4_audio_sample_rate(jkCutscene_pMp4);
            int bits = mp4_audio_bits_per_sample(jkCutscene_pMp4);

            for (int i = 0; i < AUDIO_NUM_STDBUFS; i++) {
                jkCutscene_audio[i] = stdSound_BufferCreate(
                    channels == 2, sampleRate, bits, AUDIO_BUFS_DEPTH);
                stdSound_BufferSetVolume(jkCutscene_audio[i], jkGuiSound_cutsceneVolume);
            }

            mp4_set_audio_callback(jkCutscene_pMp4, mp4_audio_callback);

            // ... rest of setup ...
            return 1;
        }
    }

    // Fall through to existing SMK/SMUSH code
    jkCutscene_pSmush = smush_from_fpath(tmp);
    // ... existing code continues ...
}
```

### New mp4_process() function

```c
int jkCutscene_mp4_process()
{
    if (!jkCutscene_isRendering) return 0;
    if (!std3D_IsReady()) return 0;

    flex64_t cur_displayFrame = (flex64_t)Linux_TimeUs();
    flex64_t usPerFrame = jkCutscene_smk_usf;
    flex64_t delta = cur_displayFrame - last_displayFrame;

    jkCutscene_smacker_smusher_audio_queue();

    if (delta <= usPerFrame) return 0;

    if (last_displayFrame)
        extraUs += (delta - usPerFrame);
    last_displayFrame = cur_displayFrame;

    // Skip frames if lagging
    while (extraUs > usPerFrame) {
        if (mp4_frame(jkCutscene_pMp4) == MP4_DONE) {
            return 1;
        }
        extraUs -= usPerFrame;
    }

    // Decode frame
    if (mp4_frame(jkCutscene_pMp4) == MP4_DONE) {
        return 1;
    }

    // Get RGB data and render
    const uint8_t* rgb_data = mp4_get_video_rgb(jkCutscene_pMp4);

    stdDisplay_VBufferLock(jkCutscene_frameBuf);
    memcpy(jkCutscene_frameBuf->surface_lock_alloc, rgb_data,
           jkCutscene_smk_w * jkCutscene_smk_h * 3);
    stdDisplay_VBufferUnlock(jkCutscene_frameBuf);

    stdDisplay_VBufferLock(Video_pMenuBuffer);
    stdDisplay_VBufferCopy(Video_pMenuBuffer, jkCutscene_frameBuf, 0, 50, NULL, 0);
    stdDisplay_VBufferFill(Video_pMenuBuffer, 0, &jkCutscene_rect1);
    stdDisplay_VBufferCopy(Video_pMenuBuffer, &Video_otherBuf,
                          jkCutscene_rect1.x, jkCutscene_rect1.y,
                          &jkCutscene_rect1, 0);
    stdDisplay_VBufferUnlock(Video_pMenuBuffer);

    return 0;
}
```

---

## CMake Integration

Add to CMakeLists.txt:

```cmake
# MP4 video support via FFmpeg
set(TARGET_USE_FFMPEG TRUE CACHE BOOL "Enable FFmpeg for MP4 video playback")

if(TARGET_USE_FFMPEG)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED
        libavcodec libavformat libavutil libswscale libswresample)

    include_directories(${FFMPEG_INCLUDE_DIRS})
    link_directories(${FFMPEG_LIBRARY_DIRS})
    add_compile_definitions(USE_FFMPEG_VIDEO)

    list(APPEND ENGINE_SOURCE_FILES
        ${PROJECT_SOURCE_DIR}/src/external/libmp4/mp4_player.c)
endif()

# Link FFmpeg libraries
if(TARGET_USE_FFMPEG)
    target_link_libraries(${BIN_NAME} ${FFMPEG_LIBRARIES})
endif()
```

---

## VBuffer Considerations

The current VBuffer system uses 8-bit palette-indexed rendering. For MP4 with RGB output, you have two options:

### Option A: Convert RGB to Palette (Slower, Compatible)
Use color quantization to convert RGB frames to 256-color palette. This maintains compatibility with the existing pipeline but adds CPU overhead.

### Option B: Extend VBuffer for RGB (Faster, Recommended)
Modify the VBuffer copy functions to handle RGB24/RGBA32 formats directly. The OpenGL renderer already supports this internally.

Check `stdDisplay_VBufferCopy()` implementation - it may already support different BPP formats. If not, add a code path for 24-bit blitting.

---

## File Format Detection

In `jkCutscene_sub_421310()`, detect format by extension:

```c
const char* ext = strrchr(fpath, '.');
if (ext) {
    if (strcasecmp(ext, ".mp4") == 0 ||
        strcasecmp(ext, ".mkv") == 0 ||
        strcasecmp(ext, ".webm") == 0) {
        // Use MP4/FFmpeg decoder
    } else if (strcasecmp(ext, ".san") == 0) {
        // Use SMUSH decoder
    } else {
        // Default to Smacker
    }
}
```

---

## Platform-Specific Notes

### Windows (MSVC)
- Use vcpkg to install FFmpeg: `vcpkg install ffmpeg:x64-windows`
- Or download pre-built binaries from https://github.com/BtbN/FFmpeg-Builds

### Linux
- `apt install libavcodec-dev libavformat-dev libswscale-dev libswresample-dev`

### macOS
- `brew install ffmpeg`

### WebAssembly
- Build FFmpeg with Emscripten (complex but possible)
- Or skip MP4 support on WASM (current SMK files already skipped)

### Nintendo DSi
- FFmpeg too large; keep existing SMK/SMUSH only

---

## Testing Plan

1. **Basic playback**: Play a simple MP4 file
2. **Audio sync**: Verify audio/video remain synchronized
3. **Seeking**: Test skip functionality (ESC key)
4. **Pause**: Test space bar pause/resume
5. **Subtitles**: Verify subtitle system works (may need sidecar .srt support)
6. **Performance**: Test on lower-end hardware
7. **Format variety**: Test H.264, H.265, VP9, various audio codecs

---

## Alternative: Keep SMK, Convert MP4→SMK

If adding FFmpeg is too complex, consider:
1. Provide a tool to convert MP4→SMK
2. Use RAD Video Tools or ffmpeg command line
3. Document the conversion process for modders

This keeps the codebase simple but requires users to convert videos.

---

## Summary

The recommended approach is:
1. Add FFmpeg as an optional dependency
2. Create `libmp4` wrapper following existing decoder pattern
3. Modify `jkCutscene.c` to detect and handle MP4 files
4. Update CMake for FFmpeg linking
5. Optionally extend VBuffer for native RGB support

This provides modern video support while maintaining the existing architecture and fallback to SMK/SMUSH for compatibility.
