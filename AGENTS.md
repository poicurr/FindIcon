# findicon Development and Icon Selection

## Project

This is a standalone C++20 CLI. Its raster path is extracted from DokuText.
Keep runtime operation offline and keep GPU/windowing, HarfBuzz, Python, and
browser dependencies out of the executable. Dependencies used only to build
or test are documented in README.md.

Use LLVM formatting, SCREAMING_SNAKE_CASE constants, camelCase variables, and
m_/s_/g_ prefixes for class member/static/global variables. Preserve upstream
license notices and extraction provenance. Do not introduce CI.

## Selecting an Icon

Use `.agents/skills/findicon/SKILL.md` for the complete reusable workflow,
including CLI discovery and codepoint handoff to a consuming project.

1. Search by several short, related English words, or supported Japanese UI
   keywords. Example: `findicon search settings gear --family cod,md`.
2. Use `findicon preview` with the same query and filters to generate a PNG
   and its companion JSON. Start with 24 candidates and the real UI pixel size.
3. Open the PNG through an image-viewing tool. Reading JSON, a filename, or the
   glyph's UTF-8 bytes alone is not a visual inspection.
4. Choose only among cells with `supported: true`. Check `clipped`, the
   actual-size sample, line weight, meaning, and consistency with nearby UI.
5. Resolve the chosen numbered cell through that PNG's companion JSON. Do not
   reconstruct the codepoint from OCR or memory. Persist the `id`, `codepoint`,
   `catalog_version`, font, and intended UI use in the consuming project.
6. If none fits, try another query or family and inspect the new PNG. Use
   `--all --family cod --offset 24` for paginated visual exploration.

The short number is local to one output sheet. The stable identifier is `id`.
Matching codepoints are deduplicated; alternate names remain in `aliases`.
The tool retrieves candidates; the agent or user makes the visual choice.

## Verification

Build with `FINDICON_BUILD_TESTS=ON` and run CTest after changes to parsing,
Unicode, alpha composition, glyph positioning, or file output. Confirm at least
one real PNG visually after a rendering change. Keep generated outputs out of
the source tree. Preserve the matching Nerd Fonts JSON/font release pair.
