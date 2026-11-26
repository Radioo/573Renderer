# The render command stream (P7)

The afp -> GPU boundary as a typed, recordable command list. It is the plan's CI
render-regression net: record the exact draw and state command sequence a frame
produces, compare it against a golden (per-field epsilon), and replay it through
a standalone executor so the render logic can be tested without the live callback
plumbing. This doc covers the architecture, the executor blueprint it was built
from, and the verification evidence for each step.

## What the direct backend path does (the thing being strangled)

The proprietary afp-core / afp-utils DLLs call our registered callbacks
(afp_d3d9_callbacks.cpp SetLayer / SetBlend / SubmitGeometry, afp_d3d9_commands.cpp
SetMaskRegion / LayerCommand). Each callback issues D3D9 device calls directly
(SubmitGeometry alone touches ~53 device calls: SetRenderState, SetTexture,
SetPixelShader, SetSamplerState, SetVertexShaderConstantF, DrawPrimitiveUP...).
There is no seam between deciding what to draw and issuing it to the GPU, so
rendering can only be tested by running the real DLLs and a GPU.

## The recording layer

The pure per-command logic lives in the gated, unit-tested `r573_render`
library (namespace `Render`). It has six modules:

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
owns its vertices (`std::vector<AfpVertex>` copied at record time), so a
recorded list is self-contained and can be replayed. Every field the executor
needs to reproduce the draw is captured: prim_type, tex_slot, the hsl/add
pixel-shader selection, and the full snapped and inset vertex buffer.

### The observe tap (GpuContext.cmd_list)

`GpuContext` carries `Render::RenderCommandList* cmd_list = nullptr`. When it is
non-null, each state or draw callback appends a structured command
(`g_gpu.cmd_list->emplace_back(...)`) in addition to its normal device calls.
The tap only observes and leaves execution untouched. Null (the default) costs
nothing and has no pixel effect (verified: the export byte size is unchanged with
the tap off). `--cmd-trace <file>` (render_loop) points the tap at a per-run list,
then writes `FormatCommandList(list)` when the loop exits.

### The golden format + determinism

`FormatCommandList` produces one line per command via std::format, which is
locale-independent and therefore deterministic. Draw line:
`draw prim=<t> vc=<n> tex=<slot> ps=<hsl|add|-> xy=[..bbox..] uv=[..bbox..]
c0=<hex>`. The output was verified byte-identical across two runs on the same
binary and DLLs, so an exact text match is a valid regression signal on one
machine. The per-field epsilon in DiffCommandLists is for comparisons across
builds or against the executor, where the last float digit can legitimately vary.

### The comparison (command_diff.h)

`DiffCommandLists(ref, actual, epsilon) -> optional<CommandDivergence{index,
reason}>` checks variant type, compares int, id, flag and color fields exactly,
and compares floats within epsilon. It returns the first divergence with a
readable reason, or nullopt. It is both the golden-check primitive and the
executor's parity check.

## The executor blueprint

Goal: separate deciding from issuing. The callbacks stop calling the device
directly. They record into a command list (which they already can), and a
standalone `D3D9Executor` consumes the list and issues the device calls. CI then
exercises the render logic by building a list and snapshotting or diffing it
(no GPU), and the executor is validated locally by byte-comparing its output
against the current direct path.

### Proposed shape

- `RenderCommand` gains the state fields the direct calls currently derive
  inline. SetLayerCmd already carries blend_mode and the HSV desc; it must also
  carry the resolved `Blend::D3d9Blend` and the mode-3 multiply-stage flag, so
  the executor does not re-run backend logic. The decisions stay in the
  recorder, and the executor only issues.
- `void D3D9Executor::Execute(IDirect3DDevice9*, const RenderCommand&, const
  GpuContext&)`: one `std::visit` dispatch, each arm issuing the exact device
  sequence the matching callback issues today (lift those ~53 calls verbatim
  into the executor arms). Texture bind and shader selection read GpuContext
  (device, textures[], afp_hsl_ps/afp_add_ps, hsv_scope rects).
