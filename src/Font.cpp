// FreeType raster conversion adapted from poicurr/DokuText, commit
// 1db338c4ded4e76e54f4ceb46062ad2c632c900f, src/DokuText.cpp:
// renderMonochromeGlyph and renderBitmapSlot. See thirdparty/DokuText/LICENSE.
#include "Font.hpp"

#include FT_COLOR_H
#include FT_SFNT_NAMES_H

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace findicon::raster {
namespace {

constexpr int MAX_COORDINATE = 1000000;

[[nodiscard]] bool isScalar(std::uint32_t value) {
  return value <= 0x10FFFFU && (value < 0xD800U || value > 0xDFFFU);
}

void appendUtf8(std::string &text, std::uint32_t value) {
  if (!isScalar(value)) {
    value = 0xFFFDU;
  }
  if (value <= 0x7FU) {
    text.push_back(static_cast<char>(value));
  } else if (value <= 0x7FFU) {
    text.push_back(static_cast<char>(0xC0U | (value >> 6U)));
    text.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
  } else if (value <= 0xFFFFU) {
    text.push_back(static_cast<char>(0xE0U | (value >> 12U)));
    text.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
    text.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
  } else {
    text.push_back(static_cast<char>(0xF0U | (value >> 18U)));
    text.push_back(static_cast<char>(0x80U | ((value >> 12U) & 0x3FU)));
    text.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
    text.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
  }
}

[[nodiscard]] std::vector<std::uint32_t> decodeUtf8(std::string_view text) {
  std::vector<std::uint32_t> result;
  for (std::size_t index = 0; index < text.size();) {
    const auto first = static_cast<unsigned char>(text[index]);
    if (first <= 0x7FU) {
      result.push_back(first);
      ++index;
      continue;
    }
    unsigned int count = 0;
    std::uint32_t value = 0;
    std::uint32_t minimum = 0;
    if (first >= 0xC2U && first <= 0xDFU) {
      count = 2;
      value = first & 0x1FU;
      minimum = 0x80U;
    } else if (first >= 0xE0U && first <= 0xEFU) {
      count = 3;
      value = first & 0x0FU;
      minimum = 0x800U;
    } else if (first >= 0xF0U && first <= 0xF4U) {
      count = 4;
      value = first & 0x07U;
      minimum = 0x10000U;
    }
    bool valid = count != 0 && index + count <= text.size();
    for (unsigned int offset = 1; valid && offset < count; ++offset) {
      const auto next = static_cast<unsigned char>(text[index + offset]);
      valid = (next & 0xC0U) == 0x80U;
      value = (value << 6U) | (next & 0x3FU);
    }
    valid = valid && value >= minimum && isScalar(value);
    result.push_back(valid ? value : 0xFFFDU);
    index += valid ? count : 1U;
  }
  return result;
}

[[nodiscard]] std::string sfntName(FT_Face face, unsigned int nameId) {
  std::string result;
  int selectedPriority = -1;
  const auto count = FT_Get_Sfnt_Name_Count(face);
  for (FT_UInt index = 0; index < count; ++index) {
    FT_SfntName entry{};
    if (FT_Get_Sfnt_Name(face, index, &entry) != 0 || entry.name_id != nameId ||
        entry.string == nullptr || entry.string_len > 4096U) {
      continue;
    }
    const bool unicode = entry.platform_id == 0 || entry.platform_id == 3;
    const int priority = unicode ? (entry.language_id == 0x0409U ? 3 : 2) : 1;
    if (priority <= selectedPriority) {
      continue;
    }
    std::string decoded;
    if (unicode) {
      for (FT_UInt offset = 0; offset + 1 < entry.string_len; offset += 2) {
        std::uint32_t value =
            (static_cast<std::uint32_t>(entry.string[offset]) << 8U) |
            entry.string[offset + 1];
        if (value >= 0xD800U && value <= 0xDBFFU &&
            offset + 3 < entry.string_len) {
          const std::uint32_t low =
              (static_cast<std::uint32_t>(entry.string[offset + 2]) << 8U) |
              entry.string[offset + 3];
          if (low >= 0xDC00U && low <= 0xDFFFU) {
            value = 0x10000U + ((value - 0xD800U) << 10U) + (low - 0xDC00U);
            offset += 2;
          }
        }
        if (value >= 0x20U && value != 0x7FU) {
          appendUtf8(decoded, value);
        }
      }
    } else {
      // Legacy name encodings need additional converters for non-ASCII text.
      // Version strings and these fallback labels are restricted to ASCII.
      for (FT_UInt offset = 0; offset < entry.string_len; ++offset) {
        const auto value = entry.string[offset];
        if (value >= 0x20U && value <= 0x7EU) {
          decoded.push_back(static_cast<char>(value));
        }
      }
    }
    if (!decoded.empty()) {
      result = std::move(decoded);
      selectedPriority = priority;
    }
  }
  return result;
}

[[nodiscard]] int pixels(FT_Pos value) {
  const double rounded = std::round(static_cast<double>(value) / 64.0);
  if (rounded < -MAX_COORDINATE || rounded > MAX_COORDINATE) {
    throw std::runtime_error("Font metrics exceed the supported coordinate range");
  }
  return static_cast<int>(rounded);
}

[[nodiscard]] bool requiresColrV1Renderer(FT_Face face, FT_UInt glyphId) {
#if FREETYPE_MAJOR > 2 || (FREETYPE_MAJOR == 2 && FREETYPE_MINOR >= 11)
  FT_OpaquePaint paint{};
  return FT_Get_Color_Glyph_Paint(face, glyphId, FT_COLOR_NO_ROOT_TRANSFORM,
                                  &paint) != 0;
#else
  (void)face;
  (void)glyphId;
  return false;
#endif
}

} // namespace

Bitmap copyFreeTypeBitmap(const FT_Bitmap &source) {
  if (source.width > static_cast<unsigned int>(Bitmap::MAX_DIMENSION) ||
      source.rows > static_cast<unsigned int>(Bitmap::MAX_DIMENSION)) {
    throw std::length_error("Glyph bitmap dimensions exceed the supported limit");
  }
  Bitmap bitmap;
  bitmap.resize(static_cast<int>(source.width), static_cast<int>(source.rows));
  if (source.width == 0 || source.rows == 0) {
    return bitmap;
  }
  if (source.buffer == nullptr) {
    throw std::invalid_argument("Nonempty FreeType bitmap has a null buffer");
  }
  std::size_t minimumPitch = 0;
  switch (source.pixel_mode) {
  case FT_PIXEL_MODE_GRAY:
    if (source.num_grays < 2) {
      throw std::invalid_argument("GRAY bitmap has fewer than two gray levels");
    }
    minimumPitch = source.width;
    break;
  case FT_PIXEL_MODE_MONO:
    minimumPitch = (static_cast<std::size_t>(source.width) + 7U) / 8U;
    break;
  case FT_PIXEL_MODE_GRAY2:
    minimumPitch = (static_cast<std::size_t>(source.width) + 3U) / 4U;
    break;
  case FT_PIXEL_MODE_GRAY4:
    minimumPitch = (static_cast<std::size_t>(source.width) + 1U) / 2U;
    break;
  case FT_PIXEL_MODE_BGRA:
    minimumPitch = static_cast<std::size_t>(source.width) * 4U;
    break;
  default:
    throw std::runtime_error("Unsupported FreeType bitmap pixel mode");
  }
  const auto pitch64 = static_cast<std::int64_t>(source.pitch);
  const auto pitch = static_cast<std::size_t>(pitch64 < 0 ? -pitch64 : pitch64);
  if (pitch < minimumPitch || pitch > Bitmap::MAX_BYTES / source.rows) {
    throw std::invalid_argument("Invalid or excessive FreeType bitmap pitch");
  }

  auto &rgba = bitmap.rgba();
  for (unsigned int y = 0; y < source.rows; ++y) {
    const auto row = source.pitch < 0 ? source.rows - 1U - y : y;
    const auto *src = source.buffer + static_cast<std::size_t>(row) * pitch;
    for (unsigned int x = 0; x < source.width; ++x) {
      const auto index = (static_cast<std::size_t>(y) * source.width + x) * 4U;
      auto *dst = rgba.data() + index;
      dst[0] = dst[1] = dst[2] = 255;
      switch (source.pixel_mode) {
      case FT_PIXEL_MODE_GRAY: {
        const std::uint32_t maximum = source.num_grays - 1U;
        const auto sample = std::min<std::uint32_t>(src[x], maximum);
        dst[3] = static_cast<std::uint8_t>((sample * 255U + maximum / 2U) /
                                          maximum);
        break;
      }
      case FT_PIXEL_MODE_MONO:
        dst[3] = (src[x / 8U] & (0x80U >> (x & 7U))) != 0 ? 255 : 0;
        break;
      case FT_PIXEL_MODE_GRAY2:
        dst[3] = static_cast<std::uint8_t>(
            ((src[x / 4U] >> (6U - 2U * (x & 3U))) & 3U) * 85U);
        break;
      case FT_PIXEL_MODE_GRAY4:
        dst[3] = static_cast<std::uint8_t>(
            ((src[x / 2U] >> (4U - 4U * (x & 1U))) & 15U) * 17U);
        break;
      case FT_PIXEL_MODE_BGRA: {
        const auto *pixel = src + static_cast<std::size_t>(x) * 4U;
        const std::uint32_t alpha = pixel[3];
        dst[3] = static_cast<std::uint8_t>(alpha);
        for (std::size_t channel = 0; channel < 3; ++channel) {
          dst[channel] = alpha == 0
                             ? 0
                             : static_cast<std::uint8_t>(std::min(
                                   255U,
                                   (static_cast<std::uint32_t>(pixel[2U - channel]) *
                                        255U +
                                    alpha / 2U) /
                                       alpha));
        }
        break;
      }
      default:
        break;
      }
    }
  }
  return bitmap;
}

Font::Font(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) {
    throw std::runtime_error("Cannot open font: " + path.string());
  }
  const auto length = stream.tellg();
  if (length <= 0 || static_cast<std::uint64_t>(length) > MAX_FONT_BYTES ||
      static_cast<std::uint64_t>(length) >
          static_cast<std::uint64_t>(std::numeric_limits<FT_Long>::max())) {
    throw std::runtime_error("Font file is empty or exceeds the 64 MiB limit");
  }
  m_bytes.resize(static_cast<std::size_t>(length));
  stream.seekg(0);
  if (!stream.read(reinterpret_cast<char *>(m_bytes.data()),
                   static_cast<std::streamsize>(m_bytes.size()))) {
    throw std::runtime_error("Cannot read font: " + path.string());
  }
  try {
    if (FT_Init_FreeType(&m_library) != 0) {
      throw std::runtime_error("FT_Init_FreeType failed");
    }
    if (FT_New_Memory_Face(m_library, m_bytes.data(),
                           static_cast<FT_Long>(m_bytes.size()), 0, &m_face) != 0) {
      throw std::runtime_error("FreeType cannot load font: " + path.string());
    }
    if (FT_Select_Charmap(m_face, FT_ENCODING_UNICODE) != 0) {
      throw std::runtime_error("Font has no Unicode character map: " +
                               path.string());
    }
    m_family = sfntName(m_face, 1);
    m_style = sfntName(m_face, 2);
    m_version = sfntName(m_face, 5);
    if (m_family.empty()) {
      m_family = "Unnamed font";
    }
    if (m_style.empty()) {
      m_style = "Unknown style";
    }
  } catch (...) {
    release();
    throw;
  }
}

