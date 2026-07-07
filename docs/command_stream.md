# The render command stream (P7)

The afp -> GPU boundary as a typed, recordable command list. The plan's CI
render-regression net: record the exact draw/state command sequence a frame
produces, compare it against a golden (per-field epsilon), and eventually
REPLAY it through a standalone executor so the render logic is testable without
the live callback plumbing. This doc is the architecture, the executor
blueprint it was built from, and the verification evidence per step.

## What the direct backend path does (the thing being strangled)

The proprietary afp-core / afp-utils DLLs call our registered callbacks
(afp_d3d9_callbacks.cpp SetLayer / SetBlend / SubmitGeometry, afp_d3d9_commands.cpp
SetMaskRegion / LayerCommand). Each callback issues D3D9 device calls DIRECTLY
(SubmitGeometry alone touches ~53 device calls: SetRenderState, SetTexture,
SetPixelShader, SetSamplerState, SetVertexShaderConstantF, DrawPrimitiveUP...).
There is no seam between "decide what to draw" and "issue it to the GPU", so
rendering can only be tested by running the real DLLs + a GPU.

## The recording layer

The pure per-command logic lives in the gated, unit-tested `r573_render`
library (namespace `Render`). Six modules:

| module | purpose |
|--------|---------|
| blend_map | afp blend-mode -> D3D9 blend states (shared w/ DDR) |
| vertex_math | AfpVertex + flag-packed decode + half-pixel snap + UV inset |
| hsv_filter | afp HSV descriptor parse + gate + shader-constant math |
| command_trace | one stable golden text line per command (Trace* formatters) |
| command_list | the TYPED command vocabulary + FormatCommandList |
| command_diff | per-field-epsilon list comparison (DiffCommandLists) |

### The vocabulary (command_list.h)

`RenderCommand = std::variant<SetLayerCmd, SetBlendCmd, DrawCmd, MaskCmd,
LayerCmdCmd>`; `RenderCommandList = std::vector<RenderCommand>`. `DrawCmd`
OWNS its vertices (`std::vector<AfpVertex>` copied at record time) so a
recorded list is self-contained and replayable. Every field the executor
needs to reproduce the draw is captured: prim_type, tex_slot, the hsl/add
pixel-shader selection, and the full snapped/inset vertex buffer.

### The observe tap (GpuContext.cmd_list)

`GpuContext` carries `Render::RenderCommandList* cmd_list = nullptr`. When
non-null, each state/draw callback appends a structured command
(`g_gpu.cmd_list->emplace_back(...)`) IN ADDITION to its normal device calls -
observe-only, execution untouched. Null (default) = zero cost, zero pixel
effect (verified: tap-off export byte-size unchanged). `--cmd-trace <file>`
(render_loop) points the tap at a per-run list, then writes
`FormatCommandList(list)` at loop exit.

### The golden format + determinism

`FormatCommandList` produces one line per command via std::format
(locale-independent -> deterministic). Draw line:
`draw prim=<t> vc=<n> tex=<slot> ps=<hsl|add|-> xy=[..bbox..] uv=[..bbox..]
c0=<hex>`. VERIFIED byte-identical across two runs on the same binary/DLLs, so
an EXACT text match is a valid same-machine regression signal; the per-field
epsilon in DiffCommandLists is for cross-build / executor-parity comparison
where the last float digit may legitimately jitter.

### The comparison (command_diff.h)

`DiffCommandLists(ref, actual, epsilon) -> optional<CommandDivergence{index,
reason}>`: variant-type mismatch + int/id/flag/color EXACT + float within
epsilon; returns the first divergence with a human reason or nullopt. This is
the golden-check primitive AND the executor's future parity check.

## The executor blueprint

Goal: split "decide" from "issue". The callbacks stop calling the device
directly; they RECORD into a command list (they already can), and a standalone
`D3D9Executor` consumes the list and issues the device calls. Then the render
logic is exercised in CI by building a list + snapshotting/diffing it (no GPU),
and the executor is validated locally by byte-comparing its output against the
current direct path.

### Proposed shape

- `RenderCommand` gains the state fields the direct calls currently derive
  inline: SetLayerCmd already carries blend_mode + HSV desc; it must also carry
  the resolved `Blend::D3d9Blend` + the mode-3 multiply-stage flag so the
  executor does not re-run backend logic (keep the decision in the recorder,
  the executor is dumb).
