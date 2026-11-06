# findicon

[Japanese / 日本語](README.ja.md)

A small, offline C++20 tool for finding Nerd Font icons and comparing their
actual appearance. Search the official names, render a numbered PNG contact
sheet, and resolve a selected number through the accompanying JSON.

The CPU raster path is extracted from
[DokuText](https://github.com/poicurr/DokuText/tree/1db338c4ded4e76e54f4ceb46062ad2c632c900f).
The executable uses FreeType and libpng (with zlib). It does not need a browser,
GPU, display server, Python runtime, HarfBuzz, or the rest of DokuText.

## Quick start

Clone the repository and build the CLI (see the compiler requirements below):

```sh
git clone https://github.com/poicurr/FindIcon.git
cd FindIcon
cmake --preset release
cmake --build --preset release --parallel
```

Open this checkout in Codex and ask, for example:

```text
Use $findicon to choose a settings icon for a 20 px ImGui toolbar.
Inspect the preview and return the exact codepoint and C++ UTF-8 string.
```

The repository includes the [findicon skill](.agents/skills/findicon/SKILL.md).
Codex discovers it when working inside this checkout. To use it from another
project, copy the entire `.agents/skills/findicon` folder into that project's
`.agents/skills/`, or into `~/.agents/skills/` for personal use. Keep this
checkout available and tell Codex its location, or put the built CLI on `PATH`.
See [Codex skill discovery](https://developers.openai.com/codex/skills/).
No global settings or MCP server is required.

With the `release` preset, the executable is
`build/release/Release/findicon.exe` on Windows with Visual Studio, or
`build/release/findicon` with a single-configuration generator. Use that path
in place of `findicon` below until it is on `PATH`.

Keep the executable and its `assets` directory together. All assets are
included: the Nerd Fonts 3.5.1 index and Symbols Only font, plus a separate
DejaVu Sans label font. The index contains 10,995 names and 10,617 distinct
codepoints. All indexed codepoints are present in the bundled icon font.

```sh
findicon search settings gear --family cod,md --limit 24
findicon preview settings gear --family cod,md --out Settings.png --text
findicon info nf-md-cog
```

On Windows, use `./findicon.exe` (PowerShell) or `findicon.exe` (cmd) in these
examples. A source build places it in `build/Release` with multi-configuration
generators, or `build` with single-configuration generators.

`preview` writes **both** `Settings.png` and `Settings.json`. The JSON records
the exact ID/codepoint of every numbered cell. It is also printed to stdout by
default; `--text` prints a compact summary instead.

## Build from source

Requirements: a C++20 compiler, CMake 3.24 or newer, and a build tool. On
Windows, install the Visual Studio C++ Build Tools (MSVC and Windows SDK).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The default configuration downloads pinned source releases of FreeType,
libpng, and zlib during the first configure and builds them statically.
Network access is not needed when running `findicon`. See
`cmake/Dependencies.cmake` for the exact source versions and checksums.

To use already installed libraries instead:

```sh
cmake -S . -B build -DFINDICON_FETCH_DEPS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

That configuration requires the development packages for FreeType and libpng
and follows their normal shared/static linkage. With vcpkg, pass its CMake
toolchain file and set `FINDICON_FETCH_DEPS=OFF`. On MSVC, match the runtime:
use `x64-windows-static` with the default static runtime, or set
`FINDICON_STATIC_RUNTIME=OFF` for an `/MD` triplet such as `x64-windows`.

Install into a separate prefix:

```sh
cmake --install build --config Release --prefix out
```

The executable locates its data relative to its own path, so it can run from
another working directory. Portable builds use `assets` beside the executable;
installed builds use `../share/findicon`. Override that with
`FINDICON_DATA_DIR` or `--data-dir`.

## Commands

### Search

```sh
findicon search rotate rotation orbit --family cod,md --limit 24
findicon search settings --out SettingsSearch.json
findicon search U+F0493
findicon search --all --family cod --limit 24 --offset 24
findicon families --text
```

- Multiple terms are OR-matched. Exact IDs rank above complete names,
  word-boundary matches, prefixes, and substrings. Multiple matched terms add
  a ranking bonus.
- Names are ASCII case-insensitive. Both `nf-md-cog` and `md-cog` work.
- Matching codepoints are deduplicated. Other names remain in `aliases`.
- Family filtering happens before alias grouping, so a `cod` result is not
  represented by an alias from another family.
- `--limit` defaults to 24 and is bounded to 1–256; `--offset` pages through
  ranked unique-codepoint results. `--all` still honors these limits.
- Empty queries require `--all`. Unknown families and malformed arguments
  produce an error instead of an unrestricted search.

The following Japanese keywords expand into a small built-in dictionary:
`設定`, `移動`, `回転`, `拡縮`, `拡大`, `縮小`, `保存`, `削除`, `検索`, `表示`,
`非表示`, `再生`, `停止`. Separate several keywords with spaces. This is a
keyword helper, not a natural-language or embedding search engine.

### Preview

```sh
findicon preview settings gear --family cod,md --out Settings.png --size 20
findicon preview 回転 拡縮 --family md --out Transform.png --theme light
findicon preview --id nf-cod-settings_gear --id U+F0493 --out Selected.png
findicon preview --all --family cod --offset 24 --out Codicons02.png
findicon preview settings --font /path/to/ActualUiFont.ttf --out Actual.png
```

Every cell shows a number, the official icon ID, the codepoint, a large sample
(64 px by default), and an actual-size sample (20 px by default). Samples use
the requested FreeType pixel size and glyph metrics, without scaling each
outline to fill its cell. Title text uses a separate font.

Useful options:

| Option | Default | Purpose |
| --- | --- | --- |
| `--size` | `20` | Actual-size sample, 8–96 px |
| `--large-size` | `64` | Inspection sample, 8–256 px |
| `--columns` | `4` | Sheet columns, 1–8 |
| `--theme` | `dark` | `dark` or `light` |
| `--out` | `Candidates.png` | PNG path; parent directories are created |
| `--manifest` | PNG stem + `.json` | Exact numbered JSON mapping |
| `--font` | bundled Symbols Only | Actual font to inspect |
| `--label-font` | bundled DejaVu Sans | Latin font for labels |
| `--strict` | off | Exit 4 when any glyph is missing or clipped |

Explicit `--id` selections preserve input order and deduplicate codepoints.
They cannot be combined with query, family, or pagination options. IDs must
exist in the loaded catalog. Use another catalog through `--data` to inspect
other single Unicode glyphs.

The JSON includes `supported` and `clipped` per candidate, bounding rectangles
for each cell, real-size advance/bearings, font family/style/version, image
dimensions, and the exact font paths. Missing characters are visibly marked;
the font's `.notdef` box is never silently presented as a valid icon.

Large pages or unusually large fonts can exceed bounded image allocations.
Use smaller `--limit`, `--large-size`, or multiple pages in that case.

### Inspect an exact ID

```sh
findicon info nf-fa-gear md-cog U+F0493
```

Exact lookup accepts `nf-` names, short names, `U+...`, `0x...`, bare 4–6 digit
hexadecimal codes, and a single UTF-8 character. Supplementary-plane values
are handled as Unicode scalar values and are not truncated to 16 bits.

## Use from Codex or another agent

1. Expand the requested purpose into several short search terms.
2. Run `preview` for a small candidate set, with the real UI font and size
   when available.
3. Open the generated PNG with an image-viewing tool.
4. Compare meaning, clarity at the actual size, and consistency with the UI.
5. Read the selected cell from **that image's JSON**; retain its `id` and
   `codepoint`. The cell number is local to one image, not a permanent ID.
6. Save the chosen mapping in the consuming project. Search again if no
   candidate fits.

The packaged skill also explains tool discovery, building, and ImGui integration.
`AGENTS.md` contains operational guidance for local agents. No
automatic visual quality claim or final icon recommendation is generated by
the CLI itself.

## Use the selected codepoint in ImGui

Retain the selected `id`, `codepoint`, `catalog_version`, and the font used in
the preview. A private-use codepoint has meaning only with the matching font;
the same codepoint in an unrelated font can display a different symbol.

For example, `findicon info nf-cod-settings_gear` resolves to `U+EB51` in the
bundled catalog. Its UTF-8 bytes can be used as an ordinary C++ `char` string:

```cpp
// Nerd Fonts 3.5.1, nf-cod-settings_gear, U+EB51.
constexpr char SETTINGS_ICON[] = "\xEE\xAD\x91";
ImGui::Button(SETTINGS_ICON);
```

Load `SymbolsNerdFont-Regular.ttf` into ImGui's font atlas, either separately
or merged into your UI font with `ImFontConfig::MergeMode`. Follow
[Dear ImGui's font instructions](https://github.com/ocornut/imgui/blob/master/docs/FONTS.md)
for your version: older atlas workflows need persistent glyph ranges that
include the selected codepoint. Codepoints above `U+FFFF`, such as `nf-md-cog`
at `U+F0493`, require `IMGUI_USE_WCHAR32` consistently across ImGui and its
consumers. Do not truncate them into 16-bit values. In C++20, a `u8` literal
has type `char8_t[]`, so it cannot be passed directly to ImGui's `const char*`
API. Ordinary escaped UTF-8 byte strings avoid that mismatch; split adjacent
text into another literal, e.g. `"\xEE\xAD\x91" " Settings"`.

The preview verifies the selected font's glyph at the requested size. Check
the final UI too, since ImGui's rasterizer, hinting, DPI, and layout may differ.

## Exit status and machine output

| Code | Meaning |
| --- | --- |
| `0` | Success |
| `2` | Invalid arguments, catalog/font errors, or output I/O failure |
| `3` | No candidates at the requested query/page |
| `4` | `--strict` preview contains missing/clipped glyphs; PNG and JSON exist |

JSON is the default stdout format for search, info, families, and preview.
Diagnostics use stderr. `--text` switches stdout to a readable summary. Search,
info, and families can also save JSON with `--out`; preview always saves its
companion JSON. An empty preview does not overwrite existing output files.

Output paths are checked against the loaded input catalog and fonts, including
symlink/hardlink aliases. The PNG and its manifest must be different paths.
Both files are first written to temporary locations. A normal write/rename
failure preserves or restores the previous pair. This is not a cross-file
atomic transaction against power loss, process termination, or concurrent
external changes; consume outputs only after a successful tool invocation.

## Tests

```sh
cmake -S . -B build -DFINDICON_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Tests use Catch2 and cover catalog validation, exact lookup, aliases, search
ranking, pagination, supplementary Unicode, alpha composition, glyph bitmap
conversion, missing-glyph handling, and PNG output. Rendering changes should
also be verified by opening an actual generated sheet.

## Scope and provenance

This tool inspects **single Unicode glyphs**. It does not reproduce DokuText's
text shaping, ligatures, bidirectional layout, multi-codepoint emoji sequences,
font fallback chain, or COLRv1 paint-graph renderer. Its primary supported
content is the monochrome Nerd Fonts catalog.

The DokuText extraction retains the RGBA/FreeType/libpng architecture and fixes
straight-alpha composition, signed bitmap pitch, and the C++/libpng longjmp
boundary. See `thirdparty/DokuText/README.md` for the exact extraction record.

| Component | Source and notices |
| --- | --- |
| DokuText raster code | Commit `1db338c4ded4e76e54f4ceb46062ad2c632c900f`; `thirdparty/DokuText/` |
| Nerd Fonts data/font | Release `v3.5.1`; `thirdparty/NerdFonts/` |
| Latin labels | DejaVu Sans; `thirdparty/DejaVu/Copyright.txt` |
| nlohmann/json | Release `v3.12.0`; `thirdparty/nlohmann/LICENSE.MIT` |
| Build dependencies | Pinned sources in `cmake/Dependencies.cmake` |

`assets/Manifest.json` records the bundled assets' source URLs, sizes, and
SHA-256 hashes. Update the Nerd Fonts JSON and font together. Independent
upstream icon sets retain their own licenses, listed in the upstream Nerd
Fonts README shipped with this project.

## License

The findicon code, documentation, and findicon skill are distributed under
the [MIT License](LICENSE). The skill includes its own copy of that license
so it can be copied independently.

**The bundled fonts and third-party components are not all MIT licensed.**
Nerd Fonts combines icon sets under MIT, CC BY 4.0, Apache 2.0, OFL, and other
terms; its top-level MIT license does not replace those terms. DejaVu Sans
uses the Bitstream Vera license with public-domain DejaVu changes. Preserve
the applicable notices when redistributing fonts or using their icons.
See [third-party notices](THIRD_PARTY.md) and the bundled upstream notices.
The full bundle must not be advertised as MIT-only. A strictly MIT-only font
bundle would require replacing the fonts and catalog with verified MIT assets.
