/**
 * libmp4 - FFmpeg-based MP4/H.264 video decoder for OpenJKDF2
 *
 * This implementation provides video decoding compatible with the existing
 * jkCutscene pipeline, outputting both RGB and palette-indexed formats.
 */

#include "mp4_player.h"

// Windows compatibility
#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#ifdef USE_FFMPEG_VIDEO

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Internal Structures
// ============================================================================

struct mp4_ctx {
    // FFmpeg format/demux context
    AVFormatContext* format_ctx;

    // Video decoding
    AVCodecContext* video_codec_ctx;
    int video_stream_idx;
    AVFrame* video_frame;
    AVFrame* video_frame_rgb;
    struct SwsContext* sws_ctx;

    // Audio decoding
    AVCodecContext* audio_codec_ctx;
    int audio_stream_idx;
    AVFrame* audio_frame;
    struct SwrContext* swr_ctx;

    // Video info
    uint32_t width;
    uint32_t height;
    double fps;
    uint32_t num_frames;
    uint32_t cur_frame;

    // Audio info
    int audio_channels;
    int audio_sample_rate;
    int audio_bits_per_sample;

    // Output buffers
    uint8_t* rgb_buffer;        // RGB24 output (width * height * 3)
    uint8_t* rgba_buffer;       // RGBA32 output (width * height * 4)
    uint8_t* indexed_buffer;    // 8-bit indexed (width * height)
    uint8_t palette[256 * 3];   // Current palette

    // Audio output
    mp4_audio_callback_t audio_callback;
    uint8_t* audio_buffer;
    size_t audio_buffer_size;
    size_t audio_buffer_capacity;

    // State
    int is_done;
    int has_video_frame;

    // Packet for reading
    AVPacket* packet;
};

// ============================================================================
// Color Quantization (RGB -> 8-bit palette)
// ============================================================================

// Simple median-cut palette generation for compatibility with existing pipeline
static void mp4_generate_palette_from_frame(mp4_ctx* ctx) {
    // Use a fixed "web-safe" style palette for simplicity
    // This gives decent results for most video content
    int idx = 0;

    // Generate a 6x6x6 color cube (216 colors)
    for (int r = 0; r < 6; r++) {
        for (int g = 0; g < 6; g++) {
            for (int b = 0; b < 6; b++) {
                ctx->palette[idx * 3 + 0] = r * 51;
                ctx->palette[idx * 3 + 1] = g * 51;
                ctx->palette[idx * 3 + 2] = b * 51;
                idx++;
            }
        }
    }

    // Fill remaining with grayscale ramp
    for (int i = 216; i < 256; i++) {
        uint8_t gray = (uint8_t)((i - 216) * 255 / 39);
        ctx->palette[i * 3 + 0] = gray;
        ctx->palette[i * 3 + 1] = gray;
        ctx->palette[i * 3 + 2] = gray;
    }
}

// Find closest palette color for RGB value
static uint8_t mp4_find_closest_color(const uint8_t* palette, uint8_t r, uint8_t g, uint8_t b) {
    // For 6x6x6 cube, we can compute directly
    int ri = (r + 25) / 51;
    int gi = (g + 25) / 51;
    int bi = (b + 25) / 51;

    if (ri > 5) ri = 5;
    if (gi > 5) gi = 5;
    if (bi > 5) bi = 5;

    return (uint8_t)(ri * 36 + gi * 6 + bi);
}

// Convert RGB buffer to indexed
static void mp4_rgb_to_indexed(mp4_ctx* ctx) {
    const uint8_t* rgb = ctx->rgb_buffer;
    uint8_t* indexed = ctx->indexed_buffer;
    size_t num_pixels = ctx->width * ctx->height;

    for (size_t i = 0; i < num_pixels; i++) {
        indexed[i] = mp4_find_closest_color(ctx->palette,
            rgb[i * 3 + 0],
            rgb[i * 3 + 1],
            rgb[i * 3 + 2]);
    }
}

// ============================================================================
// Open/Close
// ============================================================================

