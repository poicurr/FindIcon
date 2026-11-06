// CPU grid composition follows DokuText's VisualPngScenarioTest.cpp.
// See thirdparty/DokuText/README.md for the pinned source and license.
#include "Preview.hpp"

#include "Font.hpp"
#include "DokuText/PngWriter.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace findicon {
namespace {

using raster::Bitmap;
using raster::Color;
using raster::Font;
using raster::GlyphSurface;
using raster::Rect;

constexpr std::size_t MAX_CANDIDATES = 256;
constexpr int MAX_COLUMNS = 8;
constexpr int OUTER_PADDING = 20;
constexpr int GAP = 12;
constexpr int HEADER_HEIGHT = 88;
constexpr int CARD_PADDING = 12;
constexpr int NAME_SIZE = 13;
constexpr int NAME_LINE_HEIGHT = 17;
constexpr int ICON_TOP = 36;

struct Theme {
  Color m_background;
  Color m_card;
  Color m_sample;
  Color m_border;
  Color m_text;
  Color m_muted;
  Color m_accent;
  Color m_warning;
  Color m_guide;
};

[[nodiscard]] Theme makeTheme(bool light) {
  if (light) {
    return {{243, 246, 250, 255}, {255, 255, 255, 255}, {248, 250, 253, 255},
             {209, 218, 229, 255}, {24, 36, 51, 255}, {84, 102, 122, 255},
             {0, 99, 143, 255}, {158, 63, 0, 255}, {219, 226, 235, 255}};
  }
  return {{15, 21, 30, 255}, {25, 35, 48, 255}, {20, 28, 40, 255},
           {50, 67, 87, 255}, {235, 241, 249, 255}, {153, 173, 196, 255},
           {115, 214, 240, 255}, {255, 179, 105, 255}, {44, 61, 80, 255}};
}

[[nodiscard]] std::string shorten(Font &font, std::string text, int pixelSize,
                                  int width) {
  if (font.textWidth(text, pixelSize) <= width) {
    return text;
  }
  while (!text.empty() && font.textWidth(text + "...", pixelSize) > width) {
    std::size_t begin = text.size() - 1;
    while (begin > 0 &&
           (static_cast<unsigned char>(text[begin]) & 0xC0U) == 0x80U) {
      --begin;
    }
    text.resize(begin);
  }
  return text + "...";
}

[[nodiscard]] std::vector<std::string> wrapName(Font &font,
                                                const std::string &name,
                                                int width) {
  if (name.empty() || name.size() > 256U) {
    throw std::invalid_argument("Preview icon IDs must contain 1 to 256 bytes");
  }
  // Official Nerd Fonts IDs are ASCII. Keep their exact spelling and wrap at
  // underscore/hyphen boundaries whenever possible; the manifest retains ID.
  if (!std::all_of(name.begin(), name.end(), [](unsigned char value) {
        return value >= 0x20U && value <= 0x7EU;
      })) {
    throw std::invalid_argument("Preview icon IDs must be printable ASCII");
  }
  std::vector<std::string> lines;
  std::size_t begin = 0;
  while (begin < name.size()) {
    std::size_t end = begin;
    std::size_t breakAt = begin;
    while (end < name.size() &&
           font.textWidth(std::string_view(name).substr(begin, end - begin + 1),
                           NAME_SIZE) <= width) {
      ++end;
      if (name[end - 1] == '_' || name[end - 1] == '-') {
        breakAt = end;
      }
    }
    if (end == begin) {
      throw std::runtime_error("Label font cannot fit one character in a card");
    }
    if (end < name.size() && breakAt > begin + (end - begin) / 2) {
      end = breakAt;
    }
    lines.emplace_back(name.substr(begin, end - begin));
    begin = end;
  }
  return lines;
}

void drawText(Font &font, Bitmap &canvas, std::string_view text, int x,
              int baseline, int size, Color color, const Rect &clip) {
  (void)font.drawText(canvas, text, x, baseline, size, color, clip);
}

void drawCenteredText(Font &font, Bitmap &canvas, std::string_view text,
                      const Rect &area, int baseline, int size, Color color) {
  const int left = area.m_x + (area.m_width - font.textWidth(text, size)) / 2;
  drawText(font, canvas, text, left, baseline, size, color, area);
}

[[nodiscard]] bool hasInk(const Bitmap &bitmap) {
  const auto &rgba = bitmap.rgba();
  for (std::size_t index = 3; index < rgba.size(); index += 4) {
    if (rgba[index] != 0) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool drawGlyph(Bitmap &canvas, Font &font, Font &labels,
                              const GlyphSurface &glyph, int pixelSize,
                              const Rect &area, const Theme &theme) {
  canvas.fillRect(area, theme.m_sample);
  canvas.strokeRect(area, theme.m_border);
  const Rect inner{area.m_x + 1, area.m_y + 1, area.m_width - 2,
                    area.m_height - 2};
  if (!glyph.m_supported) {
    drawCenteredText(labels, canvas, "unavailable", inner,
                      area.m_y + area.m_height / 2 + 4, 11, theme.m_warning);
    return false;
  }
  if (!hasInk(glyph.m_image)) {
    drawCenteredText(labels, canvas, "blank glyph", inner,
                      area.m_y + area.m_height / 2 + 4, 11, theme.m_muted);
    return false;
  }

  const auto metrics = font.metrics(pixelSize);
  const int lineHeight = std::max(1, metrics.m_ascender - metrics.m_descender);
  const auto baseline = static_cast<std::int64_t>(area.m_y) +
                         (area.m_height - lineHeight) / 2 + metrics.m_ascender;
  const auto originX = static_cast<std::int64_t>(area.m_x) +
                        (static_cast<std::int64_t>(area.m_width) -
                         glyph.m_advance) /
                            2;
  const auto left = originX + glyph.m_bearingX;
  const auto top = baseline - glyph.m_bearingY;
  constexpr std::int64_t MAX_POSITION = 2000000;
  if (left < -MAX_POSITION || left > MAX_POSITION || top < -MAX_POSITION ||
      top > MAX_POSITION) {
    throw std::length_error("Glyph placement exceeds the supported range");
  }
  if (baseline > inner.m_y && baseline < inner.m_y + inner.m_height) {
    canvas.fillRect({inner.m_x, static_cast<int>(baseline), inner.m_width, 1},
                     theme.m_guide);
  }
  return canvas.blitClipped(glyph.m_image, static_cast<int>(left),
                             static_cast<int>(top), inner,
                             glyph.m_colored ? Color{255, 255, 255, 255}
                                              : theme.m_text);
}

} // namespace

PreviewResult renderPreview(const std::vector<Candidate> &candidates,
                              const PreviewOptions &options) {
  if (candidates.size() > MAX_CANDIDATES) {
    throw std::length_error("A preview can contain at most 256 candidates; use "
                             "pagination for larger selections");
  }
  if (options.m_columns < 1 || options.m_columns > MAX_COLUMNS) {
    throw std::invalid_argument("Preview columns must be between 1 and 8");
  }
  if (options.m_pixelSize < 1 || options.m_pixelSize > Font::MAX_PIXEL_SIZE ||
      options.m_largePixelSize < 1 ||
      options.m_largePixelSize > Font::MAX_PIXEL_SIZE) {
    throw std::invalid_argument("Preview pixel sizes must be between 1 and 512");
  }
  if (options.m_fontPath.empty() || options.m_labelFontPath.empty() ||
      options.m_outputPath.empty()) {
    throw std::invalid_argument("Preview requires icon font, label font, and "
                                 "output paths");
  }

  Font font(options.m_fontPath);
  Font labels(options.m_labelFontPath);
  for (std::uint32_t codepoint = 32; codepoint <= 126; ++codepoint) {
    if (!labels.contains(codepoint)) {
      throw std::runtime_error("The label font must contain printable ASCII; "
                               "choose a text font, not Symbols Only");
    }
  }
  const Theme theme = makeTheme(options.m_light);
  const int columns = std::min(
      options.m_columns, static_cast<int>(std::max<std::size_t>(candidates.size(), 1)));
  const int rows = static_cast<int>((candidates.size() + columns - 1) / columns);
  const int smallWidth = std::max(84, options.m_pixelSize * 2 + 16);
  const int cardWidth =
      std::max(288, options.m_largePixelSize * 2 + smallWidth + 32);
  const int largeWidth = cardWidth - CARD_PADDING * 2 - smallWidth - 8;
  const int iconHeight =
      std::max(options.m_pixelSize, options.m_largePixelSize) + 24;
  const int sizeBaseline = ICON_TOP + iconHeight + 14;
  const int nameBaseline = sizeBaseline + 22;

  std::vector<std::vector<std::string>> names;
  names.reserve(candidates.size());
  std::size_t maximumNameLines = 1;
  for (const auto &candidate : candidates) {
    names.push_back(wrapName(labels, candidate.m_icon.m_id,
                             cardWidth - 2 * CARD_PADDING));
    maximumNameLines = std::max(maximumNameLines, names.back().size());
  }
  const int codeBaseline =
      nameBaseline + static_cast<int>(maximumNameLines) * NAME_LINE_HEIGHT + 11;
  const int cardHeight = codeBaseline + 13;
  const auto width = static_cast<std::int64_t>(OUTER_PADDING) * 2 +
                     static_cast<std::int64_t>(columns) * cardWidth +
                     static_cast<std::int64_t>(columns - 1) * GAP;
  const auto height = static_cast<std::int64_t>(HEADER_HEIGHT) + OUTER_PADDING +
                      (rows == 0 ? 100 : static_cast<std::int64_t>(rows) *
                                             cardHeight +
                                         static_cast<std::int64_t>(rows - 1) * GAP);
  if (width > Bitmap::MAX_DIMENSION || height > Bitmap::MAX_DIMENSION) {
    throw std::length_error("Preview dimensions exceed 16384 pixels; reduce "
                             "sizes, columns, or the candidate count");
  }

  Bitmap canvas;
  canvas.resize(static_cast<int>(width), static_cast<int>(height));
  canvas.clear(theme.m_background[0], theme.m_background[1],
                theme.m_background[2], theme.m_background[3]);
  const Rect canvasBounds{0, 0, canvas.width(), canvas.height()};
  drawText(labels, canvas, "findicon", OUTER_PADDING, 33, 24, theme.m_text,
            canvasBounds);
  const std::string countText = std::to_string(candidates.size()) + " candidates";
  drawText(labels, canvas, countText,
            canvas.width() - OUTER_PADDING - labels.textWidth(countText, 13), 31,
            13, theme.m_accent, canvasBounds);
  const auto fontDescription = shorten(
      labels, font.family() + " / " + font.style() +
                  (font.version().empty() ? "" : " / " + font.version()),
      12, canvas.width() - 2 * OUTER_PADDING);
  drawText(labels, canvas, fontDescription, OUTER_PADDING, 56, 12, theme.m_muted,
            canvasBounds);
  const auto legend = shorten(labels,
      "Fixed font sizes; common baseline per size. IDs match the JSON manifest.",
      11, canvas.width() - 2 * OUTER_PADDING);
  drawText(labels, canvas, legend, OUTER_PADDING, 74, 11, theme.m_muted,
            canvasBounds);

  PreviewResult result;
  result.m_width = canvas.width();
  result.m_height = canvas.height();
  result.m_fontFamily = font.family();
  result.m_fontStyle = font.style();
  result.m_fontVersion = font.version();
  result.m_cells.reserve(candidates.size());

  if (candidates.empty()) {
    drawText(labels, canvas, "No matching icons.", OUTER_PADDING,
              HEADER_HEIGHT + 40, 16, theme.m_muted, canvasBounds);
  }
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const auto &candidate = candidates[index];
    const int column = static_cast<int>(index % columns);
    const int row = static_cast<int>(index / columns);
    const int x = OUTER_PADDING + column * (cardWidth + GAP);
    const int y = HEADER_HEIGHT + row * (cardHeight + GAP);
    const Rect card{x, y, cardWidth, cardHeight};
    canvas.fillRect(card, theme.m_card);
    canvas.strokeRect(card, theme.m_border);

    const auto &largeGlyph =
        font.glyph(candidate.m_icon.m_codepoint, options.m_largePixelSize);
    const auto &smallGlyph =
        font.glyph(candidate.m_icon.m_codepoint, options.m_pixelSize);
    PreviewCell cell;
    cell.m_id = candidate.m_icon.m_id;
    cell.m_x = x;
    cell.m_y = y;
    cell.m_width = cardWidth;
    cell.m_height = cardHeight;
    cell.m_supported = largeGlyph.m_supported && smallGlyph.m_supported;
    cell.m_advance = smallGlyph.m_advance;
    cell.m_bearingX = smallGlyph.m_bearingX;
    cell.m_bearingY = smallGlyph.m_bearingY;

    const Rect largeArea{x + CARD_PADDING, y + ICON_TOP, largeWidth, iconHeight};
    const Rect smallArea{x + CARD_PADDING + largeWidth + 8, y + ICON_TOP,
                         smallWidth, iconHeight};
    const bool largeClipped = drawGlyph(canvas, font, labels, largeGlyph,
                                         options.m_largePixelSize, largeArea, theme);
    const bool smallClipped = drawGlyph(canvas, font, labels, smallGlyph,
                                         options.m_pixelSize, smallArea, theme);
    cell.m_clipped = largeClipped || smallClipped;

    std::ostringstream number;
    number << std::setw(2) << std::setfill('0') << index + 1;
    drawText(labels, canvas, number.str(), x + CARD_PADDING, y + 24, 16,
              theme.m_accent, card);
    const std::string status =
        !cell.m_supported
            ? (font.contains(candidate.m_icon.m_codepoint) ? "UNSUPPORTED" : "MISSING")
            : (cell.m_clipped ? "CLIPPED" : candidate.m_icon.m_family);
    drawText(labels, canvas, status,
              x + cardWidth - CARD_PADDING - labels.textWidth(status, 11), y + 23,
              11, cell.m_supported && !cell.m_clipped ? theme.m_muted : theme.m_warning,
              card);
    drawCenteredText(labels, canvas,
                       std::to_string(options.m_largePixelSize) + " px",
                       {largeArea.m_x, y + ICON_TOP + iconHeight,
                         largeArea.m_width, 20},
                       y + sizeBaseline, 11, theme.m_muted);
    drawCenteredText(labels, canvas,
                       std::to_string(options.m_pixelSize) + " px (actual)",
                       {smallArea.m_x, y + ICON_TOP + iconHeight,
                         smallArea.m_width, 20},
                       y + sizeBaseline, 11, theme.m_muted);
    for (std::size_t line = 0; line < names[index].size(); ++line) {
      drawText(labels, canvas, names[index][line], x + CARD_PADDING,
                y + nameBaseline + static_cast<int>(line) * NAME_LINE_HEIGHT,
                NAME_SIZE, theme.m_text, card);
    }
    drawText(labels, canvas, formatCodepoint(candidate.m_icon.m_codepoint),
              x + CARD_PADDING, y + codeBaseline, 12, theme.m_muted, card);
    result.m_cells.push_back(std::move(cell));
  }

  if (!raster::writePng(options.m_outputPath, canvas)) {
    throw std::runtime_error("Failed to write PNG preview: " +
                             options.m_outputPath.string());
  }
  return result;
}

} // namespace findicon
