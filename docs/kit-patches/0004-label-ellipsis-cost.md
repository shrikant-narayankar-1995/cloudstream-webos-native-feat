# Kit patch 0004: bounded cost of ellipsising a long label

Applies to Native App Kit `e10f35ecd6e27987cc866b28caea6be66a023fcc` after patches 0001-0003.
Patch: `0004-label-ellipsis-cost.patch` (ui/src/widgets.cpp, tests/unit/test_ui.cpp).

## What
`Label::wrap()` shortens the last allowed line with an ellipsis. It used to take everything left in the
paragraph and drop one code point at a time, measuring the whole string each time, until it fit. It now
starts from the longest run of whole words that fits plus the next word (or, for a word broken across
lines, one more code point): nothing beyond that can fit on the line. Results are unchanged.

## Why
Measured on the TV (LG OLED65C1PTB, webOS 6.5.3) on 2026-10-20: with YouTube as the home source, the
home screen's preview shows the video description in a 3-line label. Descriptions run to thousands of
characters, and the main thread spent tens of seconds per layout in `Label::wrap` → `measure_text`
(18 of 18 stack samples from the app's stall sampler). The UI stopped answering for over a minute.
After the patch: no stall over 1.5 s on the same screen, frames 17-20 ms.

## Tests
`tests/unit/test_ui.cpp` "label: ellipsising a long paragraph measures about one line, not the whole
paragraph" (10,000-character paragraph and a 5,000-character word: under 100 and 200 measurements);
the whole host suite (`ctest --preset host-debug`, 107 tests) passes.
