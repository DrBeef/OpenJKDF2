/**
 * libmp4 - A C library for decoding MP4/H.264 video files using FFmpeg
 * For OpenJKDF2 - Modern video playback support
 *
 * Interface designed to match libsmacker/libsmusher patterns for easy integration
 */

#ifndef MP4_PLAYER_H
#define MP4_PLAYER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque context
typedef struct mp4_ctx mp4_ctx;

// Return codes (matching SMK pattern)
#define MP4_DONE    0
#define MP4_MORE    1
#define MP4_ERROR  -1

// Audio callback type (same pattern as SMUSH)
typedef void (*mp4_audio_callback_t)(const uint8_t* data, size_t len);

// ============================================================================
// Open/Close Operations
// ============================================================================

/**
 * Open an MP4 file for playback
 * @param filename Path to the video file
 * @return Context pointer, or NULL on failure
 */
mp4_ctx* mp4_open_file(const char* filename);

/**
 * Close and free all resources
 * @param ctx Context to destroy
 */
void mp4_destroy(mp4_ctx* ctx);

// ============================================================================
// Video Information
// ============================================================================

/**
 * Get video width in pixels
 */
uint32_t mp4_video_width(mp4_ctx* ctx);

/**
 * Get video height in pixels
 */
uint32_t mp4_video_height(mp4_ctx* ctx);

/**
 * Get video framerate (frames per second)
 */
double mp4_video_fps(mp4_ctx* ctx);

/**
 * Get total number of frames (approximate)
 */
uint32_t mp4_num_frames(mp4_ctx* ctx);

/**
 * Get current frame number
 */
uint32_t mp4_cur_frame(mp4_ctx* ctx);

// ============================================================================
// Audio Information
// ============================================================================

/**
 * Get number of audio channels (1=mono, 2=stereo)
 */
int mp4_audio_channels(mp4_ctx* ctx);

/**
 * Get audio sample rate in Hz
 */
int mp4_audio_sample_rate(mp4_ctx* ctx);

/**
 * Get audio bits per sample (typically 16)
 */
int mp4_audio_bits_per_sample(mp4_ctx* ctx);

// ============================================================================
// Decoding Operations
// ============================================================================

/**
 * Decode the next frame (video + audio)
 * @param ctx Context
 * @return MP4_MORE if frame decoded, MP4_DONE if end of file, MP4_ERROR on error
 */
int mp4_frame(mp4_ctx* ctx);

/**
 * Check if playback is complete
 * @return 1 if done, 0 if more frames available
 */
int mp4_done(mp4_ctx* ctx);

// ============================================================================
// Video Data Access
// ============================================================================

/**
 * Get decoded video frame as 8-bit palette-indexed data
 * Compatible with existing jkCutscene pipeline
 * @return Pointer to width*height bytes of indexed color data
 */
const uint8_t* mp4_get_video_indexed(mp4_ctx* ctx);

/**
 * Get current palette (256 * 3 bytes, RGB)
 * @return Pointer to 768 bytes of palette data
 */
const uint8_t* mp4_get_palette(mp4_ctx* ctx);

/**
 * Get decoded video frame as RGB24 data
 * @return Pointer to width*height*3 bytes of RGB data
 */
const uint8_t* mp4_get_video_rgb(mp4_ctx* ctx);

/**
 * Get decoded video frame as RGBA32 data
 * @return Pointer to width*height*4 bytes of RGBA data
 */
const uint8_t* mp4_get_video_rgba(mp4_ctx* ctx);

// ============================================================================
// Audio Operations
// ============================================================================

/**
 * Set audio callback for streaming audio data
 * Callback receives PCM data (16-bit signed, native endian)
 * @param ctx Context
 * @param callback Function to call with audio data
 */
void mp4_set_audio_callback(mp4_ctx* ctx, mp4_audio_callback_t callback);

/**
 * Flush any pending audio data through the callback
 */
void mp4_audio_flush(mp4_ctx* ctx);

// ============================================================================
// Seek Operations (Optional)
// ============================================================================

/**
 * Seek to a specific frame (approximate)
 * @param ctx Context
 * @param frame Target frame number
 * @return 0 on success, -1 on failure
 */
int mp4_seek_frame(mp4_ctx* ctx, uint32_t frame);

/**
 * Rewind to the beginning
 * @return 0 on success, -1 on failure
 */
int mp4_rewind(mp4_ctx* ctx);

#ifdef __cplusplus
}
#endif

#endif // MP4_PLAYER_H
