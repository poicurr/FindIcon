#include "Output.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

namespace fs = std::filesystem;

class OutputDirectory {
public:
  OutputDirectory() {
    static std::atomic<unsigned int> s_sequence{0};
    const auto timestamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    m_path = fs::temp_directory_path() /
             ("FindIconOutput-" + std::to_string(timestamp) + "-" +
              std::to_string(s_sequence.fetch_add(1)));
    if (!fs::create_directory(m_path)) {
      throw std::runtime_error("Cannot create output test directory");
    }
  }

  ~OutputDirectory() {
    std::error_code error;
    fs::remove_all(m_path, error);
  }

  [[nodiscard]] const fs::path &path() const { return m_path; }

private:
  fs::path m_path;
};

void writeFile(const fs::path &path, const std::string &content) {
  std::ofstream file(path, std::ios::binary);
  file << content;
  file.close();
  if (!file) {
    throw std::runtime_error("Cannot write output test file");
  }
}

[[nodiscard]] std::string readFile(const fs::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("Preview output replaces PNG and manifest together", "[output]") {
  const OutputDirectory directory;
  const auto png = directory.path() / "Candidates.png";
  const auto manifest = directory.path() / "Candidates.json";
  writeFile(png, "old PNG");
  writeFile(manifest, "old manifest");
  fs::path pngTemporary;
  fs::path manifestTemporary;
  {
    findicon::PreviewOutput output(png, manifest);
    pngTemporary = output.pngTemporary();
    manifestTemporary = output.manifestTemporary();
    writeFile(pngTemporary, "new PNG");
    writeFile(manifestTemporary, "new manifest");
    REQUIRE(readFile(png) == "old PNG");
    REQUIRE(readFile(manifest) == "old manifest");
    output.commit();
    REQUIRE(readFile(png) == "new PNG");
    REQUIRE(readFile(manifest) == "new manifest");
    REQUIRE_THROWS_AS(output.commit(), std::logic_error);
  }
  REQUIRE_FALSE(fs::exists(pngTemporary));
  REQUIRE_FALSE(fs::exists(manifestTemporary));
  REQUIRE_FALSE(fs::exists(pngTemporary.parent_path()));
  REQUIRE_FALSE(fs::exists(manifestTemporary.parent_path()));
}

TEST_CASE("Incomplete manifest generation leaves both existing files unchanged",
          "[output]") {
  const OutputDirectory directory;
  const auto png = directory.path() / "Candidates.png";
  const auto manifest = directory.path() / "Candidates.json";
  writeFile(png, "old PNG");
  writeFile(manifest, "old manifest");
  findicon::PreviewOutput output(png, manifest);
  writeFile(output.pngTemporary(), "new PNG");
  REQUIRE_THROWS(output.commit());
  REQUIRE(readFile(png) == "old PNG");
  REQUIRE(readFile(manifest) == "old manifest");
}

TEST_CASE("A manifest rename failure rolls back an already installed PNG",
          "[output]") {
  const OutputDirectory directory;
  const auto png = directory.path() / "Candidates.png";
  const auto manifest = directory.path() / "Candidates.json";
  writeFile(png, "old PNG");
  writeFile(manifest, "old manifest");
  findicon::PreviewOutput output(png, manifest);
  writeFile(output.pngTemporary(), "new PNG");
  writeFile(output.manifestTemporary(), "new manifest");

  // Block the manifest backup destination to trigger a real rename error.
  // This works even when tests run as root and permission-based tests cannot
  // reliably force an I/O failure. No production failure-injection hook is used.
  const auto blocker = output.manifestTemporary().parent_path() / "Previous";
  fs::create_directory(blocker);
  writeFile(blocker / "Keep.txt", "blocker");
  REQUIRE_THROWS_AS(output.commit(), fs::filesystem_error);
  REQUIRE(readFile(png) == "old PNG");
  REQUIRE(readFile(manifest) == "old manifest");
  REQUIRE(readFile(blocker / "Keep.txt") == "blocker");
}

TEST_CASE("Preview output creates a new pair in different directories", "[output]") {
  const OutputDirectory directory;
  const auto png = directory.path() / "Images" / "Candidates.png";
  const auto manifest = directory.path() / "Metadata" / "Candidates.json";
  findicon::PreviewOutput output(png, manifest);
  writeFile(output.pngTemporary(), "new PNG");
  writeFile(output.manifestTemporary(), "new manifest");
  output.commit();
  REQUIRE(readFile(png) == "new PNG");
  REQUIRE(readFile(manifest) == "new manifest");
}

TEST_CASE("Preview output rejects identical destination paths", "[output]") {
  const OutputDirectory directory;
  const auto destination = directory.path() / "Candidates.png";
  REQUIRE_THROWS_AS(findicon::PreviewOutput(destination, destination),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      findicon::PreviewOutput(destination,
                              directory.path() / "." / "Candidates.png"),
      std::invalid_argument);
  REQUIRE_THROWS_AS(findicon::PreviewOutput({}, destination),
                    std::invalid_argument);
}

TEST_CASE("Abandoned preview output removes staged files without changing output",
          "[output]") {
  const OutputDirectory directory;
  const auto png = directory.path() / "Candidates.png";
  const auto manifest = directory.path() / "Candidates.json";
  writeFile(png, "old PNG");
  writeFile(manifest, "old manifest");
  fs::path pngTemporary;
  fs::path manifestTemporary;
  {
    findicon::PreviewOutput output(png, manifest);
    pngTemporary = output.pngTemporary();
    manifestTemporary = output.manifestTemporary();
    writeFile(pngTemporary, "partial PNG");
    writeFile(manifestTemporary, "partial JSON");
  }
  REQUIRE(readFile(png) == "old PNG");
  REQUIRE(readFile(manifest) == "old manifest");
  REQUIRE_FALSE(fs::exists(pngTemporary.parent_path()));
  REQUIRE_FALSE(fs::exists(manifestTemporary.parent_path()));
}

TEST_CASE("New case-alias destinations cannot overwrite the PNG with JSON",
          "[output]") {
  const OutputDirectory directory;
  const auto png = directory.path() / "Candidates.png";
  const auto manifest = directory.path() / "candidates.PNG";
  const auto probe = directory.path() / "CaseProbe";
  writeFile(probe, "probe");
  const bool caseInsensitive = fs::exists(directory.path() / "caseprobe");
  fs::remove(probe);
  if (!caseInsensitive) {
    SKIP("This filesystem distinguishes path case");
  }

  findicon::PreviewOutput output(png, manifest);
  writeFile(output.pngTemporary(), "PNG bytes");
  writeFile(output.manifestTemporary(), "JSON bytes");
  REQUIRE_THROWS(output.commit());
  REQUIRE_FALSE(fs::exists(png));
  REQUIRE_FALSE(fs::exists(manifest));
}