- `void D3D9Executor::Execute(IDirect3DDevice9*, const RenderCommand&, const
  GpuContext&)` - one `std::visit` dispatch, each arm issuing the exact device
  sequence the matching callback issues today (lift those ~53 calls verbatim
  into the executor arms). Texture bind + shader selection read GpuContext
  (device, textures[], afp_hsl_ps/afp_add_ps, hsv_scope rects).
- The callbacks become: build the command, then (transition phase) call
  `Execute(...)` immediately - so behaviour is UNCHANGED while the code moves.
  Once every callback records-then-executes, the "record whole frame, execute
  the list in one pass" mode is a flag flip.

### Verification plan (mandatory, this is the hot path)

1. Extract each callback's device sequence into the executor arm; callback
   calls `Execute` inline. Byte-compare IIDX 12-frame dump + DDR bg_0009
   --ddr-test PNG (the standing stash-dance recipe: build old, dump, pop,
   build new, dump, SHA). MUST be identical - it is pure code motion.
2. Only after (1) is byte-clean: add the deferred "execute the recorded list
   at end of frame" path behind a flag, byte-compare again.
3. The GPU-less CI test: FakeGameRuntime (P8) drives the recorder -> snapshot
   the golden -> DiffCommandLists vs a committed reference. This is where the
   command stream finally becomes a hosted-CI regression gate.

### Risks

- Hot path: any divergence in the lifted device sequence silently changes
  pixels. Byte-compare EVERY step, both backends, per the CLAUDE.md rule.
- LayerCommand case 4 (AFPU effect objects drawing via their own vtables) does
  NOT go through our device calls - it stays an opaque executor pass-through
  (record a LayerCmdCmd, execute by calling the same afpu vtable path). The
  command stream cannot fully capture those draws; document the gap.
- State that persists across draws (g_last_setlayer_blend, current_matrix,
  the sampler-filter tracking) must be threaded through the executor or kept
  in GpuContext; do not let the executor read backend file-statics.

## Implementation state

Recording + comparison: implemented, unit-tested, byte-verified.

Executor (`RenderExec::Execute`, src/render_executor.{h,cpp}): step 1 covers
SetLayerCmd, SetBlendCmd, MaskCmd, and DrawCmd. Each callback builds the
typed command and calls the matching `Execute` inline, so the device sequence is
unchanged while the issue-side lives behind the command seam. The DrawCmd arm is
the whole SubmitGeometry issue path (texture bind from the recorder-resolved
`cmd.tex_slot`, shape-color c4, `Render::AfpPrimCount` prim mapping + zero-prim
early-out, matrix c0..c3, HSL / additive-coverage shader setup + post-draw
restores, DrawPrimitiveUP). The recorder keeps the decisions: it resolves the
texture slot, gates the HSV/additive filters (only for a drawable prim, matching
the old post-zero-prim order), packs the pixel-shader constants into
`DrawCmd.hsv_c1` / `hsv_c3`, and records the DrawCmd only for a non-zero prim.

Verified byte-clean: DDR bg_0009 12-frame `--ddr-test` dump SHA-identical across
the stash-dance (covers bind / fallback / shape-color / prim / zero-prim /
matrix / mask / blend across hundreds of draws); the HSL + additive-coverage
sub-paths (qpro-only, not hit by DDR) proven character-identical to the pre-slice
callback by source diff of the lifted device calls; 27 r573_render test cases
(incl. `AfpPrimCount`) + all four gates pass.

