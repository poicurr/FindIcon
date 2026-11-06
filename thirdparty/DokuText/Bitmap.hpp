// Adapted from poicurr/DokuText; see README.md and LICENSE in this directory.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace findicon::raster {

using Color = std::array<std::uint8_t, 4>;

struct Rect {
  int m_x = 0;
  int m_y = 0;
  int m_width = 0;
  int m_height = 0;
};

class Bitmap {
public:
  static constexpr int BYTES_PER_PIXEL = 4;
  static constexpr int MAX_DIMENSION = 16384;
  static constexpr std::size_t MAX_BYTES = 128U * 1024U * 1024U;

  void resize(int width, int height);
  void clear(std::uint8_t red, std::uint8_t green, std::uint8_t blue,
             std::uint8_t alpha = 255);
  void fillRect(const Rect &rect, Color color);
  void strokeRect(const Rect &rect, Color color);
  void blit(const Bitmap &source, int offsetX, int offsetY);

  // All pixels use straight alpha. Returns true if nonempty source bounds
  // extend beyond the intersection of clip and the destination bitmap.
  [[nodiscard]] bool
  blitClipped(const Bitmap &source, int offsetX, int offsetY, const Rect &clip,
              Color tint = {255, 255, 255, 255});

  [[nodiscard]] int width() const { return m_width; }
  [[nodiscard]] int height() const { return m_height; }
  [[nodiscard]] const std::vector<std::uint8_t> &rgba() const { return m_rgba; }
  [[nodiscard]] std::vector<std::uint8_t> &rgba() { return m_rgba; }

private:
  void validateStorage() const;

  int m_width = 0;
  int m_height = 0;
  std::vector<std::uint8_t> m_rgba;
};

} // namespace findicon::raster