Font::~Font() { release(); }

void Font::release() noexcept {
  if (m_face != nullptr) {
    FT_Done_Face(m_face);
    m_face = nullptr;
  }
  if (m_library != nullptr) {
    FT_Done_FreeType(m_library);
    m_library = nullptr;
  }
}

void Font::setPixelSize(int pixelSize) {
  if (pixelSize < 1 || pixelSize > MAX_PIXEL_SIZE) {
    throw std::invalid_argument("Glyph pixel size must be between 1 and 512");
  }
  if (pixelSize == m_pixelSize) {
    return;
  }
  if (FT_Set_Pixel_Sizes(m_face, 0, static_cast<FT_UInt>(pixelSize)) != 0) {
    throw std::runtime_error("Font cannot render the requested pixel size; use a "
                             "scalable font or a supported bitmap strike size");
  }
  m_pixelSize = pixelSize;
}

bool Font::contains(std::uint32_t codepoint) const {
  return isScalar(codepoint) && FT_Get_Char_Index(m_face, codepoint) != 0;
}

const GlyphSurface &Font::glyph(std::uint32_t codepoint, int pixelSize) {
  setPixelSize(pixelSize);
  const std::uint64_t key =
      (static_cast<std::uint64_t>(pixelSize) << 32U) | codepoint;
  if (const auto cached = m_cache.find(key); cached != m_cache.end()) {
    return cached->second;
  }
  GlyphSurface surface;
  const auto glyphId =
      isScalar(codepoint) ? FT_Get_Char_Index(m_face, codepoint) : 0U;
  if (glyphId == 0) {
    surface.m_error = "Missing glyph";
  } else if (requiresColrV1Renderer(m_face, glyphId)) {
    surface.m_error = "COLRv1 glyphs require DokuText's paint-graph renderer";
  } else if (FT_Load_Glyph(m_face, glyphId, FT_LOAD_DEFAULT | FT_LOAD_COLOR) != 0) {
    surface.m_error = "FreeType cannot load glyph";
  } else if (m_face->glyph->format == FT_GLYPH_FORMAT_SVG) {
    surface.m_error = "SVG glyphs require an external renderer";
  } else if (FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
    surface.m_error = "FreeType cannot rasterize glyph";
  } else {
    const auto slot = m_face->glyph;
    try {
      surface.m_image = copyFreeTypeBitmap(slot->bitmap);
      surface.m_bearingX = slot->bitmap_left;
      surface.m_bearingY = slot->bitmap_top;
      surface.m_advance = pixels(slot->advance.x);
      surface.m_colored = slot->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA;
      surface.m_supported = true;
    } catch (const std::runtime_error &error) {
      surface.m_error = error.what();
    }
  }
  const auto bytes = surface.m_image.rgba().size();
  if (bytes > MAX_CACHE_BYTES - m_cachedBytes) {
    throw std::length_error("Glyph cache exceeds 64 MiB; reduce the preview size "
                             "or number of candidates");
  }
  auto [entry, inserted] = m_cache.emplace(key, std::move(surface));
  if (inserted) {
    m_cachedBytes += bytes;
  }
  return entry->second;
}