- Each callback then builds the command and, during the transition, calls
  `Execute(...)` immediately, so behaviour stays the same while the code moves.
  Once every callback records and then executes, the "record the whole frame,
  execute the list in one pass" mode is a flag flip.

### Verification plan (mandatory, this is the hot path)

1. Move each callback's device sequence into its executor arm, with the callback
   calling `Execute` inline. Byte-compare the IIDX 12-frame dump and the DDR bg_0009
   --ddr-test PNG (the standing stash-dance recipe: build old, dump, pop,
   build new, dump, SHA). They must be identical, since this is pure code motion.
2. Only once (1) is byte-clean, add the deferred "execute the recorded list
   at end of frame" path behind a flag and byte-compare again.
3. The GPU-less CI test: FakeGameRuntime (P8) drives the recorder -> snapshot
   the golden -> DiffCommandLists against a committed reference. This step makes
   the command stream a hosted-CI regression gate.

### Risks

- Hot path: any divergence in the lifted device sequence changes pixels
  silently. Byte-compare every step on both backends, as CLAUDE.md requires.
- LayerCommand case 4 (AFPU effect objects drawing via their own vtables) does
  not go through our device calls. It stays an opaque executor pass-through
  (record a LayerCmdCmd, execute by calling the same afpu vtable path). The
  command stream cannot fully capture those draws; document the gap.
- State that persists across draws (g_last_setlayer_blend, current_matrix,
  the sampler-filter tracking) must be threaded through the executor or kept
  in GpuContext. The executor must not read backend file-statics.

## Implementation state

Recording and comparison: implemented, unit-tested, byte-verified.

Executor (`RenderExec::Execute`, src/render_executor.{h,cpp}): step 1 covers
SetLayerCmd, SetBlendCmd, MaskCmd and DrawCmd. Each callback builds the
typed command and calls the matching `Execute` inline, so the device sequence is
unchanged while the issuing side sits behind the command seam. The DrawCmd arm is
the whole SubmitGeometry issue path (texture bind from the recorder-resolved
`cmd.tex_slot`, shape-color c4, `Render::AfpPrimCount` prim mapping and the
zero-prim early-out, matrix c0..c3, HSL / additive-coverage shader setup and the
post-draw restores, DrawPrimitiveUP). The recorder keeps the decisions: it
resolves the texture slot, gates the HSV and additive filters (only for a drawable
prim, matching the old post-zero-prim order), packs the pixel-shader constants into
`DrawCmd.hsv_c1` / `hsv_c3`, and records the DrawCmd only for a non-zero prim.

Verified byte-clean: the DDR bg_0009 12-frame `--ddr-test` dump is SHA-identical
across the stash-dance (covering bind / fallback / shape-color / prim / zero-prim /
matrix / mask / blend across hundreds of draws). The HSL and additive-coverage
sub-paths (qpro only, not hit by DDR) were shown character-identical to the
pre-slice callback by a source diff of the lifted device calls. 27 r573_render
test cases (incl. `AfpPrimCount`) and all four gates pass.

