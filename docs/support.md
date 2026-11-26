# r573_support (src/support/)

The lowest layer: OS and COM resource wrappers, error-handling vocabulary,
DLL loading, and logging. It depends on nothing else in the project (only Win32
and tl-expected), and everything else links it. The P4 engine seam and the P6
ownership work are built on it.

## ModuleHandle (module_handle.h)

`ModuleHandle` is `std::unique_ptr<HMODULE, ModuleCloser>` where
`ModuleCloser::pointer = HMODULE`. Declaring the deleter's `pointer` type lets a
unique_ptr own a non-pointer handle type and store it inline, so the smart
pointer is exactly one HMODULE wide and `.get()` returns the handle
directly. `LoadModule(path)` wraps `LoadLibraryA`; a failed load yields a
null handle that compares equal to nullptr. This replaces every hand-written
`FreeLibrary` in the codebase: a DllLoader that goes out of scope, is
reset, or is reassigned frees its module automatically.

## Expected (expected.h)

`Support::Expected<T, E>` aliases `tl::expected<T, E>` and
`Support::Unexpected(e)` aliases `tl::make_unexpected`. This is the
project's return type for recoverable errors. It aliases the vcpkg
`tl-expected` header-only library because MSVC ships `std::expected` only
under `/std:c++23`, and the project is C++20. When the language mode moves
to C++23, switching this one header to `std::expected` / `std::unexpected`
is the whole migration, with no call site changes. Error handling policy:
`Expected` for recoverable failures at API boundaries, exceptions only for
construction failures that cannot be recovered, and SEH kept isolated in
render_seh (see docs/boot_and_render_loop.md), never crossing into
C++ unwinding.

Adoption so far: the file-level entry points of `formats/ddr_arc`
(`ReadToc(path)`, `ExtractFirstIfs(path, out_name)`) return
`Expected<..., std::string>`. `ExtractFirstIfs` used to return an
empty vector both for "file has no .ifs" and for "read/decompress failed",
and a caller could not tell them apart. The span-level parsers across
`formats/` keep the convention most of them use (`bool` + out-param + `std::string&
err` where the failure has useful detail). New file-level APIs that can fail
should prefer `Expected`.

## ComPtr (com_ptr.h)

A minimal move-only RAII guard for COM interfaces. It is not a full CComPtr;
it only prevents leaks. One contract matters: `operator&()` resets
(releases) before returning `&ptr`, so it is safe to pass `&comptr` to a
create call that overwrites the slot. `GetAddressOf()` does not reset,
so use it only when the slot is known to be null. `Detach()` gives up ownership
without releasing the interface (for handing a raw pointer to code that
takes ownership).

The header also carries `ComInit`, a non-copyable guard that calls
`CoInitializeEx(MULTITHREADED)` on construction and `CoUninitialize` on
destruction only when its own init succeeded. The WIC helper structs
(`WicPngTarget` in media_sink.cpp and qpro_extract.cpp, `WicDecodeTarget`
in afp_d3d9_textures.cpp) are plain aggregates of `ComInit` and `ComPtr`
members. `ComInit` is declared first, so reverse-order member
destruction releases every interface before COM shuts down. Those structs
used to hand-roll the rule of five and per-member Release chains; now the
member order is the whole contract.

## FolderJob (folder_job.h)

The shared runner for background folder-batch tools (the .arc extractor and
the customize-asset extractor had byte-identical copies of this scaffolding
before it existed). `FolderJob<StatusT>` owns the mutex-guarded status
snapshot (`Publish`/`Get`), the start-once atomic (`Start` detaches a
thread only when one is not already running; `Finish` clears the flag, and the run
function must call it on every exit path), and `IsRunning`. Free helpers:
`ReadFileBytes` (whole-file read, empty on failure), `StripTrailingSlashes`
(input-folder normalization), and `ScanFolder(root, progress, entry)`, the
recursive walk with skip_permission_denied, per-entry error clearing, and a
progress callback every `kScanProgressEvery` (512) entries so the GUI never
looks frozen mid-scan (the progress rule in CLAUDE.md). The entry callback
receives the iterator instead of the path, so a consumer can call
`disable_recursion_pending()` to prune (the customize extractor prunes its
own output dir).

