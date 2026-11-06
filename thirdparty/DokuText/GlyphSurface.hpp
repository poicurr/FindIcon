// Adapted from poicurr/DokuText; see README.md and LICENSE in this directory.
#pragma once

#include "Bitmap.hpp"

#include <string>

namespace findicon::raster {

struct GlyphSurface {
  Bitmap m_image;
  int m_bearingX = 0;
  int m_bearingY = 0;
  int m_advance = 0;
  bool m_supported = false;
  bool m_colored = false;
  std::string m_error;
};

} // namespace findicon::raster
