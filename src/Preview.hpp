#pragma once

#include "Catalog.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace findicon {

struct PreviewOptions {
  std::filesystem::path m_fontPath;
  std::filesystem::path m_labelFontPath;
  std::filesystem::path m_outputPath;
  int m_pixelSize = 20;
  int m_largePixelSize = 64;
  int m_columns = 4;
  bool m_light = false;
};

struct PreviewCell {
  std::string m_id;
  bool m_supported = false;
  bool m_clipped = false;
  int m_x = 0;
  int m_y = 0;
  int m_width = 0;
  int m_height = 0;
  int m_advance = 0;
  int m_bearingX = 0;
  int m_bearingY = 0;
};

struct PreviewResult {
  int m_width = 0;
  int m_height = 0;
  std::string m_fontFamily;
  std::string m_fontStyle;
  std::string m_fontVersion;
  std::vector<PreviewCell> m_cells;
};

[[nodiscard]] PreviewResult
renderPreview(const std::vector<Candidate> &candidates,
              const PreviewOptions &options);

} // namespace findicon
