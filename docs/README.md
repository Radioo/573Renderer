# 573Renderer documentation index

The codebase is comment-free by policy (see gates.md "No comments"): every
piece of RE knowledge, design rationale, and process history lives in these
documents, which are THE source of truth. Start here to find the right one.

## Orientation

| doc | read it when |
|-----|--------------|
| [boot_and_render_loop.md](boot_and_render_loop.md) | you need the big picture: main(), boot sequence, the per-frame loop, autopilot gates, window/WndProc contracts |
| [game_runtime.md](game_runtime.md) | working across the two engine generations (modern avs2 afp-core vs DDR AFP 2.13.7) - the `Runtime::IGameRuntime` seam |
| [game_profiles.md](game_profiles.md) | adding or debugging a game: the per-game data table (boot quirks, DLL names, offsets, detection) |
| [ownership.md](ownership.md) | who owns which state: EngineSession / GpuContext / ExportSession, the P6/P8 seams, and what was deliberately deferred |

## Engine binding and rendering

| doc | read it when |
|-----|--------------|
| [engine_binding.md](engine_binding.md) | resolving/calling Konami DLL exports: ordinal tables, offset sets, re-find recipes for new game builds |
| [d3d9_backend.md](d3d9_backend.md) | the D3D9 device layer: callback contracts, texture table, sampler filters, vertex snap/UV inset, screenshot timing |
| [command_stream.md](command_stream.md) | the recorded render-command seam: recorder, executor, deferred replay, the CI recorder golden and the WARP pixel golden |
| [blend.md](blend.md) | afp blend-mode -> D3D9 blend-state mapping (shared modern + DDR) |
| [ddr.md](ddr.md) | anything DDR World / AFP 2.13.7: boot, arc loading, draw path, playhead/label codes, the play-work table |
| [qpro.md](qpro.md) | the IIDX Q-pro avatar extractor: part tables, composites, game-id asset naming |
| [gui.md](gui.md) | GUI panel architecture and AFP debug-viewer parity features |

## Export and formats

| doc | read it when |
|-----|--------------|
| [export_pipeline.md](export_pipeline.md) | the capture-and-encode pipeline end to end: session lifecycle, stop detection (playhead-idle, wraps, safety caps), DDR loop detection |
| [loop.md](loop.md) | the r573_loop primitives: modern/DDR loop detectors, frame pacer, cyclers, blend-loop seam, and the sanctioned-pixel-exception evidence |
| [media_formats.md](media_formats.md) | export formats: the Format enum, tokens/extensions, stability contract, the CI encode net |
| [formats.md](formats.md) | pure parsers: IFS/arc containers, LZ77, DXT decode, BGRA frame ops |

## Module references (gated libs)

| doc | read it when |
|-----|--------------|
| [support.md](support.md) | r573_support: DLL loading, RAII wrappers, Expected, logging, env access |
| [state.md](state.md) | r573_state: App::State, the request queue, load progress, live overrides |
| [cli.md](cli.md) | r573_cli: argv parsing, tool subcommands, option semantics |
| [settings.md](settings.md) | r573_settings: settings.ini persistence |

## Process, build, and verification

| doc | read it when |
|-----|--------------|
| [build.md](build.md) | building: CMake presets, vcpkg, line endings, ffmpeg features |
| [ci.md](ci.md) | what CI runs and why |
| [gates.md](gates.md) | every quality gate: no-comments, tidy config chain, file length, format pins, gui isolation, machine paths |
| [local_regression.md](local_regression.md) | the local byte-compare net, machine-local baselines, and the local_dll real-DLL contract tier |
| [tidy_migration.md](tidy_migration.md) | the clang-tidy whole-tree migration (3113 -> 0 findings): method, fixit tooling, and per-check gotchas |
| [comment_migration.md](comment_migration.md) | the comment-knowledge migration: method, the source-file-to-doc knowledge map, and the open questions it surfaced |
