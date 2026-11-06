#include "Font.hpp"
#include "Preview.hpp"
#include "DokuText/Bitmap.hpp"
#include "DokuText/PngWriter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <png.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using findicon::raster::Bitmap;
using findicon::raster::copyFreeTypeBitmap;

[[nodiscard]] Bitmap readPng(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  REQUIRE(stream.good());
  const std::vector<unsigned char> bytes(std::istreambuf_iterator<char>{stream},
                                         std::istreambuf_iterator<char>{});
  png_image image{};
  image.version = PNG_IMAGE_VERSION;
  const bool headerOk = png_image_begin_read_from_memory(
                            &image, bytes.data(), bytes.size()) != 0;
  if (!headerOk) {
    const std::string error = image.message;
    png_image_free(&image);
    FAIL("Cannot read PNG header: " << error);
  }
  image.format = PNG_FORMAT_RGBA;
  Bitmap decoded;
  decoded.resize(static_cast<int>(image.width), static_cast<int>(image.height));
  const bool pixelsOk = png_image_finish_read(
                            &image, nullptr, decoded.rgba().data(), 0, nullptr) != 0;
  const std::string error = image.message;
  png_image_free(&image);
  INFO(error);
  REQUIRE(pixelsOk);
  return decoded;
}

[[nodiscard]] std::filesystem::path outputPath(const std::filesystem::path &name) {
  const auto directory = std::filesystem::path(FINDICON_TEST_OUTPUT_DIR);
  std::filesystem::create_directories(directory);
  return directory / name;
}

[[nodiscard]] findicon::PreviewOptions previewOptions(const char *name) {
  findicon::PreviewOptions options;
  const auto assets = std::filesystem::path(FINDICON_TEST_ASSET_DIR);
  options.m_fontPath = assets / "SymbolsNerdFont-Regular.ttf";
  options.m_labelFontPath = assets / "LabelFont.ttf";
  options.m_outputPath = outputPath(name);
  return options;
}

} // namespace

TEST_CASE("Straight alpha survives a transparent intermediate", "[render][alpha]") {
  Bitmap glyph;
  glyph.resize(1, 1);
  glyph.clear(255, 255, 255, 128);
  Bitmap intermediate;
  intermediate.resize(1, 1);
  intermediate.blit(glyph, 0, 0);
  REQUIRE(intermediate.rgba() == std::vector<std::uint8_t>{255, 255, 255, 128});

  Bitmap canvas;
  canvas.resize(1, 1);
  canvas.clear(0, 0, 0, 255);
  canvas.blit(intermediate, 0, 0);
  REQUIRE(canvas.rgba() == std::vector<std::uint8_t>{128, 128, 128, 255});
}

TEST_CASE("Source over accounts for destination alpha", "[render][alpha]") {
  Bitmap source;
  source.resize(1, 1);
  source.clear(255, 0, 0, 128);
  Bitmap destination;
  destination.resize(1, 1);
  destination.clear(0, 0, 255, 128);
  destination.blit(source, 0, 0);
  REQUIRE(destination.rgba() == std::vector<std::uint8_t>{170, 0, 85, 192});
}

TEST_CASE("Blitting clips without leaking into adjacent cells", "[render][bounds]") {
  Bitmap source;
  source.resize(3, 2);
  source.clear(255, 255, 255, 255);
  Bitmap destination;
  destination.resize(4, 3);
  destination.clear(0, 0, 0, 255);
  REQUIRE(destination.blitClipped(source, 0, 0, {1, 1, 2, 1}));
  for (int y = 0; y < 3; ++y) {
    for (int x = 0; x < 4; ++x) {
      const auto offset = static_cast<std::size_t>((y * 4 + x) * 4);
      REQUIRE(destination.rgba()[offset] == ((y == 1 && x >= 1 && x <= 2) ? 255 : 0));
    }
  }
  REQUIRE(destination.blitClipped(source, std::numeric_limits<int>::max(), 0,
                                   {0, 0, 4, 3}));
  REQUIRE(destination.blitClipped(source, std::numeric_limits<int>::min(), 0,
                                   {0, 0, 4, 3}));
}

TEST_CASE("Negative FreeType pitch preserves top to bottom order",
           "[render][freetype]") {
  std::array<unsigned char, 6> pixels{10, 20, 0, 30, 40, 0};
  FT_Bitmap bitmap{};
  bitmap.width = 2;
  bitmap.rows = 2;
  bitmap.pitch = -3;
  bitmap.buffer = pixels.data();
  bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
  bitmap.num_grays = 256;
  const auto result = copyFreeTypeBitmap(bitmap);
  REQUIRE(result.rgba()[3] == 30);
  REQUIRE(result.rgba()[7] == 40);
  REQUIRE(result.rgba()[11] == 10);
  REQUIRE(result.rgba()[15] == 20);
}

TEST_CASE("MONO coverage is unpacked MSB first across bytes", "[render][freetype]") {
  std::array<unsigned char, 2> pixels{0x80, 0x80};
  FT_Bitmap bitmap{};
  bitmap.width = 9;
  bitmap.rows = 1;
  bitmap.pitch = 2;
  bitmap.buffer = pixels.data();
  bitmap.pixel_mode = FT_PIXEL_MODE_MONO;
  const auto result = copyFreeTypeBitmap(bitmap);
  for (std::size_t x = 0; x < 9; ++x) {
    REQUIRE(result.rgba()[x * 4 + 3] == ((x == 0 || x == 8) ? 255 : 0));
  }
}

