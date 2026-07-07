# r573_media_format (src/media/)

The export-format vocabulary shared by the GUI dropdown, the CLI parser,
and the export pipeline: `MediaSink::Format` plus the pure helper functions
(label/token/extension/directory-ness, token parsing, index mapping, output
path building). Split out of media_sink so the CLI and tests link a tiny
pure library instead of the ffmpeg-backed Sink. The Sink itself
(media_sink.h) re-exports the enum by including this header, so all
existing `MediaSink::` call sites are unchanged.

## Stability contract

Enum integer values are STABLE and serialized into
`App::Request::export_format` (an int, kept trivially copyable for the
cross-thread request struct). Renaming or reordering is a breaking change;
the enum is append-only. `FromIndex` clamps out-of-range input to AVIF so a
corrupt request degrades gracefully instead of asserting.

## Formats

| Format | token | ext | notes |
|---|---|---|---|
| AVIF | `avif` | .avif | AV1 dual-stream, alpha, NVENC-capable; browser decode of 60 fps animated AVIF is unreliable |
| WebM_VP9 | `webm-vp9` | .webm | alpha (yuva420p), software-only |
| WebM_AV1 | `webm-av1` | .webm | opaque, NVENC |
| WebP_Anim | `webp` | .webp | alpha, software; recommended default |
| PNG_Sequence | `png` | (dir) | folder of frame_NNNNNN.png, lossless |
| MP4_H264 | `mp4` | .mp4 | opaque, h264_nvenc or libx264, most compatible |
| MP4_HEVC_Alpha | `mp4-hevc-alpha` | .mp4 | ALPHA via libx265 --alpha, hvc1 (Safari); needs the x265 vcpkg overlay (docs/build.md) |

`ParseToken` also accepts legacy aliases preserved from the pre-MediaSink
CLI parsers: `webm` (=VP9, the name before WebM-AV1 existed), `vp9`, `av1`,
`webp-anim`, `png-seq`, `pngseq`, `h264`, `avc`, `mp4-h264`, `hevc`,
`hevc-alpha`, `mp4-hevc`, `safari`.

`WritesDirectory` is true only for PNG_Sequence: the sink creates a
directory of per-frame files rather than a single file. `MakeOutputPath`
appends the extension for file formats and trims a trailing slash for
directory formats (so consumers can blindly append "/frame_*.png").

Adding a format: append the enum value, bump `kFormatCount`, add one
`kTable` row here, and wire the backend in media_sink's `Sink::Impl::Open`.
CLI, dropdowns, and path building pick it up from the table.

## Encoder implementation (src/video_encoder.cpp)

The VideoEncoder wraps libav. `Encoder::Impl` hides the plumbing behind the
public API. Frame layout per format:

- Input to the encoder is always source-sized BGRA; `sws_scale` converts to
  the encoder pixel format and output size.
