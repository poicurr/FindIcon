// Adapted from poicurr/DokuText; see README.md and LICENSE in this directory.
#pragma once

#include <filesystem>

namespace findicon::raster {

class Bitmap;

[[nodiscard]] bool writePng(const std::filesystem::path &path,
                            const Bitmap &bitmap);

} // namespace findicon::raster