TEST_CASE("FreeType BGRA becomes straight RGBA", "[render][alpha][freetype]") {
  std::array<unsigned char, 4> pixels{0, 0, 128, 128};
  FT_Bitmap bitmap{};
  bitmap.width = bitmap.rows = 1;
  bitmap.pitch = 4;
  bitmap.buffer = pixels.data();
  bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
  const auto result = copyFreeTypeBitmap(bitmap);
  REQUIRE(result.rgba() == std::vector<std::uint8_t>{255, 0, 0, 128});
  Bitmap canvas;
  canvas.resize(1, 1);
  canvas.clear(0, 0, 0, 255);
  canvas.blit(result, 0, 0);
  REQUIRE(canvas.rgba() == std::vector<std::uint8_t>{128, 0, 0, 255});
}

TEST_CASE("Bitmap allocations and pitch are bounded", "[render][bounds]") {
  Bitmap bitmap;
  REQUIRE_THROWS_AS(bitmap.resize(-1, 1), std::invalid_argument);
  REQUIRE_THROWS_AS(bitmap.resize(std::numeric_limits<int>::max(), 1),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(bitmap.resize(16384, 16384), std::length_error);
  unsigned char pixel = 0;
  FT_Bitmap malformed{};
  malformed.width = malformed.rows = 1;
  malformed.pitch = std::numeric_limits<int>::min();
  malformed.buffer = &pixel;
  malformed.pixel_mode = FT_PIXEL_MODE_GRAY;
  malformed.num_grays = 256;
  REQUIRE_THROWS_AS(copyFreeTypeBitmap(malformed), std::invalid_argument);
}

TEST_CASE("PNG output preserves RGBA and Unicode filesystem paths", "[render][png]") {
  const auto directory = outputPath(std::filesystem::path(u8"画像"));
  std::filesystem::create_directories(directory);
  const auto path = directory / "Alpha.png";
  Bitmap bitmap;
  bitmap.resize(2, 1);
  bitmap.rgba() = {255, 0, 0, 128, 10, 20, 30, 255};
  REQUIRE(findicon::raster::writePng(path, bitmap));
  const auto decoded = readPng(path);
  REQUIRE(decoded.width() == bitmap.width());
  REQUIRE(decoded.height() == bitmap.height());
  REQUIRE(decoded.rgba() == bitmap.rgba());
}

TEST_CASE("Real Nerd Font previews distinguish supplementary and missing glyphs",
           "[render][preview]") {
  std::vector<findicon::Candidate> candidates{
      {{"nf-cod-settings_gear", "cod", 0xEB51U}, {}, 0},
      {{"nf-md-cog", "md", 0xF0493U}, {}, 0},
      {{"missing-test", "test", 0x10FFFFU}, {}, 0},
  };
  const auto options = previewOptions("RenderingTest.png");
  const auto result = findicon::renderPreview(candidates, options);
  REQUIRE(result.m_cells.size() == 3);
  REQUIRE(result.m_cells[0].m_supported);
  REQUIRE(result.m_cells[1].m_supported);
  REQUIRE_FALSE(result.m_cells[2].m_supported);
  REQUIRE_FALSE(result.m_cells[0].m_clipped);
  REQUIRE_FALSE(result.m_cells[1].m_clipped);
  REQUIRE(result.m_cells[0].m_advance > 0);
  REQUIRE_FALSE(result.m_fontFamily.empty());
  REQUIRE_FALSE(result.m_fontVersion.empty());
  const auto png = readPng(options.m_outputPath);
  REQUIRE(png.width() == result.m_width);
  REQUIRE(png.height() == result.m_height);
  for (const auto &cell : result.m_cells) {
    REQUIRE(cell.m_x >= 0);
    REQUIRE(cell.m_y >= 0);
    REQUIRE(cell.m_x + cell.m_width <= result.m_width);
    REQUIRE(cell.m_y + cell.m_height <= result.m_height);
  }
  for (std::size_t index = 3; index < png.rgba().size(); index += 4) {
    if (png.rgba()[index] != 255) {
      FAIL("Contact sheet must have a fully opaque background");
    }
  }
}

TEST_CASE("Preview rejects unusable label fonts and excessive geometry",
           "[render][preview][bounds]") {
  auto options = previewOptions("Rejected.png");
  options.m_columns = 0;
  REQUIRE_THROWS_AS(findicon::renderPreview({}, options), std::invalid_argument);
  options.m_columns = 4;
  options.m_largePixelSize = 513;
  REQUIRE_THROWS_AS(findicon::renderPreview({}, options), std::invalid_argument);
  options.m_largePixelSize = 64;
  const std::vector<findicon::Candidate> tooMany(257);
  REQUIRE_THROWS_AS(findicon::renderPreview(tooMany, options), std::length_error);
  options.m_labelFontPath = options.m_fontPath;
  REQUIRE_THROWS_AS(findicon::renderPreview({}, options), std::runtime_error);
}