LayerCmdCmd arm: the device-relevant sub-dispatches (3 = nested vtable,
4 = the AFPU effect-object device hand-off, 5 = fn-ptr pair) moved verbatim into
the executor; the live AFPU pointers a2/a3/a4 ride alongside the recorded
command as side-channel args (like DrawCmd's verts) because they cannot be
captured for replay - this IS the documented LayerCommand replay gap. The subs
0/1/2 + unknown screams stay in the recording callback: AFP_SCREAM_UNIMPL
captures _ReturnAddress to map the afp-core caller offset, which only exists at
the callback frame. LayerCommand never fires in practice for title.ifs
(0 calls over 15s) - the
whole callback is a registered safety net, so no live content byte-compares the
dispatch itself. Verification therefore = 24-frame byte-compare of the flows
that DO run (SDVX7 select_bg_vi 1080x1920 + IIDX33 02005.ifs, 12 raw .bgra
frames each, SHA-identical across the stash-dance - proves the rewired
callback + unconditional executor call perturb nothing) + source-diff of the
moved dispatch vs HEAD (identical modulo g_gpu -> gpu/device operand renames).

Step 1 is complete: every AfpD3D9 render callback records its typed command and
issues through RenderExec.

Step 2 (deferred whole-frame replay) is complete: `--deferred-replay` sets
GpuContext.deferred_replay; the callbacks record into GpuContext.frame_cmds
(verts always copied, zero-prim draws included so the replay reproduces the
live counters) and issue NOTHING inline; EndRender replays the frame through
`RenderExec::ExecuteList` (still inside the BeginFrame scene bracket), then
clears the list (BeginRender also clears, in case a frame aborted mid-record).
Three state hazards were closed to make replay-at-end-of-frame correct:

- The layer matrix mutates per-layer within a frame (SetMatrix), so DrawCmd now
  CARRIES it (`matrix` + `matrix_ready`, captured at record time); the Draw arm
  uploads cmd.matrix, never gpu.current_matrix. DiffCommandLists compares it
  (epsilon).
- The in_mask_write matte-skip flag is a record-time decision (it gates what
  SubmitGeometry lets into the stream), so the SetMaskRegion CALLBACK owns it
  now; the Mask executor arm only issues scissor state.
- SetLayer records the filter descriptor ONLY when HsvFilterActive accepts it
  (id 100/101 + valid) - afp hands over its reused filter work struct even for
  filter ids we never consume (e.g. id 192), whose bytes are not
  HsvDescriptor-layout and hold per-run heap junk; recording them made the
  --cmd-trace golden nondeterministic across identical runs.

LayerCommand subs 3/4/5 still execute inline under deferred replay (the live
AFPU pointers cannot be stored - the documented replay gap); that reorders them
ahead of the frame's deferred draws, so a one-shot AFP_SCREAM_UNIMPL fires if
content ever hits them in this mode (none is known to).

Step-2 verification (all byte-identical): flag-off vs pre-slice = 24/24 frames
(SDVX7 select_bg_vi + IIDX33 02005); deferred ON vs OFF = 24/24 frames; the
--cmd-trace golden identical between live and deferred runs and now
deterministic across repeated identical runs. The DDR backend is untouched
(own callbacks, own mask flag, own render loop).

Step 3 (the GPU-less CI recorder gate) is complete: the `command_stream_tests`
target (tests/render/command_stream_gate_tests.cpp) links the REAL recorder TUs
(afp_d3d9_callbacks.cpp, afp_d3d9_commands.cpp, render_executor.cpp - they
depend only on the gated libs plus the header-inline g_gpu, so they link into a
test binary directly; they compile without r573::warnings there because they
are monolith sources, while the test TU itself is tidy-scoped via tests/*) and
drives a fixed synthetic frame through the actual callbacks with a NULL device:
`deferred_replay = true` puts the recorder in record-only mode, and the device
boot-guards were relaxed to `!device && !deferred_replay` so recording works
GPU-less (behavior-unchanged in real runs - the device is never null while afp
renders; byte-verified 12/12 SDVX frames across the stash-dance).

The fixed frame covers: SetLayer/SetBlend records, a TRIANGLESTRIP draw with
UVs from the geo blob, a TRIANGLELIST draw with the flag-packed
uv+color+xy vertex stream and a legacy tex-ref miss, the mask bracket (op0
matte draw DROPPED / op1 rect / op2), a zero-prim draw (recorded for replay
only), and LayerCommand (bit0-off no-op + a recorded sub-2 stub). Four checks:
the FormatCommandList text equals the committed golden
(tests/render/goldens/command_stream_frame.golden - a missing golden is
written and FAILED for review, so blessing is an explicit local step);
tap-vs-frame structural counts (matte drop, zero-prim replay-only, LayerCmdCmd
tap-only); run-to-run determinism via DiffCommandLists; and a perturbed
recording is flagged. The golden locks the recorder's real decisions - vertex
snap, matte suppression, prim mapping, tex resolve, per-vertex color decode -
in hosted CI with no game DLLs, content, or GPU. Not covered GPU-less: the
HSL/additive shader gating (needs live shader objects) and the tex-resolve HIT
path (needs live texture objects) - both stay covered by the local pixel
byte-compares.

All three executor steps are complete. The command stream is: recorded (step
1), replayable (step 2), and CI-gated (step 3). Future extension: drive the
recorder from a FakeGameRuntime once P8 builds one, widening the synthetic
frame into full scene scripts.

## Step 4 (P10): the WARP pixel golden

`pixel_golden_tests` (tests/render/pixel_golden_tests.cpp + src/warp_device.{h,cpp})
closes the executor's device-side coverage gap in hosted CI: the recorder gate
(step 3) locks WHAT gets recorded, this locks WHAT THE EXECUTOR DRAWS. The test
records a fixed synthetic frame through the REAL callbacks (deferred_replay
record-only, exactly the production deferred path), then replays it via
`RenderExec::ExecuteList` on a real D3D9 device backed by software: d3d9.dll's
`Direct3DCreate9On12` with a D3D12 WARP device (`IDXGIFactory4::EnumWarpAdapter`
-> `D3D12CreateDevice`), all bound dynamically (LoadLibrary/GetProcAddress, no
new link deps beyond d3d9.lib). If any bring-up step fails (pre-1903 Windows, no
d3d9on12) the test SKIPs with the failing step - safe under `ctest -L ci`.

The scene renders at 320x240 A8R8G8B8 over a cleared background: a
vertex-color-gradient quad (blend mode 0), an overlapping mode-1 quad, a
textured quad hitting a REAL registered texture (legacy tex_ref path: ref&0xFFFF
+1 = slot; a 4x4 checker at slot 7 with dims registered so ResolveBoundTexture
HITs), a full-screen translucent quad clipped by the mask bracket (op1 scissor
op2 restore), and a zero-prim draw (replay no-op). Untextured draws bind the
white 2x2 fallback texture. The executor runs FIXED-FUNCTION (afp_vs null): the
test supplies the base state (modulate stage0, POINT samplers for cross-WARP
determinism, the alpha-blend defaults) plus an orthographic PROJECTION transform
mapping pixel space to clip space, because without the vs.1.1 shader the
D3DFVF_XYZ vertices go through the fixed-function transform. NOT covered here:
the HSL/additive pixel-shader arms (they need D3DXCompileShader at runtime,
absent on CI runners) - those stay covered by the local byte-compare net.

Both gates share `tests/render/scene_support.h`, the header-only scene-script
vocabulary (quad/prim/xyz-skip-flag submit builders over the real callbacks) so
new coverage is written once and locked twice - as a recorder text golden AND a
WARP pixel hash. The first shared script is `DriveWideScene`: blend modes
2/3/4 (multiply, inv-src-color, add), prim types 1/2/6 (LINELIST, LINESTRIP,
TRIANGLEFAN), the xyz vertex flag (0x02) combined with the skip-2 flag (0x10),
and the SetLayer mode-3 stage-1 multiply path (TFACTOR/BLENDCURRENTALPHA) over
a textured quad. Goldens: `command_stream_wide.golden` +
`pixel_golden_wide.sha`. The recorder gate runs it texture-less (tex resolve
MISS recorded); the pixel gate runs it with the checker registered (executor
HIT drawn).

Verification inside the test: the frame is rendered TWICE and byte-compared
(intra-run determinism), then FNV-1a-64-hashed against a MACHINE-LOCAL baseline
in `local_baselines/` (gitignored, same policy as render_regression). The
hashes are NOT committed: WARP rasterization is deterministic per WARP build
but NOT across machines - proven in hosted CI (the windows-2022 runner's
WARP produced different pixels than the dev machine for the identical command
list, both internally deterministic). A missing baseline is blessed + WARNed + PASSed
(CI runners have ephemeral workspaces, so in hosted CI the test asserts device
replay succeeds + intra-run determinism; the hash regression gate is the
machine-local tier, where the baseline persists across builds). On mismatch the
actual frame is dumped to the build dir for review; a local hash flip with no
code change is a Windows WARP update (review + re-bless), a flip WITH a code
change is a regression. The RECORDER text goldens stay committed - they are
machine-independent and passed on CI unchanged. The include-cleaner ignore list
gained `unknwnbase.h` (IUnknown's canonical definer; unknwn.h is just a wrapper
the checker refuses to credit).