mp4_ctx* mp4_open_file(const char* filename) {
    mp4_ctx* ctx = (mp4_ctx*)calloc(1, sizeof(mp4_ctx));
    if (!ctx) return NULL;

    ctx->video_stream_idx = -1;
    ctx->audio_stream_idx = -1;
    ctx->audio_bits_per_sample = 16; // Default to 16-bit output

    // Open file
    if (avformat_open_input(&ctx->format_ctx, filename, NULL, NULL) < 0) {
        fprintf(stderr, "mp4_player: Failed to open file: %s\n", filename);
        free(ctx);
        return NULL;
    }

    // Find stream info
    if (avformat_find_stream_info(ctx->format_ctx, NULL) < 0) {
        fprintf(stderr, "mp4_player: Failed to find stream info\n");
        avformat_close_input(&ctx->format_ctx);
        free(ctx);
        return NULL;
    }

    // Find video and audio streams
    for (unsigned i = 0; i < ctx->format_ctx->nb_streams; i++) {
        AVStream* stream = ctx->format_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && ctx->video_stream_idx < 0) {
            ctx->video_stream_idx = i;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && ctx->audio_stream_idx < 0) {
            ctx->audio_stream_idx = i;
        }
    }

    if (ctx->video_stream_idx < 0) {
        fprintf(stderr, "mp4_player: No video stream found\n");
        avformat_close_input(&ctx->format_ctx);
        free(ctx);
        return NULL;
    }

    // Initialize video decoder
    AVStream* video_stream = ctx->format_ctx->streams[ctx->video_stream_idx];
    const AVCodec* video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!video_codec) {
        fprintf(stderr, "mp4_player: Video codec not found\n");
        avformat_close_input(&ctx->format_ctx);
        free(ctx);
        return NULL;
    }

    ctx->video_codec_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(ctx->video_codec_ctx, video_stream->codecpar);

    if (avcodec_open2(ctx->video_codec_ctx, video_codec, NULL) < 0) {
        fprintf(stderr, "mp4_player: Failed to open video codec\n");
        avcodec_free_context(&ctx->video_codec_ctx);
        avformat_close_input(&ctx->format_ctx);
        free(ctx);
        return NULL;
    }

    // Store video info
    ctx->width = ctx->video_codec_ctx->width;
    ctx->height = ctx->video_codec_ctx->height;

    // Calculate FPS
    if (video_stream->avg_frame_rate.den != 0) {
        ctx->fps = av_q2d(video_stream->avg_frame_rate);
    } else if (video_stream->r_frame_rate.den != 0) {
        ctx->fps = av_q2d(video_stream->r_frame_rate);
    } else {
        ctx->fps = 30.0; // Default fallback
    }

    // Estimate frame count
    if (video_stream->nb_frames > 0) {
        ctx->num_frames = (uint32_t)video_stream->nb_frames;
    } else if (video_stream->duration > 0 && ctx->fps > 0) {
        double duration_sec = video_stream->duration * av_q2d(video_stream->time_base);
        ctx->num_frames = (uint32_t)(duration_sec * ctx->fps);
    } else {
        ctx->num_frames = 0; // Unknown
    }

    printf("mp4_player: Opened %s\n", filename);
    printf("  Video: %ux%u @ %.2f fps, ~%u frames\n",
           ctx->width, ctx->height, ctx->fps, ctx->num_frames);

    // Initialize audio decoder (if present)
    if (ctx->audio_stream_idx >= 0) {
        AVStream* audio_stream = ctx->format_ctx->streams[ctx->audio_stream_idx];
        const AVCodec* audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);

        if (audio_codec) {
            ctx->audio_codec_ctx = avcodec_alloc_context3(audio_codec);
            avcodec_parameters_to_context(ctx->audio_codec_ctx, audio_stream->codecpar);

            if (avcodec_open2(ctx->audio_codec_ctx, audio_codec, NULL) == 0) {
                ctx->audio_channels = ctx->audio_codec_ctx->ch_layout.nb_channels;
                ctx->audio_sample_rate = ctx->audio_codec_ctx->sample_rate;

                printf("  Audio: %d channels @ %d Hz\n",
                       ctx->audio_channels, ctx->audio_sample_rate);

                // Setup resampler for output (convert to S16 interleaved)
                ctx->swr_ctx = swr_alloc();
                if (ctx->swr_ctx) {
                    AVChannelLayout out_layout;
                    if (ctx->audio_channels == 1) {
                        out_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
                    } else {
                        out_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
                    }

                    av_opt_set_chlayout(ctx->swr_ctx, "in_chlayout", &ctx->audio_codec_ctx->ch_layout, 0);
                    av_opt_set_chlayout(ctx->swr_ctx, "out_chlayout", &out_layout, 0);
                    av_opt_set_int(ctx->swr_ctx, "in_sample_rate", ctx->audio_sample_rate, 0);
                    av_opt_set_int(ctx->swr_ctx, "out_sample_rate", ctx->audio_sample_rate, 0);
                    av_opt_set_sample_fmt(ctx->swr_ctx, "in_sample_fmt", ctx->audio_codec_ctx->sample_fmt, 0);
                    av_opt_set_sample_fmt(ctx->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

                    if (swr_init(ctx->swr_ctx) < 0) {
                        fprintf(stderr, "mp4_player: Failed to init resampler\n");
                        swr_free(&ctx->swr_ctx);
                        ctx->swr_ctx = NULL;
                    }
                }

                ctx->audio_frame = av_frame_alloc();
            } else {
                avcodec_free_context(&ctx->audio_codec_ctx);
                ctx->audio_stream_idx = -1;
            }
        }
    }

    // Allocate video frames
    ctx->video_frame = av_frame_alloc();
    ctx->video_frame_rgb = av_frame_alloc();

    // Setup scaler for RGB24 output
    ctx->sws_ctx = sws_getContext(
        ctx->width, ctx->height, ctx->video_codec_ctx->pix_fmt,
        ctx->width, ctx->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    if (!ctx->sws_ctx) {
        fprintf(stderr, "mp4_player: Failed to create scaler context\n");
    }

    // Allocate output buffers
    ctx->rgb_buffer = (uint8_t*)malloc(ctx->width * ctx->height * 3);
    ctx->rgba_buffer = (uint8_t*)malloc(ctx->width * ctx->height * 4);
    ctx->indexed_buffer = (uint8_t*)malloc(ctx->width * ctx->height);

    // Generate initial palette
    mp4_generate_palette_from_frame(ctx);

    // Allocate packet
    ctx->packet = av_packet_alloc();

    // Audio buffer
    ctx->audio_buffer_capacity = 32768;
    ctx->audio_buffer = (uint8_t*)malloc(ctx->audio_buffer_capacity);
    ctx->audio_buffer_size = 0;

    return ctx;
}

