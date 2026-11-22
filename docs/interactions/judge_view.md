# Interpolation judge

Every interaction of the `--judge` window (docs/judge.md). Tests are
`tests/gui/judge_view_tests.cpp` unless named.

| # | id | control | input | tooltip | tests |
|---|---|---|---|---|---|
| 1 | `judge` | --judge <game-dir> | cli-arg | - | 1 (cli_tests) |
| 2 | `judge-profile` | --profile <slug> with --judge | cli-arg | - | 1 (cli_tests) |
| 3 | `judge-verdicts` | --judge-verdicts <file.json> | cli-arg | - | 1 (cli_tests) |
| 4 | `judge-clip` | --judge-clip <package>/<clip> | cli-arg | - | 1 (cli_tests) |
| 5 | `judge-capture` | --judge-capture <dir> | cli-arg | - | 1 (cli_tests) |
| 6 | `judge-capture-frames` | --judge-capture-frames <N> | cli-arg | - | 1 (cli_tests) |
| 7 | `judge-display-hz` | --judge-display-hz <hz> | cli-arg | - | 1 (cli_tests) |
| 8 | `clip-row` | a row of the clip list | left-click | - | 1 |
| 9 | `clip-prev-next` | Up / K, Down / J | key | - | 1 |
| 10 | `next-unjudged` | N | key | - | 1 |
| 11 | `play-pause` | Play / Pause button, Space | left-click, key | - | 1 |
| 12 | `step` | Step button, Right, `.` | left-click, key | - | 1 |
| 13 | `restart` | Restart button, R | left-click, key | - | 1 |
| 14 | `loop` | Loop checkbox, L | checkbox, key | - | 1 |
| 15 | `speed` | Speed segmented 1x .. 0.05x, `[` / `]` | left-click, key | - | 1 |
| 16 | `matrix-lerp` | Matrix lerp segmented Decompose / Linear, M | left-click, key | - | 1 |
| 17 | `verdict` | Smooth (1) / Wrong (2) / Unsure (3) buttons and keys | left-click, key | - | 1 |
| 18 | `next-after-verdict` | Next unjudged clip after a verdict | checkbox | - | 1 |
| 19 | `note` | the note field; keys above do nothing while it has focus | text-entry | - | 1 |
| 20 | `window-close` | closing the window ends the tool | window-message | - | **none** |
