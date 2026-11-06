// Adapted from poicurr/DokuText; see README.md and LICENSE in this directory.
#include "PngWriter.hpp"

#include "Bitmap.hpp"

#include <png.h>

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>

namespace findicon::raster {
namespace {

struct FileCloser {
  void operator()(std::FILE *file) const {
    if (file != nullptr) {
      std::fclose(file);
    }
  }
};

using FileHandle = std::unique_ptr<std::FILE, FileCloser>;

[[nodiscard]] FileHandle openOutput(const std::filesystem::path &path) {
#ifdef _WIN32
  FileHandle file(_wfopen(path.c_str(), L"wb"));
#else
  FileHandle file(std::fopen(path.c_str(), "wb"));
#endif
  if (!file) {
    throw std::runtime_error("Cannot open PNG output: " + path.string());
  }
  return file;
}

} // namespace

bool writePng(const std::filesystem::path &path, const Bitmap &bitmap) {
  if (bitmap.width() <= 0 || bitmap.height() <= 0) {
    return false;
  }
  const auto rowBytes = static_cast<std::size_t>(bitmap.width()) *
                        Bitmap::BYTES_PER_PIXEL;
  if (bitmap.rgba().size() != rowBytes * bitmap.height()) {
    throw std::invalid_argument("PNG dimensions and RGBA storage do not match");
  }

  // Construct every nontrivial automatic object before setjmp. libpng errors
  // must not longjmp across a C++ object whose destructor would be skipped.
  FileHandle file = openOutput(path);
  std::vector<png_bytep> rows(static_cast<std::size_t>(bitmap.height()));
  for (std::size_t row = 0; row < rows.size(); ++row) {
    rows[row] = const_cast<png_bytep>(bitmap.rgba().data() + row * rowBytes);
  }

  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (png == nullptr) {
    throw std::runtime_error("png_create_write_struct failed");
  }
  png_infop info = png_create_info_struct(png);
  if (info == nullptr) {
    png_destroy_write_struct(&png, nullptr);
    throw std::runtime_error("png_create_info_struct failed");
  }
  if (setjmp(png_jmpbuf(png)) != 0) {
    png_destroy_write_struct(&png, &info);
    return false;
  }
  png_init_io(png, file.get());
  png_set_IHDR(png, info, static_cast<png_uint_32>(bitmap.width()),
               static_cast<png_uint_32>(bitmap.height()), 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_set_rows(png, info, rows.data());
  png_write_png(png, info, PNG_TRANSFORM_IDENTITY, nullptr);
  png_destroy_write_struct(&png, &info);
  return std::fflush(file.get()) == 0 && std::ferror(file.get()) == 0;
}

} // namespace findicon::raster