void mp4_destroy(mp4_ctx* ctx) {
    if (!ctx) return;

    if (ctx->sws_ctx) sws_freeContext(ctx->sws_ctx);
    if (ctx->swr_ctx) swr_free(&ctx->swr_ctx);
    if (ctx->video_frame) av_frame_free(&ctx->video_frame);
    if (ctx->video_frame_rgb) av_frame_free(&ctx->video_frame_rgb);
    if (ctx->audio_frame) av_frame_free(&ctx->audio_frame);
    if (ctx->packet) av_packet_free(&ctx->packet);
    if (ctx->video_codec_ctx) avcodec_free_context(&ctx->video_codec_ctx);
    if (ctx->audio_codec_ctx) avcodec_free_context(&ctx->audio_codec_ctx);
    if (ctx->format_ctx) avformat_close_input(&ctx->format_ctx);

    free(ctx->rgb_buffer);
    free(ctx->rgba_buffer);
    free(ctx->indexed_buffer);
    free(ctx->audio_buffer);

    free(ctx);
}

// ============================================================================
// Video Information
// ============================================================================

uint32_t mp4_video_width(mp4_ctx* ctx) {
    return ctx ? ctx->width : 0;
}

uint32_t mp4_video_height(mp4_ctx* ctx) {
    return ctx ? ctx->height : 0;
}

double mp4_video_fps(mp4_ctx* ctx) {
    return ctx ? ctx->fps : 0.0;
}

uint32_t mp4_num_frames(mp4_ctx* ctx) {
    return ctx ? ctx->num_frames : 0;
}

uint32_t mp4_cur_frame(mp4_ctx* ctx) {
    return ctx ? ctx->cur_frame : 0;
}

// ============================================================================
// Audio Information
// ============================================================================

int mp4_audio_channels(mp4_ctx* ctx) {
    return ctx ? ctx->audio_channels : 0;
}

int mp4_audio_sample_rate(mp4_ctx* ctx) {
    return ctx ? ctx->audio_sample_rate : 0;
}

int mp4_audio_bits_per_sample(mp4_ctx* ctx) {
    return ctx ? ctx->audio_bits_per_sample : 16;
}

// ============================================================================
// Decoding
// ============================================================================

