# DokuText rendering extraction

Source: https://github.com/poicurr/DokuText

Pinned upstream commit: `1db338c4ded4e76e54f4ceb46062ad2c632c900f`.

License: MIT, Copyright (c) 2026 poicurr. The complete upstream license is
included in `LICENSE`.

This directory adapts the CPU RGBA and PNG output path from:

- `src/core/ir/Bitmap.hpp` and `Bitmap.cpp`
- `src/core/ir/GlyphSurface.hpp`
- `src/adapter/io/PngWriter.hpp` and `PngWriter.cpp`

The FreeType conversion in `../../src/Font.cpp` is derived from
`renderMonochromeGlyph` and `renderBitmapSlot` in upstream `src/DokuText.cpp`.
The grid composition in `../../src/Preview.cpp` follows the CPU contact-sheet
approach in `tests/VisualPngScenarioTest.cpp`.

Changes made for findicon:

- Standalone `findicon::raster` namespace and no DokuText, HarfBuzz, atlas, or
  shaping dependency.
- Checked bitmap dimensions and a 128 MiB per-bitmap allocation limit.
- Correct straight-alpha source-over compositing, tinting, and clipping.
- Signed bitmap pitch, MONO/GRAY/GRAY2/GRAY4 conversion, and BGRA
  unpremultiplication.
- Missing/unsupported glyph status, common font metrics, and per-size caching.
- Unicode filesystem paths for fonts and PNG files, including Windows.
- PNG row storage and file ownership established before libpng's setjmp so
  error handling does not skip C++ destructors.

The extracted renderer previews individual Unicode codepoints. It does not
provide DokuText's complex-script shaping, emoji sequence shaping, custom
COLRv1 painting, SVG rendering, fallback chains, or text layout engine.
