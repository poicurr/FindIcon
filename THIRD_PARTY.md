# Third-party notices

The root MIT license covers findicon's original code, documentation, and
`.agents/skills/findicon`. It does not relicense third-party works. Keep this
file and the applicable upstream notices with redistributed bundles.

## Bundled assets

| Component | Version / source | Terms and local notices |
| --- | --- | --- |
| Nerd Fonts index and Symbols Only font | [v3.5.1](https://github.com/ryanoasis/nerd-fonts/tree/v3.5.1) | Nerd Fonts MIT wrapper in `thirdparty/NerdFonts/LICENSE`; underlying glyph sets retain their terms |
| DejaVu Sans label font | [DejaVu](https://dejavu-fonts.github.io/) | Bitstream Vera license and public-domain DejaVu changes; `thirdparty/DejaVu/Copyright.txt` |
| DokuText raster extraction | `1db338c4ded4e76e54f4ceb46062ad2c632c900f` | MIT; `thirdparty/DokuText/LICENSE` and extraction record in its README |
| nlohmann/json | 3.12.0 | MIT; `thirdparty/nlohmann/LICENSE.MIT` |

Asset URLs and hashes are recorded in `assets/Manifest.json`. The font files
are shipped unchanged. Renaming a file does not change the font's ownership
or license. The Debian packaging stanza in the DejaVu notice applies to
Debian packaging files, which are not bundled here.

## Nerd Fonts glyph sources

Nerd Fonts combines independently licensed glyphs. Its own MIT license
cannot be used as the license for every glyph. The unmodified
`thirdparty/NerdFonts/README.md` lists the upstream icon sets, versions, and
license labels. The upstream [license audit](https://github.com/ryanoasis/nerd-fonts/blob/v3.5.1/license-audit.md)
is also retained in `thirdparty/NerdFonts/upstream/license-audit.md`.
Copies of upstream notices are under `thirdparty/NerdFonts/upstream/`;
`NoticeSources.json` records their source URLs.

- Codicons: Microsoft; CC BY 4.0.
- Font Awesome: Fonticons, Inc.; icon artwork under CC BY 4.0. Its upstream
  notice also describes the font/code licenses; retain that notice.
- Material Design Icons: Pictogrammers and contributors; Apache 2.0.
- Octicons, Devicons, Font Awesome Extension, Seti/original icons, Hack extra
  glyphs, Powerline Extra, and IEC Power Symbols: upstream MIT notices.
- Powerline Symbols: upstream permissive notice.
- Pomicons and Weather Icons: SIL OFL 1.1, including reserved-name conditions
  where stated in their notices.
- Font Logos: the Nerd Fonts table calls this "unlicensed". The linked
  [Font Logos v1.3.0 license](https://github.com/lukas-w/font-logos/blob/v1.3.0/LICENSE)
  is the Unlicense public-domain dedication, retained alongside these notices.
  Logos may also be subject to their owners' trademark rights.

Keep the named sources and notices when redistributing the bundled font.
For selected icons, carry the applicable attribution/license into the
consumer's established third-party notices. The full bundled font retains
all its source obligations even when the application displays only one icon.
This project does not claim that the complete font bundle is MIT-only.

## Build dependencies

Pinned source URLs and SHA-256 checks are in `cmake/Dependencies.cmake`.
The fetched static build installs the following notices with the executable:

| Dependency | Terms | Installed notice directory |
| --- | --- | --- |
| FreeType | FreeType License (FTL), with GPL alternative retained | `share/findicon/licenses/FreeType` |
| libpng | libpng license | `share/findicon/licenses/libpng` |
| zlib | zlib license | `share/findicon/licenses/zlib` |

Portions of this software are copyright The FreeType Project
(https://freetype.org). All rights reserved.

Catch2 is used only by tests and is licensed under BSL-1.0. It is not linked
into the findicon executable. System-library builds and compiler runtimes
can have additional distribution requirements; retain their notices when
shipping them. The optional LLVM-MinGW packaging path copies its runtime
license notices when `LLVM_MINGW_ROOT` is provided.