static int mp4_decode_video_packet(mp4_ctx* ctx) {
    int ret = avcodec_send_packet(ctx->video_codec_ctx, ctx->packet);
    if (ret < 0) return ret;

    ret = avcodec_receive_frame(ctx->video_codec_ctx, ctx->video_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return ret;
    } else if (ret < 0) {
        return ret;
    }

    // Convert to RGB24
    uint8_t* dest[1] = { ctx->rgb_buffer };
    int dest_linesize[1] = { (int)(ctx->width * 3) };

    sws_scale(ctx->sws_ctx,
        (const uint8_t* const*)ctx->video_frame->data,
        ctx->video_frame->linesize, 0, ctx->height,
        dest, dest_linesize);

    // Also convert to RGBA for convenience
    for (uint32_t i = 0; i < ctx->width * ctx->height; i++) {
        ctx->rgba_buffer[i * 4 + 0] = ctx->rgb_buffer[i * 3 + 0];
        ctx->rgba_buffer[i * 4 + 1] = ctx->rgb_buffer[i * 3 + 1];
        ctx->rgba_buffer[i * 4 + 2] = ctx->rgb_buffer[i * 3 + 2];
        ctx->rgba_buffer[i * 4 + 3] = 255;
    }

    // Convert to indexed for legacy pipeline
    mp4_rgb_to_indexed(ctx);

    ctx->has_video_frame = 1;
    ctx->cur_frame++;

    return 0;
}

static int mp4_decode_audio_packet(mp4_ctx* ctx) {
    if (!ctx->audio_codec_ctx || !ctx->swr_ctx) return 0;

    int ret = avcodec_send_packet(ctx->audio_codec_ctx, ctx->packet);
    if (ret < 0) return ret;

    while (ret >= 0) {
        ret = avcodec_receive_frame(ctx->audio_codec_ctx, ctx->audio_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            return ret;
        }

        // Resample to S16
        int out_samples = av_rescale_rnd(
            swr_get_delay(ctx->swr_ctx, ctx->audio_sample_rate) + ctx->audio_frame->nb_samples,
            ctx->audio_sample_rate, ctx->audio_sample_rate, AV_ROUND_UP);

        int out_channels = (ctx->audio_channels > 1) ? 2 : 1;
        size_t out_size = out_samples * out_channels * 2; // 16-bit = 2 bytes

        // Ensure buffer is large enough
        if (ctx->audio_buffer_size + out_size > ctx->audio_buffer_capacity) {
            ctx->audio_buffer_capacity = ctx->audio_buffer_size + out_size + 32768;
            ctx->audio_buffer = (uint8_t*)realloc(ctx->audio_buffer, ctx->audio_buffer_capacity);
        }

        uint8_t* out_ptr = ctx->audio_buffer + ctx->audio_buffer_size;
        int converted = swr_convert(ctx->swr_ctx,
            &out_ptr, out_samples,
            (const uint8_t**)ctx->audio_frame->data, ctx->audio_frame->nb_samples);

        if (converted > 0) {
            ctx->audio_buffer_size += converted * out_channels * 2;
        }
    }

    return 0;
}

int mp4_frame(mp4_ctx* ctx) {
    if (!ctx || ctx->is_done) return MP4_DONE;

    ctx->has_video_frame = 0;

    while (!ctx->has_video_frame) {
        int ret = av_read_frame(ctx->format_ctx, ctx->packet);
        if (ret < 0) {
            // End of file or error
            ctx->is_done = 1;
            return MP4_DONE;
        }

        if (ctx->packet->stream_index == ctx->video_stream_idx) {
            ret = mp4_decode_video_packet(ctx);
            av_packet_unref(ctx->packet);

            if (ret == 0) {
                // Successfully decoded a video frame
                return MP4_MORE;
            } else if (ret != AVERROR(EAGAIN)) {
                // Error
                ctx->is_done = 1;
                return MP4_ERROR;
            }
        } else if (ctx->packet->stream_index == ctx->audio_stream_idx) {
            mp4_decode_audio_packet(ctx);
            av_packet_unref(ctx->packet);

            // Flush audio through callback if we have enough
            if (ctx->audio_callback && ctx->audio_buffer_size >= 4096) {
                ctx->audio_callback(ctx->audio_buffer, ctx->audio_buffer_size);
                ctx->audio_buffer_size = 0;
            }
        } else {
            av_packet_unref(ctx->packet);
        }
    }

    return MP4_MORE;
}

int mp4_done(mp4_ctx* ctx) {
    return ctx ? ctx->is_done : 1;
}

// ============================================================================
// Video Data Access
// ============================================================================

const uint8_t* mp4_get_video_indexed(mp4_ctx* ctx) {
    return ctx ? ctx->indexed_buffer : NULL;
}

const uint8_t* mp4_get_palette(mp4_ctx* ctx) {
    return ctx ? ctx->palette : NULL;
}