- `frame_main` holds the encoder's own pix_fmt: yuv444p (AVIF colour),
  yuv420p (opaque H.264 / AV1), or yuva420p (WebM_VP9 / WebP_Anim / HEVC
  alpha, where the alpha rides in the frame's 4th plane).
- `frame_alpha` (gray8) + `sws_alpha` exist for AVIF ONLY: AVIF is the only
  format that carries alpha as a SEPARATE stream, so the alpha plane is
  extracted on its own. Every other alpha-capable format packs alpha into a
  single yuva420p stream.

Stream construction per format:

- AVIF: dual-stream (colour + alpha). Colour = NVENC if requested and
  available else libaom yuv444p; alpha = ALWAYS libaom gray8 (small
  bitstream, no benefit from hardware at this scale). Both streams get
  timebase `{1, fps*1000}` for jitter-free frame pacing.
- WebM_VP9: single yuva420p stream, alpha packed in the 4th plane,
  software-only.
- WebP_Anim: libwebp_anim, single yuva420p, software-only (animated WebP is
  VP8 internally, which has no hardware path; software is fast anyway).
- WebM_AV1: hardware AV1 (NVENC first when requested, else libaom-av1),
  OPAQUE-only (no hardware-accelerated alpha path for AV1-in-WebM).
- MP4_H264: h264_nvenc first, fall back to libx264, opaque (H.264 has no
  alpha plane).
- MP4_HEVC_Alpha: libx265 with an x265 auxiliary alpha layer (single
  yuva420p, no hardware path); forces the `hvc1` tag (MKTAG) so Safari's
  transparent-video path plays it.

Keyframe interval (`Params::keyframe_interval`, `KeyframeGop` helper): every
codec path sets `gop_size` and `keyint_min` from it. 0 (default) keeps the
historical cadence of one keyframe per second (`gop = fps`); a positive value
overrides it directly (floored to 1, or 2 for av1_nvenc which rejects a GOP
of 1). A large interval - or one >= the captured frame count - forces a
single keyframe, which shrinks static / slowly-scrolling scenes dramatically
because every non-key frame is a near-empty delta (measured ~34% smaller on a
180-frame animated SDVX bg; more on truly static content). `libwebp_anim`
sets no `gop_size` and PNG is not a codec, so `MediaSink::UsesKeyframeInterval`
returns false for WebP_Anim and PNG_Sequence and the UI hides the control for
them.

Dispatch quirks:

- PNG_Sequence is MediaSink's own directory writer, not an encoder format;
  the codec dispatch coerces it to AVIF so the encoder switch never sees it
  (mirrors the retired MediaSink->VideoEncoder bridge's default arm).
- H.264 yuv420p requires EVEN dimensions; dims are rounded down so an odd
  crop can't hard-fail libx264 / h264_nvenc.
- Muxers: AVIF -> "avif" (HEIF-derived), WebP_Anim -> "webp", WebM_* ->
  "webm" (matroska subset Blink's `<video>` parser accepts), MP4_* -> "mp4"
  (ISO BMFF).
- WebP infinite loop: the "webp" muxer has its own `loop` option defaulting
  to 1 (play once) that OVERRIDES the loop count libwebp_anim wrote into the
  bitstream. Both layers (the encoder's `loop` in OpenLibwebpAnim AND the
  muxer's `loop=0` here) must be set or browsers play the file once.
- MP4 faststart: the moov atom is moved to the front so the file streams /
  plays before it is fully downloaded.

## PNG-sequence backend (src/media_sink.cpp)

A `MediaSink::Sink` holds exactly one of `enc` (the video encoder) or `png`
(the sequence writer), chosen by the format at `Open()`. The PNG backend
uses WIC: one `frame_NNNNNN.png` per captured frame (6-digit zero-pad so a
directory listing sorts in capture order); zlib compression is baked into
WIC's PNG codec. `CoInitializeEx` is called ONCE at session start and
released at session end, not per frame - per-frame init would cost
noticeably on 600-frame captures. On Cancel (or a re-Open of an existing
directory) the sink best-effort deletes every PNG it may have written so a
prior failed run leaves no stale frames mixed in. `Sink` guards a
moved-from state (unique_ptr Impl left null) so a session struct can
`sink = {}; sink.Open(...)` to reset between runs.

## Native file dialog (src/native_dialog.cpp)

The IFileOpen/IFileSave shell dialogs run on an apartment-threaded (STA)
thread and call `CoInitializeEx` per invocation, tearing it down on exit:
most threads in the app do not CoInitialize themselves, and the shell COM
objects are not thread-safe so they require STA.

## NVENC probe

NVENC probe (`ProbeNvenc`): opens the named encoder at a 256x256 yuv420p
test config (clears every current card's minimum: NVENC AV1 needs 128x128
on Ada; H.264 NVENC's minimum is smaller). ffmpeg stderr is silenced; the
return code is the verdict. Cached per encoder via a thread-safe local
static (init costs hundreds of ms spinning up a CUDA context + NVENC
session, and the verdict cannot change without an app restart since GPU /
driver are not hot-swapped). MP4_H264 probes h264_nvenc (present on far more
GPUs); the AV1 formats probe av1_nvenc (Ada+ only).

## CI encode net (media_encode_tests)

`media_encode_tests` (tests/media/media_encode_tests.cpp) drives the REAL
MediaSink -> ffmpeg pipeline in hosted CI with synthetic BGRA frames - no GPU,
game DLLs, or content. Per format (software paths, prefer_hardware=false):
open, submit 12 moving-gradient 64x48 frames with keyframe_interval=5, Finish,
then RE-OPEN the output with avformat and assert it probes (stream count,
video dims). Covers: AVIF (color+alpha mux; the avif demuxer exposes >= 2
streams incl. aux items), WebM-VP9 (plus the out_width/out_height sws scaling
path, probed at the scaled size), WebM-AV1 (libaom software), WebP (the ffmpeg
webp DEMUXER reports 0x0 dims for animated webp, so only stream presence is
asserted), MP4-HEVC-alpha (libx265 hvc1), PNG sequence (WIC; asserts 12
numbered frames on disk). MP4-H264 asserts the documented
no-software-encoder error contract when the build lacks libx264 and NVENC is
off; if a software H.264 encoder appears in a future ffmpeg bump the test
passes through. Outputs go to the system temp dir, never the repo.
