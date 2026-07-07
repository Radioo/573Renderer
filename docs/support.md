# r573_support (src/support/)

The lowest layer: OS/COM resource wrappers, error-handling vocabulary,
DLL loading, and logging. Depends on nothing in the project (only Win32 +
tl-expected). Everything else links it. This is the substrate the P4 engine
seam and P6 ownership work build on.

## ModuleHandle (module_handle.h)

`ModuleHandle` is `std::unique_ptr<HMODULE, ModuleCloser>` where
`ModuleCloser::pointer = HMODULE` (the deleter typedef trick lets a
unique_ptr own a non-pointer handle type and store it inline, so the smart
pointer is exactly one HMODULE wide and `.get()` returns the handle
directly). `LoadModule(path)` wraps `LoadLibraryA`; a failed load yields a
null handle that compares equal to nullptr. This replaces every hand-written
`FreeLibrary` in the codebase - a DllLoader that goes out of scope, is
reset, or is reassigned frees its module automatically.

## Expected (expected.h)

`Support::Expected<T, E>` aliases `tl::expected<T, E>` and
`Support::Unexpected(e)` aliases `tl::make_unexpected`. This is the
project's recoverable-error return type. It is an alias over the vcpkg
`tl-expected` header-only library because MSVC ships `std::expected` only
under `/std:c++23`, and the project is C++20. When the language mode moves
to C++23, swapping this one header to `std::expected` / `std::unexpected`
is the entire migration - no call site changes. Error handling policy:
`Expected` for recoverable failures at API boundaries; exceptions only for
truly unrecoverable construction failures; SEH stays isolated in
render_seh (see docs/boot_and_render_loop.md) and never crosses into
C++ unwinding.

## ComPtr (com_ptr.h)

Minimal move-only RAII guard for D3D9 COM interfaces - not a full CComPtr,
just leak prevention. The one contract worth knowing: `operator&()` RESETS
(releases) before returning `&ptr`, so it is safe to pass `&comptr` to a
D3D9 create call that overwrites the slot; `GetAddressOf()` does NOT reset,
so use it only when the slot is known null. `Detach()` releases ownership
without releasing the interface (for handing a raw pointer to code that
takes ownership).

## DllLoader (dll_loader.h / .cpp)

Loads a Konami DLL (owning it via ModuleHandle) and resolves its obfuscated
exports. The export-naming classification is factored into two pure,
unit-tested free functions so the PE-walk stays the only untestable part:

- `ClassifyFirstExport(name)` decides the DLL's export scheme from its
  first export name. Two schemes exist:
  - Mangled (avs2-core, modern afp-core, DDR's libavs-win64): names are
    `<prefix>NNNNNN` where the trailing 6 characters are the hex ordinal,
    e.g. `XCnbrep7000129`. Detected by "ends in exactly 6 hex digits AND
    has at least one prefix character". Returns the prefix; resolution is
    by ordinal.
  - Readable (DDR World's libafp-win64 / libafputils-win64, AFP 2.13.7):
    plain names like `afp_boot`, `afp_do_render`. Anything that is not the
    mangled shape. Resolution is by the `name` argument, ordinal ignored.
- `FormatMangledExport(prefix, ordinal)` builds `prefix + %06x(ordinal)`,
  the symbol looked up with GetProcAddress in mangled mode.

`DetectPrefix()` parses the PE export directory to read the first export
name and caches `NumberOfNames` as `num_exports_` - a coarse version
fingerprint, since different game/build versions of the same DLL export
different counts. `GetFunc(ordinal, name)` resolves by the detected scheme;
`name` is used for the readable path and for the warning log when a symbol
does not resolve. The `DLL_LOAD(loader, field, ord)` macro is the shorthand
every `*_funcs.h` uses to fill a typed function pointer:
`field = loader.GetFunc<decltype(field)>(ord, "field")`.

The module carries a per-directory `.clang-tidy` (src/support/.clang-tidy)
that subtracts the pro-type-vararg / reinterpret-cast / pointer-arithmetic /
macro-usage checks and disables const-correctness pointer analysis: this is
the FFI layer whose entire job is GetProcAddress casts, PE pointer walking,
and the DLL_LOAD macro. Per the no-NOLINT decision, that suppression is
structural (one config file) rather than per-line. This is the template
every future FFI/adapter module (avs, afp) follows.

## Log (log.h / .cpp)

`printf`-style logging with a tag prefix, mirrored to `renderer.log`
(CWD-relative, unbuffered so a crash loses nothing) and to a console that
`Init()` allocates - the renderer is a WIN32-subsystem app, so stdout is
otherwise dropped, which once made DLL load failures invisible in the log.

`SetSink(fn, user)` is the capture seam: when a sink is installed, `Write`
formats the line into a fixed buffer and hands `(tag, message, user)` to the
sink instead of touching files or stdout. This is the seam that lets a unit
test capture log output (support_tests does exactly this) and lets a future
GUI route logs into a panel. `LOG(tag, fmt, ...)` and `LOG_ONCE(...)` (fires
once per call site) are unchanged. Internal state lives behind a
function-local-static accessor rather than file globals so the tidy
non-const-global check passes without weakening it project-wide.

## Env (env.h / .cpp)

`Support::EnvVar`, `EnvFlag`, `EnvInt` read process environment variables
through `getenv_s`, the bounds-checked variant, so no call site trips MSVC
C4996 (the `getenv` deprecation). This exists because the DDR and qpro paths
read dozens of debug/tuning knobs (`DDR_*`, `QPRO_LIMIT`) and the renderer
target now links `r573::warnings`, which turns `getenv` into a warning.

- `EnvVar(name)` returns `std::optional<std::string>` - the value when set
  (even if empty), `nullopt` when unset. Internally it sizes with a first
  `getenv_s(&needed, nullptr, 0, name)` then fills a `needed`-sized buffer
  and drops the trailing NUL.
- `EnvFlag(name)` is presence-only (`EnvVar(name).has_value()`) - the
  replacement for the old `getenv("X") != nullptr` idiom.
- `EnvInt(name)` is `atoi`-of-the-value or `nullopt` - use `.value_or(def)`
  for the old `e ? atoi(e) : def` idiom. NOTE it is base-10 (`atoi`): the few
  knobs that took hex via `strtol(e, 0, 0)` (`DDR_POLICY`, `DDR_SYS_ATTR`,
  `DDR_AFPU_ATTR`, `DDR_LAYER_ATTR`) call `EnvVar` and keep their own
  `strtol` on `.c_str()`. Likewise `DDR_TIME_SCALE` (atof) and `DDR_NEARFAR`
  (`sscanf_s`) parse the `EnvVar` string themselves.
