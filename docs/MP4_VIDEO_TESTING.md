# MP4 Video Player - Testing Guide

## Build Status

The MP4 video player integration has been successfully built with the following configuration:
- **Build Directory:** `build_mp4_test`
- **Executable:** `build_mp4_test/Release/openjkdf2-64.exe`
- **FFmpeg Libraries:** Located in `lib/ffmpeg/`

## Files Created/Modified

### New Files
1. `src/external/libmp4/mp4_player.h` - Public interface for MP4 playback
2. `src/external/libmp4/mp4_player.c` - FFmpeg-based implementation
3. `docs/MP4_VIDEO_INTEGRATION_PLAN.md` - Detailed integration documentation

### Modified Files
1. `src/Main/jkCutscene.c` - Integrated MP4 playback support
2. `src/Main/jkCutscene.h` - Added mp4_process declaration
3. `CMakeLists.txt` - Added FFmpeg detection and linking

## How It Works

### Video Format Detection
The system detects video format by file extension:
- `.mp4`, `.mkv`, `.webm`, `.avi`, `.mov` → Uses FFmpeg MP4 player
- `.san` → Uses SMUSH decoder (MOTS)
- `.smk` → Uses Smacker decoder (original JK)

### Playback Pipeline
1. `jkCutscene_sub_421310()` opens the video file
2. FFmpeg decodes video frames to RGB24
3. RGB is quantized to 8-bit palette-indexed for compatibility
4. Audio is decoded and converted to 16-bit PCM
5. Frames are rendered using the existing VBuffer system

## Testing

### Prerequisites
1. FFmpeg DLLs must be in the same directory as the executable:
   - `avcodec-62.dll`
   - `avformat-62.dll`
   - `avutil-60.dll`
   - `swscale-9.dll`
   - `swresample-6.dll`

### Test Video
A test video has been created at:
```
build_mp4_test/Release/video/test.mp4
```

### Running Tests
1. Copy game assets (episode/, resource/) to the build directory
2. Place MP4 video files in `video/` folder
3. Run `openjkdf2-64.exe`

### To Replace Intro Video with MP4
Rename your MP4 file to match the expected video name:
- For JK: `video/01-02a.mp4` (instead of `.smk`)
- For MOTS: `video/jkmintro.mp4` (instead of `.san`)

## CMake Configuration

To build with MP4 support:
```bash
cmake .. -DTARGET_USE_FFMPEG=ON
cmake --build . --config Release
```

FFmpeg will be automatically detected from:
1. `$ENV{FFMPEG_DIR}` environment variable
2. `lib/ffmpeg/` in the source tree
3. `C:/ffmpeg/` (Windows default)

## Supported Video Codecs

Through FFmpeg, the following codecs are supported:
- **Video:** H.264, H.265/HEVC, VP8, VP9, MPEG-1/2/4, and more
- **Audio:** AAC, MP3, Vorbis, Opus, PCM, and more
- **Containers:** MP4, MKV, WebM, AVI, MOV, and more

## Known Limitations

1. **Color Quantization:** Video is converted to 8-bit palette for compatibility with the existing rendering pipeline. This may cause some color banding in high-quality videos.

2. **Subtitles:** MP4 subtitle tracks are not currently supported. The existing subtitle system (via string tables) still works.

3. **Seeking:** Basic seeking is implemented but not tested. Skip functionality (ESC key) should work.

## Troubleshooting

### "FFmpeg not found" during CMake
- Ensure FFmpeg development libraries are installed
- Set `FFMPEG_DIR` environment variable to FFmpeg installation path
- Or place FFmpeg in `lib/ffmpeg/` with `include/` and `lib/` subdirectories

### Missing DLL errors at runtime
- Copy all FFmpeg DLLs from `lib/ffmpeg/bin/` to the executable directory

### Video doesn't play
- Check the file extension is supported (.mp4, .mkv, .webm, .avi, .mov)
- Ensure the video codec is supported by FFmpeg
- Check console output for error messages

### No audio
- Ensure the video has an audio track
- Check audio sample rate is compatible (typically 44100 or 22050 Hz)
