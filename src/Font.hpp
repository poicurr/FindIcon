#pragma once

#include "DokuText/GlyphSurface.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace findicon::raster {

struct FontMetrics {
  int m_ascender = 0;
  int m_descender = 0;
  int m_height = 0;
};

// DokuText's FT_Bitmap -> RGBA conversion, with explicit pixel mode, signed
// pitch, and premultiplied BGRA handling. The returned pixels use straight
// alpha, matching Bitmap and PNG.
[[nodiscard]] Bitmap copyFreeTypeBitmap(const FT_Bitmap &bitmap);

class Font {
public:
  static constexpr int MAX_PIXEL_SIZE = 512;
  static constexpr std::size_t MAX_FONT_BYTES = 64U * 1024U * 1024U;
  static constexpr std::size_t MAX_CACHE_BYTES = 64U * 1024U * 1024U;

  explicit Font(const std::filesystem::path &path);
  ~Font();
  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;
  Font(Font &&) = delete;
  Font &operator=(Font &&) = delete;

  [[nodiscard]] const GlyphSurface &glyph(std::uint32_t codepoint,
                                          int pixelSize);
  [[nodiscard]] FontMetrics metrics(int pixelSize);
  [[nodiscard]] bool contains(std::uint32_t codepoint) const;
  [[nodiscard]] int textWidth(std::string_view text, int pixelSize);
  [[nodiscard]] bool drawText(Bitmap &target, std::string_view text, int x,
                              int baseline, int pixelSize, Color color,
                              const Rect &clip);

  [[nodiscard]] const std::string &family() const { return m_family; }
  [[nodiscard]] const std::string &style() const { return m_style; }
  [[nodiscard]] const std::string &version() const { return m_version; }

private:
  void setPixelSize(int pixelSize);
  void release() noexcept;

  std::vector<unsigned char> m_bytes;
  FT_Library m_library = nullptr;
  FT_Face m_face = nullptr;
  int m_pixelSize = 0;
  std::size_t m_cachedBytes = 0;
  std::unordered_map<std::uint64_t, GlyphSurface> m_cache;
  std::string m_family;
  std::string m_style;
  std::string m_version;
};

} // namespace findicon::raster
