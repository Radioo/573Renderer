# r573_media_format (src/media/)

The export-format vocabulary shared by the GUI dropdown, the CLI parser,
and the export pipeline: `MediaSink::Format` plus the pure helper functions
(label/token/extension/directory-ness, token parsing, index mapping, output
path building, and the muxer / short-label columns the encoder needs).
Split out of media_sink so the CLI and tests link a tiny pure library
instead of the ffmpeg-backed Sink. The Sink itself (media_sink.h)
re-exports the enum by including this header, so all existing
`MediaSink::` call sites are unchanged.

One table, one place: `kTable` in media_format.cpp carries token,
extension, is_dir, label, muxer and short_label per format.
video_encoder.cpp used to hand-roll `MuxerName` and `FmtLabel` as separate
switches (and a third, `DefaultExtension`, that had no callers at all and
is deleted) - adding a seventh format meant editing four places in two
files. The muxer/label mapping is now covered by media_format_tests.

## Library split (r573_media_format vs r573_media)

Two libraries, deliberately:

- `r573_media_format` - the pure table. No ffmpeg, no Win32. `r573_cli`
  links it, so cli_tests stays dependency-free.
- `r573_media` - the ffmpeg-backed encoder stack (media_sink.cpp,
  video_encoder.cpp, video_encoder_codecs.cpp). PUBLIC-links
  r573_media_format plus the ffmpeg and Win32 libs, so consumers inherit
  them. `export_capture_tests` and `media_encode_tests` used to re-list
  those three .cpp files as target sources and recompile them per target;
  they now link this library.

The three r573_media TUs still physically live in flat `src/`, NOT in
`src/media/`. That is deliberate and temporary: `src/media/.clang-tidy`
re-enables the FFI check set (vararg, macro-usage, reinterpret/c-style
casts, c-arrays), and every migrated module TU in the tree is `LOG`-free
by construction - `LOG` is a vararg macro, so it trips both
`cppcoreguidelines-pro-type-vararg` and `-macro-usage`. video_encoder.cpp
and media_sink.cpp carry 13 LOG calls between them. Moving the files
without first deciding how the encoder reports failures would mean either
deleting real diagnostics or weakening the module config (which would also
weaken media_format.cpp). The physical move is therefore queued behind
that migration; the library boundary above already delivers the
build-level win.

## Stability contract

Enum integer values are STABLE and serialized into
`App::ExportRequest::format` (an int, kept trivially copyable for the
cross-thread command payload). Renaming or reordering is a breaking change;
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
  transparent-video path plays it. NVENC's HEVC encoder has no alpha
  channel, so this format can never use hardware encode on any GPU - the
  export dialog's HW-accel tooltip says exactly that, ahead of its
  machine-capability probe, because the reason is intrinsic to the format
  (`DrawHwAccelTooltip`; note `MediaSink::HardwareProbeFormat` folds every
  non-H.264 format onto AVIF, so its answer is about AV1 NVENC and is
  meaningless here).

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

## Movie decode (src/media/movie_source.cpp)

`Movie::Source` is the DECODE side of `r573_media`, and it exists for one caller:
the `poly.tile_grid` pass, which textures its quads with a game BGA movie
(`docs/preset_document.md`). IIDX's `data\movie\*.4` files are MPEG-2 PROGRAM
STREAMS (`00 00 01 BA` pack header), which libavformat demuxes as `mpegps` and
libavcodec decodes as `mpeg2video`. Nothing about the class is IIDX specific: it
opens whatever libavformat can open.

`Open(path)` demuxes, picks the best video stream and starts a decoder, keeping
every failure in `LastError()` and leaving `IsOpen()` false. `FrameRate()` is
`av_guess_frame_rate`, so it is the FILE's own rate. `IndexAt(seconds)` turns a
document time into that file's frame number. `SetOutputSize(w, h)` makes libswscale
resample every decoded picture to the size the tiles' uv rectangle expects.
`FrameAt(index)` returns the BGRA of that frame.

`FrameAt` decodes FORWARD from wherever it is, and seeks back to the start and
replays when asked for an earlier frame, which is what timeline scrubbing does.
The decoded picture is cached, so the common case of a 30 fps movie under a 60 fps
document costs one decode every other frame and a pointer return in between.

