# Verification record

Verified on 2026-11-06.

## Current Windows requirements review

The later Windows review ran the CLI on Windows x64 using Visual Studio 18
2026 (MSVC), CMake 4.4.0, and the pinned static dependencies. Both CTest
entries (`findicon.tests` and `findicon.cli`) passed. Catch2's JUnit output
reported zero failures, errors, or skips and 179 successful assertions.

Generated and opened `Settings.png` with 12 candidates at 64 px / 20 px.
The selected first cell was supported and unclipped. Its companion JSON
and independent `info` lookup both resolved `nf-cod-settings_gear` to
`U+EB51`, UTF-8 bytes `EE AD 91`. BMP and supplementary-plane lookup also
worked from the installed executable outside the installation directory.

The install included both READMEs, the reusable MIT-licensed skill, and
the added upstream font notices. The bundled skill passed skill-creator's
`quick_validate.py`; PyYAML 6.0.3 was isolated under the ignored review output
directory for that validation. No global Python environment was changed.

Local review evidence is under `out/review/`: initial-file backups and hashes,
build/CTest logs, Catch2 JUnit XML, generated PNG/JSON, installation outputs,
and skill validation results. This directory is ignored and is not part of
the source distribution. Final source hashes and a delta against the initial
untracked files are recorded there for review because this checkout had no
initial commit. Existing untracked files were not staged or committed.

No consuming ImGui application was changed or rendered. Performance was not
benchmarked; the review did not change the CLI's search or raster code.
Third-party notices are retained; this is not a claim that the font bundle
has been relicensed under MIT or a certification of all upstream rights.

## Earlier supplied verification

The following Linux and cross-compilation results were already present in
the checkout. They were not re-executed during the Windows review. The
earlier limitation about not running the Windows executable applies to
that earlier environment only.

## Native build and execution

- Platform: Linux x86-64; GCC 13, C++20; CMake 4.4.4 and Ninja.
- FreeType 2.15.0, libpng 1.6.59, and zlib 1.4 built from the pinned source
  archives in `cmake/Dependencies.cmake` and linked statically.
- Release build completed without findicon compiler warnings.
- CTest: both `findicon.tests` and `findicon.cli` passed.
- Catch2: 25 test cases, **24 passed and 1 skipped**; **176 assertions passed**.
  The skipped case requires a filesystem that does not distinguish filename
  case; the verification filesystem distinguishes case.
- CLI integration covered exact alias lookup, supplementary Unicode,
  Japanese keyword search, Unicode/spaced output paths, numbered PNG/JSON
  mapping, malformed options, empty result preservation, input overwrite
  prevention, empty path rejection, and failed stdout writes.
- Output tests forced a real filesystem rename failure and verified rollback
  of the previous PNG/JSON pair. Incomplete generation leaves both old files
  unchanged; abandoned staging files are cleaned up.
- Installed executable ran successfully from outside the installation's
  working directory, locating its catalog through `../share/findicon`.
- Native executable references system libstdc++, libgcc, libm and libc; it does
  not reference external FreeType, libpng, or zlib shared libraries.

## Assets and visual checks

- Nerd Fonts catalog and Symbols Only font are both release **3.5.1**.
- Catalog: **10,995 names / 10,617 distinct codepoints**.
- The font cmap contains all **10,617** indexed codepoints; missing count **0**.
- Asset byte sizes and SHA-256 hashes are recorded in `assets/Manifest.json`.
- Opened real generated PNGs for dark and light sheets, BMP and supplementary
  icons, blank glyphs, and missing glyphs. Labels are readable and complete;
  no label overlap or drawing across cell boundaries was observed.
- The bundled Settings example was rendered with 12 candidates, four columns,
  64 px inspection samples and 20 px actual-size samples, with `--strict`.

## Windows x64 build

- Cross-compiled with the official **llvm-mingw 20261104 UCRT** toolchain,
  Clang **23.1.5**, target `x86_64-w64-windows-gnu`.
- Toolchain archive SHA-256:
  `919380403de87dca3c581c2053230cd298af5e891286628ba0613b7fed370926`.
- The release asset's published digest matched the downloaded archive.
- Executable format: **PE32+ console / AMD64**.
- Executable size: **2,964,480 bytes**.
- Executable SHA-256:
  `2d7cc2636cdcdab02af44807a814ff92086b405903f9c727944f116f64d359fb`.
- PE imports: `KERNEL32.dll` and `api-ms-win-crt-*.dll` only.
- No external FreeType, libpng, zlib, libc++, libunwind, or winpthread DLL
  imports. Delay import table is empty.
- The portable archive includes the executable, catalog, fonts, usage
  instructions, and dependency/runtime license notices.

**The Windows executable has not been run on Windows in this environment.**
Build, PE architecture, dependencies, and package contents were checked;
runtime/visual tests were executed with the native Linux build.

## Scope

The tool previews individual Unicode glyphs. DokuText's shaping, ligatures,
multi-codepoint emoji, SVG rendering, and custom COLRv1 paint graph are not
part of this extraction. Missing or unsupported glyphs are explicitly marked
in the preview and manifest. The Nerd Fonts use case uses the supported
monochrome rendering path.