const uint8_t* mp4_get_video_rgb(mp4_ctx* ctx) {
    return ctx ? ctx->rgb_buffer : NULL;
}

const uint8_t* mp4_get_video_rgba(mp4_ctx* ctx) {
    return ctx ? ctx->rgba_buffer : NULL;
}

// ============================================================================
// Audio Operations
// ============================================================================

void mp4_set_audio_callback(mp4_ctx* ctx, mp4_audio_callback_t callback) {
    if (ctx) {
        ctx->audio_callback = callback;
    }
}

void mp4_audio_flush(mp4_ctx* ctx) {
    if (!ctx || !ctx->audio_callback || ctx->audio_buffer_size == 0) return;

    ctx->audio_callback(ctx->audio_buffer, ctx->audio_buffer_size);
    ctx->audio_buffer_size = 0;
}

// ============================================================================
// Seek Operations
// ============================================================================

int mp4_seek_frame(mp4_ctx* ctx, uint32_t frame) {
    if (!ctx || ctx->video_stream_idx < 0) return -1;

    AVStream* stream = ctx->format_ctx->streams[ctx->video_stream_idx];
    int64_t timestamp = (int64_t)(frame / ctx->fps / av_q2d(stream->time_base));

    if (av_seek_frame(ctx->format_ctx, ctx->video_stream_idx, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
        return -1;
    }

    avcodec_flush_buffers(ctx->video_codec_ctx);
    if (ctx->audio_codec_ctx) {
        avcodec_flush_buffers(ctx->audio_codec_ctx);
    }

    ctx->is_done = 0;
    ctx->cur_frame = frame;
    return 0;
}

int mp4_rewind(mp4_ctx* ctx) {
    if (!ctx) return -1;

    if (av_seek_frame(ctx->format_ctx, -1, 0, AVSEEK_FLAG_BACKWARD) < 0) {
        return -1;
    }

    avcodec_flush_buffers(ctx->video_codec_ctx);
    if (ctx->audio_codec_ctx) {
        avcodec_flush_buffers(ctx->audio_codec_ctx);
    }

    ctx->is_done = 0;
    ctx->cur_frame = 0;
    return 0;
}

#else // !USE_FFMPEG_VIDEO

// Stub implementations when FFmpeg is not available

mp4_ctx* mp4_open_file(const char* filename) {
    (void)filename;
    fprintf(stderr, "mp4_player: FFmpeg support not compiled in\n");
    return NULL;
}

void mp4_destroy(mp4_ctx* ctx) { (void)ctx; }
uint32_t mp4_video_width(mp4_ctx* ctx) { (void)ctx; return 0; }
uint32_t mp4_video_height(mp4_ctx* ctx) { (void)ctx; return 0; }
double mp4_video_fps(mp4_ctx* ctx) { (void)ctx; return 0.0; }
uint32_t mp4_num_frames(mp4_ctx* ctx) { (void)ctx; return 0; }
uint32_t mp4_cur_frame(mp4_ctx* ctx) { (void)ctx; return 0; }
int mp4_audio_channels(mp4_ctx* ctx) { (void)ctx; return 0; }
int mp4_audio_sample_rate(mp4_ctx* ctx) { (void)ctx; return 0; }
int mp4_audio_bits_per_sample(mp4_ctx* ctx) { (void)ctx; return 16; }
int mp4_frame(mp4_ctx* ctx) { (void)ctx; return MP4_DONE; }
int mp4_done(mp4_ctx* ctx) { (void)ctx; return 1; }
const uint8_t* mp4_get_video_indexed(mp4_ctx* ctx) { (void)ctx; return NULL; }
const uint8_t* mp4_get_palette(mp4_ctx* ctx) { (void)ctx; return NULL; }
const uint8_t* mp4_get_video_rgb(mp4_ctx* ctx) { (void)ctx; return NULL; }
const uint8_t* mp4_get_video_rgba(mp4_ctx* ctx) { (void)ctx; return NULL; }
void mp4_set_audio_callback(mp4_ctx* ctx, mp4_audio_callback_t callback) { (void)ctx; (void)callback; }
void mp4_audio_flush(mp4_ctx* ctx) { (void)ctx; }
int mp4_seek_frame(mp4_ctx* ctx, uint32_t frame) { (void)ctx; (void)frame; return -1; }
int mp4_rewind(mp4_ctx* ctx) { (void)ctx; return -1; }

#endif // USE_FFMPEG_VIDEO
