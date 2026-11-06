// Adapted from poicurr/DokuText; see README.md and LICENSE in this directory.
#include "Bitmap.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace findicon::raster {
namespace {

[[nodiscard]] std::size_t bufferSize(int width, int height) {
  if (width < 0 || height < 0 || width > Bitmap::MAX_DIMENSION ||
      height > Bitmap::MAX_DIMENSION) {
    throw std::invalid_argument("Bitmap dimensions must be between 0 and 16384");
  }
  const auto bytes = static_cast<std::size_t>(width) *
                     static_cast<std::size_t>(height) * Bitmap::BYTES_PER_PIXEL;
  if (bytes > Bitmap::MAX_BYTES) {
    throw std::length_error("Bitmap exceeds the 128 MiB allocation limit");
  }
  return bytes;
}

struct Bounds {
  std::int64_t m_left;
  std::int64_t m_top;
  std::int64_t m_right;
  std::int64_t m_bottom;
};

[[nodiscard]] Bounds clipBounds(const Rect &rect, int width, int height) {
  const auto left = std::clamp<std::int64_t>(rect.m_x, 0, width);
  const auto top = std::clamp<std::int64_t>(rect.m_y, 0, height);
  const auto right = std::clamp<std::int64_t>(
      static_cast<std::int64_t>(rect.m_x) + std::max(rect.m_width, 0), 0, width);
  const auto bottom = std::clamp<std::int64_t>(
      static_cast<std::int64_t>(rect.m_y) + std::max(rect.m_height, 0), 0, height);
  return {left, top, std::max(left, right), std::max(top, bottom)};
}

} // namespace

void Bitmap::resize(int width, int height) {
  const auto size = bufferSize(width, height);
  std::vector<std::uint8_t> replacement(size, 0);
  m_width = width;
  m_height = height;
  m_rgba = std::move(replacement);
}

void Bitmap::validateStorage() const {
  if (m_rgba.size() != bufferSize(m_width, m_height)) {
    throw std::logic_error("Bitmap dimensions and RGBA storage do not match");
  }
}

void Bitmap::clear(std::uint8_t red, std::uint8_t green, std::uint8_t blue,
                   std::uint8_t alpha) {
  validateStorage();
  for (std::size_t index = 0; index < m_rgba.size(); index += BYTES_PER_PIXEL) {
    m_rgba[index] = red;
    m_rgba[index + 1] = green;
    m_rgba[index + 2] = blue;
    m_rgba[index + 3] = alpha;
  }
}

void Bitmap::fillRect(const Rect &rect, Color color) {
  validateStorage();
  const auto bounds = clipBounds(rect, m_width, m_height);
  for (auto y = bounds.m_top; y < bounds.m_bottom; ++y) {
    for (auto x = bounds.m_left; x < bounds.m_right; ++x) {
      const auto index = (static_cast<std::size_t>(y) * m_width +
                          static_cast<std::size_t>(x)) * BYTES_PER_PIXEL;
      std::copy(color.begin(), color.end(), m_rgba.begin() +
                                                static_cast<std::ptrdiff_t>(index));
    }
  }
}

void Bitmap::strokeRect(const Rect &rect, Color color) {
  if (rect.m_width <= 0 || rect.m_height <= 0) {
    return;
  }
  const auto right = static_cast<std::int64_t>(rect.m_x) + rect.m_width - 1;
  const auto bottom = static_cast<std::int64_t>(rect.m_y) + rect.m_height - 1;
  fillRect({rect.m_x, rect.m_y, rect.m_width, 1}, color);
  fillRect({rect.m_x, rect.m_y, 1, rect.m_height}, color);
  if (bottom >= 0 && bottom < m_height) {
    fillRect({rect.m_x, static_cast<int>(bottom), rect.m_width, 1}, color);
  }
  if (right >= 0 && right < m_width) {
    fillRect({static_cast<int>(right), rect.m_y, 1, rect.m_height}, color);
  }
}

void Bitmap::blit(const Bitmap &source, int offsetX, int offsetY) {
  (void)blitClipped(source, offsetX, offsetY,
                   {0, 0, m_width, m_height});
}

bool Bitmap::blitClipped(const Bitmap &source, int offsetX, int offsetY,
                         const Rect &clip, Color tint) {
  validateStorage();
  source.validateStorage();
  if (source.m_width == 0 || source.m_height == 0) {
    return false;
  }
  if (this == &source) {
    const Bitmap copy = source;
    return blitClipped(copy, offsetX, offsetY, clip, tint);
  }
  const auto bounds = clipBounds(clip, m_width, m_height);
  const auto sourceRight = static_cast<std::int64_t>(offsetX) + source.m_width;
  const auto sourceBottom = static_cast<std::int64_t>(offsetY) + source.m_height;
  const bool clipped = offsetX < bounds.m_left || offsetY < bounds.m_top ||
                       sourceRight > bounds.m_right ||
                       sourceBottom > bounds.m_bottom;
  const auto left = std::max<std::int64_t>(offsetX, bounds.m_left);
  const auto top = std::max<std::int64_t>(offsetY, bounds.m_top);
  const auto right = std::min(sourceRight, bounds.m_right);
  const auto bottom = std::min(sourceBottom, bounds.m_bottom);

  for (auto y = top; y < bottom; ++y) {
    for (auto x = left; x < right; ++x) {
      const auto dstIndex = (static_cast<std::size_t>(y) * m_width +
                             static_cast<std::size_t>(x)) * BYTES_PER_PIXEL;
      const auto srcIndex = (static_cast<std::size_t>(y - offsetY) *
                                 source.m_width +
                             static_cast<std::size_t>(x - offsetX)) *
                            BYTES_PER_PIXEL;
      auto *dst = m_rgba.data() + dstIndex;
      const auto *src = source.m_rgba.data() + srcIndex;
      const std::uint32_t sourceAlpha =
          (static_cast<std::uint32_t>(src[3]) * tint[3] + 127U) / 255U;
      if (sourceAlpha == 0) {
        continue;
      }
      const std::uint32_t inverseAlpha = 255U - sourceAlpha;
      const std::uint32_t destinationAlpha = dst[3];
      const std::uint32_t alphaNumerator =
          sourceAlpha * 255U + destinationAlpha * inverseAlpha;
      for (std::size_t channel = 0; channel < 3; ++channel) {
        const std::uint32_t sourceColor =
            (static_cast<std::uint32_t>(src[channel]) * tint[channel] + 127U) /
            255U;
        const std::uint32_t colorNumerator =
            sourceColor * sourceAlpha * 255U +
            static_cast<std::uint32_t>(dst[channel]) * destinationAlpha *
                inverseAlpha;
        dst[channel] = static_cast<std::uint8_t>(
            (colorNumerator + alphaNumerator / 2U) / alphaNumerator);
      }
      dst[3] = static_cast<std::uint8_t>((alphaNumerator + 127U) / 255U);
    }
  }
  return clipped;
}

} // namespace findicon::raster