`Position()` is the frame the source is actually HOLDING, which is not the frame
that was asked for: past the end of the stream the request keeps climbing with
document time while `Position()` stops at the last decoded frame. `PolyDraw` keys
its texture upload on `Position()` for exactly that reason. Keyed on the requested
index instead, every frame past the end of a movie looked like a new picture and
re-locked and re-uploaded the same 512x512 texture for the rest of the screen.

`FrameAt` takes an optional `StepFn(decoded, wanted)`, called once per decoded
frame, and `PolyDraw::Fetch` uses it to drive the loading overlay. A normal
advance is one decode and stays silent; a backwards scrub replays from frame 0,
which is thousands of decodes on the render thread, so past `kQuietDecodeSteps`
the callback opens the overlay and then reports "Replaying the tile movie, frame
N / M" with a determinate fraction every `kReportEverySteps`. Past the end of the
stream nothing decodes, so the callback never fires and the overlay never flashes.
The overlay hooks are INJECTED (`Scene3d::MovieReporter`, wired by
`Scene3dBackend::Boot` through `Scene3dHost::SetMovieReporter`) rather than read
out of `App::Global()`, because the render layer does not depend upwards on the
state layer (`docs/ownership.md`).

`Broken()` latches when `sws_getCachedContext` cannot build a scaler. That failure
is TERMINAL: `Store` returns false, `DecodeNext` refuses to keep going, and
`FrameAt` returns an empty frame instead of a stale buffer. It used to return
early from `Store` without advancing `current` while still reporting a decoded
frame, so `FrameAt` burned all `kMaxDecodeSteps` (4096) attempts on every single
request. `PolyDraw` treats a broken source the way it treats a missing file: it
logs once and draws the tiles untextured.

**At the end of the stream it HOLDS the last decoded picture.** That is the game,
and it is worth writing down because it is a deletion rather than a design: IIDX
12's player is the DirectX 9 SDK "Texture3D" DirectShow sample (the filter still
names itself `DirectShow Texture3D Sample`), and the sample's own loop lives in a
`CheckMovieStatus()` that watches `IMediaEvent` for `EC_COMPLETE` and calls
`IMediaPosition::put_CurrentPosition(0)`. KONAMI stripped that function and its
`WM_GRAPHNOTIFY` caller out. Both interfaces are still QueryInterface'd and are
then only ever `Release()`d, so the `EC_COMPLETE` the renderer posts at end of
stream is never drained: the graph stays running, `DoRenderSample` is simply never
called again, and the D3D texture keeps the last picture until the screen exits.
To re-verify this on a new build, take the globals filled by the second and third
`QueryInterface` in the graph builder (the one carrying
`"Could not add renderer filter to graph!  hr=0x%x"`) and enumerate every xref: if
they are only the QI store, a `Release()` in the cleanup function and the CRT
static-init and atexit thunk pair, the loop is still absent.

The movie also runs on its OWN clock in the game. The player never reads
`AvgTimePerFrame` and never sets a sync source, so `CBaseRenderer` schedules each
sample by its presentation timestamp against the graph's default reference clock,
decoupled from the 60 Hz game loop. `Movie::Source` reproduces that by mapping
document SECONDS onto the file's own frame numbering rather than stepping one
movie frame per document frame.

`media_decode_tests` writes its own fixture: a 12-frame 96x64 `mpeg2video` program
stream muxed with the `mpeg` muxer into the system temp dir, each frame a flat
field one step brighter than the last. It needs no game install and no GPU. The
frames are compared RELATIVELY (strictly brightening, identical on re-read)
rather than against absolute levels, because the YUV to RGB conversion applies a
limited-range expansion that has nothing to do with what is being tested. It pins
the walk forward, the seek back, the hold past the end (byte-identical to the last
frame, and never the first one again, which is what would happen if this looped),
the seconds-to-index mapping, the scaled output and the missing-file path. It also
pins the two contracts the tile pass leans on: `Position()` stops climbing at the
last frame while the request does not, and `StepFn` fires once per decoded frame,
replays 0..N on a backwards scrub, and fires NOT AT ALL once the stream has ended.
The `Broken()` branch itself has no test: `sws_getCachedContext` only fails on
inputs `Movie::Source` never produces (its destination format is fixed and its
destination size is validated), so there is no seam to force it from outside
without an injection point that would exist only for the test. What the tests do
pin is that a stream which decodes normally never latches it.