## DllLoader (dll_loader.h / .cpp)

Loads a Konami DLL (owning it via ModuleHandle) and resolves its obfuscated
exports. The export-naming classification is split into two pure,
unit-tested free functions, so the PE walk is the only part that cannot be tested:

- `ClassifyFirstExport(name)` decides the DLL's export scheme from its
  first export name. There are two schemes:
  - Mangled (avs2-core, modern afp-core, DDR's libavs-win64): names are
    `<prefix>NNNNNN` where the trailing 6 characters are the hex ordinal,
    e.g. `XCnbrep7000129`. A name counts as mangled when it ends in exactly 6
    hex digits and has at least one prefix character. Returns the prefix;
    resolution is by ordinal.
  - Readable (DDR World's libafp-win64 / libafputils-win64, AFP 2.13.7):
    plain names like `afp_boot`, `afp_do_render`. This is anything that does not
    have the mangled shape. Resolution is by the `name` argument, and the
    ordinal is ignored.
- `FormatMangledExport(prefix, ordinal)` builds `prefix + %06x(ordinal)`,
  the symbol looked up with GetProcAddress in mangled mode.

`DetectPrefix()` parses the PE export directory to read the first export
name and caches `NumberOfNames` as `num_exports_`, a coarse version
fingerprint, since different game and build versions of the same DLL export
different counts. `GetFunc(ordinal, name)` resolves by the detected scheme;
`name` is used for the readable path and for the warning log when a symbol
does not resolve. The `DLL_LOAD(loader, field, ord)` macro is the shorthand
every `*_funcs.h` uses to fill a typed function pointer:
`field = loader.GetFunc<decltype(field)>(ord, "field")`.

The module has a per-directory `.clang-tidy` (src/support/.clang-tidy)
that removes the pro-type-vararg / reinterpret-cast / pointer-arithmetic /
macro-usage checks and disables const-correctness pointer analysis. This is
the FFI layer, and its job is GetProcAddress casts, PE pointer walking
and the DLL_LOAD macro. Following the no-NOLINT decision, the suppression lives
in one config file instead of on individual lines. Every future FFI or adapter
module (avs, afp) follows the same pattern.

## Log (log.h / .cpp)

`printf`-style logging with a tag prefix, mirrored to `renderer.log`
(relative to the CWD, unbuffered so a crash loses nothing) and to a console that
`Init()` allocates. The renderer is a WIN32-subsystem app, so stdout would
otherwise be dropped, which once made DLL load failures invisible in the log.

`SetSink(fn, user)` is the capture seam: when a sink is installed, `Write`
formats the line into a fixed buffer and hands `(tag, message, user)` to the
sink instead of touching files or stdout. A unit test uses it to capture log
output (support_tests does this), and a future GUI could use it to route logs
into a panel. `LOG(tag, fmt, ...)` and `LOG_ONCE(...)` (fires
once per call site) are unchanged. Internal state lives behind a
function-local static accessor instead of file globals, so the tidy
non-const-global check passes without being weakened for the whole project.

## Env (env.h / .cpp)

`Support::EnvVar`, `EnvFlag` and `EnvInt` read process environment variables
through `getenv_s`, the bounds-checked variant, so no call site trips MSVC
C4996 (the `getenv` deprecation). The DDR and qpro paths read dozens of debug
and tuning knobs (`DDR_*`, `QPRO_LIMIT`), and the renderer target now links
`r573::warnings`, which turns `getenv` into a warning.

- `EnvVar(name)` returns `std::optional<std::string>`: the value when set
  (even if empty), `nullopt` when unset. Internally it sizes with a first
  `getenv_s(&needed, nullptr, 0, name)` call, then fills a `needed`-sized buffer
  and drops the trailing NUL.
- `EnvFlag(name)` checks presence only (`EnvVar(name).has_value()`). It
  replaces the old `getenv("X") != nullptr` idiom.
- `EnvInt(name)` is `atoi` of the value, or `nullopt`. Use `.value_or(def)`
  in place of the old `e ? atoi(e) : def` idiom. It is base 10 (`atoi`), so the few
  knobs that took hex via `strtol(e, 0, 0)` (`DDR_POLICY`, `DDR_SYS_ATTR`,
  `DDR_AFPU_ATTR`, `DDR_LAYER_ATTR`) call `EnvVar` and keep their own
  `strtol` on `.c_str()`. Likewise `DDR_TIME_SCALE` (atof) and `DDR_NEARFAR`
  (`sscanf_s`) parse the `EnvVar` string themselves.

## Deterministic float trigonometry (math/float_trig.h / .cpp)

`Support::Sinf(float)` and `Support::Cosf(float)` are the project's `sinf` and
`cosf`. Everything whose output is compared bit for bit across machines calls
them instead of `std::sin` / `std::cos`: the preset evaluator's orbit position
(`eval_models.cpp`), its `sine_deg` ease (`eval_tween.cpp`) and its emitter ring
phase and scatter angle (`eval_particles.cpp`).

### Why the CRT could not stay

`sinf` and `cosf` are not exactly specified. Every libm may return its own
last bit, and Microsoft's does not return the same one on every machine. The
golden preset recording is a per-frame hash of the push text, and the push text
prints floats through `std::to_chars`'s shortest round-tripping form, so it
records the float bits exactly. A difference of one ulp is therefore visible.

An example from this repo's own data: the IIDX 10 class course select
preset orbits `cube_x` at `rate = 0.033333335`, so frame 733 has
`angle = 24.4333344`:

| | `sinf(24.4333344)` | printed `y = 0.2 + sin * 0.2` |
|---|---|---|
| this machine's UCRT (VS 2026 / v14.51) | `0xbf24cdb7` | `0.071247205` |
| the GitHub `windows-2022` runner | `0xbf24cdb6` | `0.07124722` |
| correctly rounded | `0xbf24cdb6` | `0.07124722` |

That one ulp failed CI at frame 732 (the transform pushed on frame N
is the pose for frame N+1) while every local run passed. Across all 1211 orbit
angles of that preset, this machine's CRT differs from the correctly rounded
result at four, and the difference reaches the printed text at three; the
first of those is the frame CI reported. The cause is inside the CRT and not in
the build: the project passes no `/fp` or `/arch` flag, so both sides compile at
`/fp:precise` with the x64 SSE2 baseline, and the object file for this
translation unit contains no FMA instruction (checked with `dumpbin /disasm`).
`/fp:precise` is set explicitly on this one source file in `CMakeLists.txt`,
so a future global `/fp:fast` cannot silently contract the argument reduction.

### What it is

A comment-free transcription of the FreeBSD `msun` float trigonometry, which is
the same code musl carries:

| Vendored here | Upstream |
|---|---|
| `Sinf`, `Cosf` | `lib/msun/src/s_sinf.c`, `s_cosf.c` |
| `KernelSin`, `KernelCos` | `lib/msun/src/k_sinf.c`, `k_cosf.c` |
| `Reduce` | `lib/msun/src/e_rem_pio2f.c` (`__ieee754_rem_pio2f`) |
| `ReduceLarge` and its helpers | `lib/msun/src/k_rem_pio2.c` (`__kernel_rem_pio2`) |

Provenance and licence are kept here because the repo's sources carry no
comments:

> Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
> Developed at SunPro, a Sun Microsystems, Inc. business.
> Permission to use, copy, modify, and distribute this software is freely
> granted, provided that this notice is preserved.

with the float conversion by Ian Lance Taylor (Cygnus Support) and the
optimisation by Bruce D. Evans, as the upstream headers state.

`__kernel_rem_pio2` is specialised in two ways, because `sinf`/`cosf` are
its only callers. `nx` is always 1 (the input is one 24-bit chunk, so the inner
convolution over `x[]` collapses to a single multiply), and `prec` is always 0
(so `jk` is the constant 3, the `init_jk` table is gone, and only the `case 0`
compression remains; the `PIo2` table keeps the first four of its eight terms,
which is `jp + 1`). Everything else is transcribed line for line: the same
polynomial coefficients, the same branch structure and the same evaluation order,
and that is what makes the result reproducible. `std::scalbn` and `std::floor` stay
CRT calls on purpose. Both are exactly specified operations with no freedom in
rounding, so no two implementations can disagree about them.

The implementation is deterministic by construction, and that property cannot
be tested from a single machine: it is pure C++
with no runtime CPU dispatch, no ISA-specific path and no fused multiply-add.
What can be tested on one machine is that no codegen setting changes the result,
and none does. An FNV hash of `Sinf` and `Cosf` over the same 2000000 pseudo-random bit
patterns (776307 of them through the large-argument path) is `e7abfead807e0b1a`
for each of x64 `/O2`, x86 `/O2`, x64 `/O2 /arch:AVX2` (FMA-capable
codegen), x64 `/Od`, and x86 `/arch:IA32`, which runs the whole thing on the
x87 stack with 80-bit intermediates. The CRT could not produce that table.

### It is also more correct than the 32-bit CRT

The x86 build showed that Microsoft's 32-bit `sinf` / `cosf` do not do
full-range argument reduction. Above the `0x4dc90fdb` threshold (`|x| ~ 2^28 *
pi/2`) they return values that are plainly wrong, well beyond a last-bit
difference. About a third of random float bit patterns are that large, and the
x86 CRT disagreed with a 140-digit reference on all of them, by up to 2.0 in a
function whose range is `[-1, 1]`. The vendored code agrees with the reference.
The x64 CRT does the reduction properly, which is why the problem stays hidden
until the 32-bit build runs.

For that reason the fuzz test compares against the CRT only below that threshold,
since above it the CRT is not a valid oracle on every target. The large-argument
path is covered by exact pins instead (`1e20`, `FLT_MAX` and three of the arguments
the x86 CRT gets wrong), each one checked against a 140-digit `Decimal`
computation of the true value.

### Why not a library

`vcpkg` was checked first, per the project rule.

- `sleef` was the first candidate and was rejected. Its scalar entry points are
  bound at library build time to either the `purec` or the `purecfma` variant, so
  the bits it returns depend on how the port was configured on the machine that
  built it. Build-time dispatch between an FMA and a non-FMA kernel is the
  failure this module exists to remove. It also needs a host-tool build for
  two functions.
- `fdlibm` is double-only, so a float result would come from rounding a double
  result instead of from the float algorithm. Its port fetches from
  `android.googlesource.com` with `vcpkg_from_git` at build time, and its header
  exports bare `sin` / `cos` names that collide with `<cmath>`.

About 200 lines of transcription with a pinned upstream and a licence notice
was judged a smaller risk than a dependency whose bit-level answer depends on
how it was built.

### Tests

`tests/support/float_trig_tests.cpp` (`support_tests`, `ci` label):

- `the deterministic sine and cosine hold their exact float bits` pins twelve
  arguments to exact `std::uint32_t` bit patterns, including `24.4333344` (the
  frame that failed CI), zero, `+/-1`, pi, an argument in each of the four
  reduction bands, `1e20` and `FLT_MAX` (both go through `ReduceLarge`). A CRT
  change on any machine cannot move a result without failing here.
- `the deterministic sine and cosine track the platform library` fuzzes random
  bit patterns below the large-argument threshold against `std::sin` /
  `std::cos` and requires the absolute gap to stay within `4 * FLT_EPSILON`.
  This catches a transcription error, which would be off by a whole number and
  not by a last bit. It uses an absolute gap on purpose, not an ulp
  distance in integer bit space: bit distance is meaningless across a
  sign change, and the tolerance has to hold on whatever CRT the runner has.

`tests/game/preset_eval_tests.cpp` pins the other end,
`the orbit position carries the same float bits on every machine`: the IIDX 10
class course select `cube_x` transform at frame 732 must be exactly
`[0xbecb3db6 0x3d91ea10 0x3fb33333]`. That test fails on the CRT and passes on
`Support::Sinf`; it reproduced the bug before the fix was made.

### The tidy exemption

`src/support/math/.clang-tidy` disables
`cppcoreguidelines-pro-bounds-constant-array-index` for this directory only. The
reduction indexes its working arrays by loop counters, which the check rejects
and which restructuring would make harder to read. Every other `src/` directory
already has that check off through `src/.clang-tidy`, and `src/support` is the
one that turns it back on. Nothing else is relaxed.