FontMetrics Font::metrics(int pixelSize) {
  setPixelSize(pixelSize);
  return {pixels(m_face->size->metrics.ascender),
           pixels(m_face->size->metrics.descender),
           pixels(m_face->size->metrics.height)};
}

int Font::textWidth(std::string_view text, int pixelSize) {
  if (text.size() > 8192U) {
    throw std::length_error("Preview label exceeds the supported length");
  }
  std::int64_t width = 0;
  for (const auto codepoint : decodeUtf8(text)) {
    const auto &surface = glyph(codepoint, pixelSize);
    width += surface.m_supported ? surface.m_advance
                                 : glyph('?', pixelSize).m_advance;
    if (width < -MAX_COORDINATE || width > MAX_COORDINATE) {
      throw std::length_error("Preview text width exceeds the supported limit");
    }
  }
  return static_cast<int>(width);
}

bool Font::drawText(Bitmap &target, std::string_view text, int x, int baseline,
                     int pixelSize, Color color, const Rect &clip) {
  if (text.size() > 8192U) {
    throw std::length_error("Preview label exceeds the supported length");
  }
  bool clipped = false;
  std::int64_t pen = x;
  for (const auto codepoint : decodeUtf8(text)) {
    const auto &requested = glyph(codepoint, pixelSize);
    const auto &surface = requested.m_supported ? requested : glyph('?', pixelSize);
    const auto left = pen + surface.m_bearingX;
    const auto top = static_cast<std::int64_t>(baseline) - surface.m_bearingY;
    if (left < -MAX_COORDINATE || left > MAX_COORDINATE ||
        top < -MAX_COORDINATE || top > MAX_COORDINATE) {
      throw std::length_error("Preview text position exceeds the supported limit");
    }
    clipped |= target.blitClipped(surface.m_image, static_cast<int>(left),
                                  static_cast<int>(top), clip,
                                  surface.m_colored ? Color{255, 255, 255, 255}
                                                     : color);
    pen += surface.m_advance;
  }
  return clipped;
}

} // namespace findicon::raster
