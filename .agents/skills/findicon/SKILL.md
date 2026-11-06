---
name: findicon
description: Search Nerd Fonts icons with the findicon CLI, inspect PNG candidate sheets, and resolve exact Unicode codepoints for UI or ImGui use.
---

# Findicon

Use the offline CLI to shortlist icons, visually compare their glyphs, and
return a codepoint tied to the actual font and catalog. The CLI retrieves
candidates; you make the visual choice.

## Locate the tool

Use `findicon` on `PATH` or the user's FindIcon checkout. In the original
checkout this skill lives at `.agents/skills/findicon`; the repository root
is three directories above the skill folder. A copied skill does not imply
that the executable or fonts were copied with it. Locate the known checkout
or ask for its path if neither checkout nor executable is available.

Check `--version` and `--help`. If a build is needed, run from the checkout:

```sh
cmake --preset release
cmake --build --preset release --parallel
```

This needs CMake 3.24+, a C++20 compiler, and network access on the first build
to fetch pinned dependencies. With Visual Studio the output is
`build/release/Release/findicon.exe`; with a single-configuration generator
it is `build/release/findicon`. Invoke its absolute path when working in a
different project. Runtime needs the adjacent `assets` directory (or an
installed `../share/findicon` directory); override with `--data-dir` if needed.
Do not install dependencies or change global configuration just to discover
icons. Report a missing prerequisite when the existing environment cannot build.

## Search and inspect

- Translate the requested purpose into a few short English terms. Prefer
  one icon family already used by the target UI. Run `families --text` if
  the available family names are unknown. Terms use OR matching, not AND;
  multiple matches improve rank. Use `--family` to narrow the visual style.
- Search in compact text first, then preview 12–24 candidates with the same
  query and filters. Use the intended UI pixel size and actual font when
  available. Put outputs in a temporary or ignored output directory.

```sh
findicon search settings gear cog --family cod,md --limit 12 --text
findicon preview settings gear cog --family cod,md --limit 12 --size 20 --out /absolute/output/Settings.png --text
```

On Windows, use a Windows output path and invoke an absolute executable path
with PowerShell's `&` when necessary. `preview` always saves a companion JSON,
even with `--text`, which keeps stdout small.

- Open the PNG with an image-viewing tool. Compare meaning, line weight, and
  the actual-size sample. JSON, terminal glyphs, filenames, and OCR alone do
  not establish visual suitability. If image viewing is unavailable, return
  the shortlist and paths without claiming a visually verified selection.
- Choose a cell with `supported: true` and `clipped: false`. Re-preview a
  smaller shortlist with repeated `--id` values if helpful. Explicit `--id`
  cannot be combined with queries, family filters, or pagination.
- If nothing fits, change terms or family, or browse with
  `preview --all --family cod --limit 24 --offset 24`. Bound the page size;
  do not dump the full catalog into context.

Exit 2 means invalid input/I/O; 3 means no matches; 4 with `--strict` means
the files exist but some glyphs are missing or clipped. Inspect those flags
before selecting. Do not treat a failed invocation's old output as a new sheet.

## Resolve and hand off

Read the selected cell's `index` in **that PNG's companion JSON**. Extract
its `id`, `codepoint`, `codepoint_decimal`, and `utf8`, together with
`catalog_version` and `preview.font`/`font_version`/`pixel_size`. The cell
number is local to that sheet. Do not derive a codepoint from the image,
memory, a ranking position from another search, or the icon's name.

Confirm with `findicon info <selected-id>` when producing source code.
Retain the selected ID, codepoint, catalog version, font identity, intended
UI use, and brief visual rationale in the consuming project's existing icon
mapping or the response, according to the user's task. Do not change a
consumer's source unless that change is within the request.

For ImGui:

- Load or merge the same font, and include the selected glyph in persistent
  ranges if required by the consumer's ImGui version. Preview support does
  not establish that the consumer loads the font correctly.
- Values above `U+FFFF` need `IMGUI_USE_WCHAR32` consistently in ImGui and
  consumers. Preserve the full Unicode scalar value; never cast to 16 bits.
- For a `const char*` API, derive ordinary C++ `"\xNN..."` literals from the
  JSON `utf8` bytes. C++20 `u8` literals use `char8_t`. Separate any following
  label into another literal so hexadecimal escapes cannot consume it.
- Report final UI rendering as unverified unless you actually inspect it.

The tool and this skill are MIT licensed; bundled fonts retain their original
licenses. The Nerd Fonts MIT wrapper does not make every glyph MIT. Consult
the checkout's `THIRD_PARTY.md` and applicable upstream notices when handing
off an icon or redistributing its font; do not promise an MIT-only font bundle.