LayerCmdCmd arm: the device-relevant sub-dispatches (3 = nested vtable,
4 = the AFPU effect-object device hand-off, 5 = fn-ptr pair) moved verbatim into
the executor. The live AFPU pointers a2/a3/a4 travel next to the recorded
command as side-channel args (like DrawCmd's verts) because they cannot be
captured for replay; this is the documented LayerCommand replay gap. Subs
0/1/2 and the unknown-sub screams stay in the recording callback, because
AFP_SCREAM_UNIMPL captures _ReturnAddress to map the afp-core caller offset,
which exists only in the callback frame. LayerCommand never fires in practice
for title.ifs (0 calls over 15s); the
whole callback is a registered safety net, so no live content byte-compares the
dispatch itself. Verification is therefore a 24-frame byte-compare of the flows
that do run (SDVX7 select_bg_vi 1080x1920 and IIDX33 02005.ifs, 12 raw .bgra
frames each, SHA-identical across the stash-dance, which shows the rewired
callback and the unconditional executor call change nothing), plus a source
diff of the moved dispatch against HEAD (identical apart from the g_gpu ->
gpu/device operand renames).

Step 1 is complete: every AfpD3D9 render callback records its typed command and
issues it through RenderExec.

Step 2 (deferred whole-frame replay) is complete. `--deferred-replay` sets
GpuContext.deferred_replay. The callbacks record into GpuContext.frame_cmds
(verts always copied, zero-prim draws included so the replay reproduces the
live counters) and issue nothing inline. EndRender replays the frame through
`RenderExec::ExecuteList` (still inside the BeginFrame scene bracket), then
clears the list (BeginRender also clears it, in case a frame aborted mid-record).
Replaying at end of frame needed three state hazards closed:

- The layer matrix changes per layer within a frame (SetMatrix), so DrawCmd now
  carries it (`matrix` + `matrix_ready`, captured at record time). The Draw arm
  uploads cmd.matrix, never gpu.current_matrix, and DiffCommandLists compares it
  (epsilon).
- The in_mask_write matte-skip flag is decided at record time (it gates what
  SubmitGeometry lets into the stream), so the SetMaskRegion callback now owns
  it. The Mask executor arm only issues scissor state.
- SetLayer records the filter descriptor only when HsvFilterActive accepts it
  (id 100/101 + valid). afp hands over its reused filter work struct even for
  filter ids we never consume (e.g. id 192), whose bytes are not in
  HsvDescriptor layout and hold heap junk that differs per run. Recording them
  made the --cmd-trace golden nondeterministic across identical runs.

LayerCommand subs 3/4/5 still execute inline under deferred replay, because the
live AFPU pointers cannot be stored (the documented replay gap). That moves them
ahead of the frame's deferred draws, so a one-shot AFP_SCREAM_UNIMPL fires if
content ever hits them in this mode (no known content does).

Step-2 verification (all byte-identical): flag off against pre-slice = 24/24 frames
(SDVX7 select_bg_vi + IIDX33 02005); deferred on against off = 24/24 frames; the
--cmd-trace golden is identical between live and deferred runs and is now
deterministic across repeated identical runs. The DDR backend is untouched
(its own callbacks, mask flag and render loop).

Step 3 (the GPU-less CI recorder gate) is complete. The `command_stream_tests`
target (tests/render/command_stream_gate_tests.cpp) links the real recorder TUs
(afp_d3d9_callbacks.cpp, afp_d3d9_commands.cpp, render_executor.cpp). They
depend only on the gated libs plus the header-inline g_gpu, so they link into a
test binary directly. They compile without r573::warnings there because they
are monolith sources, while the test TU itself is tidy-scoped via tests/*. The
target drives a fixed synthetic frame through the actual callbacks with a NULL
device: `deferred_replay = true` puts the recorder in record-only mode, and the
device boot-guards were relaxed to `!device && !deferred_replay` so recording
works without a GPU. Real runs behave the same, since the device is never null
while afp renders (byte-verified 12/12 SDVX frames across the stash-dance).

The fixed frame covers SetLayer/SetBlend records, a TRIANGLESTRIP draw with
UVs from the geo blob, a TRIANGLELIST draw with the flag-packed
uv+color+xy vertex stream and a legacy tex-ref miss, the mask bracket (op0
matte draw dropped / op1 rect / op2), a zero-prim draw (recorded for replay
only), and LayerCommand (bit0-off no-op + a recorded sub-2 stub). There are four
checks:

- The FormatCommandList text equals the committed golden
  (tests/render/goldens/command_stream_frame.golden). A missing golden is
  written and the test fails for review, so blessing is an explicit local step.
- Structural counts between tap and frame list (matte drop, zero-prim replay-only,
  LayerCmdCmd tap-only).
- Run-to-run determinism via DiffCommandLists.
- A perturbed recording is flagged.

The golden locks the recorder's real decisions (vertex snap, matte suppression,
prim mapping, tex resolve, per-vertex color decode) in hosted CI with no game
DLLs, content or GPU. Two paths are not covered without a GPU: the HSL/additive
shader gating (needs live shader objects) and the tex-resolve hit path (needs
live texture objects). The local pixel byte-compares still cover both.

All three executor steps are complete. The command stream is recorded (step
1), replayable (step 2) and gated in CI (step 3). Possible extension: drive the
recorder from a FakeGameRuntime once P8 builds one, widening the synthetic
frame into full scene scripts.

## Step 4 (P10): the WARP pixel golden

`pixel_golden_tests` (tests/render/pixel_golden_tests.cpp + src/warp_device.{h,cpp})
closes the executor's device-side coverage gap in hosted CI. The recorder gate
(step 3) locks what gets recorded; this test locks what the executor draws. It
records a fixed synthetic frame through the real callbacks (deferred_replay
record-only, the same deferred path production uses), then replays it via
`RenderExec::ExecuteList` on a real D3D9 device backed by software: d3d9.dll's
`Direct3DCreate9On12` with a D3D12 WARP device (`IDXGIFactory4::EnumWarpAdapter`
-> `D3D12CreateDevice`). All of these are bound dynamically (LoadLibrary/GetProcAddress),
so there are no new link deps beyond d3d9.lib. If any bring-up step fails (pre-1903
Windows, no d3d9on12) the test is skipped and names the failing step, which is
safe under `ctest -L ci`.

The scene renders at 320x240 A8R8G8B8 over a cleared background:

- a vertex-color-gradient quad (blend mode 0)
- an overlapping mode-1 quad
- a textured quad that hits a real registered texture (legacy tex_ref path:
  ref&0xFFFF +1 = slot; a 4x4 checker at slot 7 with dims registered so
  ResolveBoundTexture hits)
- a full-screen translucent quad clipped by the mask bracket (op1 scissor,
  op2 restore)
- a zero-prim draw (replay no-op)

Untextured draws bind the white 2x2 fallback texture. The executor runs
fixed-function (afp_vs null). The test supplies the base state (modulate stage0,
POINT samplers so results match across WARP versions, the alpha-blend defaults)
plus an orthographic PROJECTION transform mapping pixel space to clip space,
because without the vs.1.1 shader the D3DFVF_XYZ vertices go through the
fixed-function transform. This test does not cover the HSL/additive pixel-shader
arms: they need D3DXCompileShader at runtime, which CI runners lack, so the local
byte-compare net still covers them.

Both gates share `tests/render/scene_support.h`, the header-only scene-script
vocabulary (quad/prim/xyz-skip-flag submit builders over the real callbacks), so
new coverage is written once and locked twice, as a recorder text golden and as a
WARP pixel hash. The first shared script is `DriveWideScene`: blend modes
2/3/4 (multiply, inv-src-color, add), prim types 1/2/6 (LINELIST, LINESTRIP,
TRIANGLEFAN), the xyz vertex flag (0x02) combined with the skip-2 flag (0x10),
and the SetLayer mode-3 stage-1 multiply path (TFACTOR/BLENDCURRENTALPHA) over
a textured quad. Goldens: `command_stream_wide.golden` +
`pixel_golden_wide.sha`. The recorder gate runs it without textures (tex resolve
miss recorded); the pixel gate runs it with the checker registered (executor
draws the hit).

Verification inside the test: the frame is rendered twice and byte-compared
(determinism within a run), then FNV-1a-64-hashed against a baseline local to the
machine in `local_baselines/` (gitignored, same policy as render_regression). The
hashes are not committed. WARP rasterization is deterministic for a given WARP
build but differs between machines, as hosted CI showed: the windows-2022 runner's
WARP produced different pixels than the dev machine for the identical command
list, while each was deterministic on its own. A missing baseline is blessed, and
the test warns and passes. CI runners have throwaway workspaces, so in hosted CI
the test checks that device replay succeeds and that the run is deterministic; the
hash regression gate is the machine-local suite, where the baseline persists
across builds. On a mismatch the actual frame is dumped to the build dir for
review. A local hash change with no code change is a Windows WARP update (review
and re-bless); a change that comes with a code change is a regression. The
recorder text goldens stay committed, because they do not depend on the machine
and passed on CI unchanged. The include-cleaner ignore list gained
`unknwnbase.h` (IUnknown's canonical definer; unknwn.h is only a wrapper, and the
checker refuses to credit it).
