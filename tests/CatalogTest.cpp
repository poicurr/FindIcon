#include "Catalog.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view FIXTURE = R"json({
  "METADATA": {"version": "test-1"},
  "fa-cog": {"char": "\uf013", "code": "f013"},
  "fa-gear": {"char": "\uf013", "code": "f013"},
  "cod-settings_gear": {"char": "\ueb51", "code": "eb51"},
  "md-cog": {"char": "\udb81\udc93", "code": "f0493"},
  "md-cog_outline": {"char": "\udb82\udcbb", "code": "f08bb"},
  "md-eye": {"char": "\udb80\ude08", "code": "f0208"},
  "md-eye_off": {"char": "\udb80\ude09", "code": "f0209"},
  "custom-bazel": {"char": "\ue63a", "code": "e63a"},
  "seti-bazel": {"char": "\ue63a", "code": "e63a"},
  "md-rotate_3d": {"char": "\udb83\udec7", "code": "f0ec7"},
  "md-rotate_3d_variant": {"char": "\udb81\udc64", "code": "f0464"}
})json";

class TemporaryCatalog {
public:
  explicit TemporaryCatalog(std::string_view content = FIXTURE) {
    static std::atomic<unsigned int> s_sequence{0};
    const auto timestamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    m_path = std::filesystem::temp_directory_path() /
             ("FindIconCatalog-" + std::to_string(timestamp) + "-" +
              std::to_string(s_sequence.fetch_add(1)) + ".json");
    std::ofstream file(m_path, std::ios::binary);
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file) {
      throw std::runtime_error("Unable to write test catalog");
    }
  }

  ~TemporaryCatalog() {
    std::error_code error;
    std::filesystem::remove(m_path, error);
  }

  TemporaryCatalog(const TemporaryCatalog &) = delete;
  TemporaryCatalog &operator=(const TemporaryCatalog &) = delete;

  [[nodiscard]] const std::filesystem::path &path() const { return m_path; }

private:
  std::filesystem::path m_path;
};

[[nodiscard]] findicon::Catalog fixtureCatalog() {
  const TemporaryCatalog temporary;
  return findicon::Catalog::load(temporary.path());
}

[[nodiscard]] std::vector<std::string>
ids(const findicon::SearchResult &result) {
  std::vector<std::string> values;
  for (const auto &candidate : result.m_candidates) {
    values.push_back(candidate.m_icon.m_id);
  }
  return values;
}

} // namespace

TEST_CASE("Catalog preserves official identifiers and supplementary codepoints",
          "[catalog]") {
  const auto catalog = fixtureCatalog();
  REQUIRE(catalog.version() == "test-1");
  REQUIRE(catalog.icons().size() == 11);
  REQUIRE(catalog.resolve("md-cog").m_icon.m_id == "nf-md-cog");
  REQUIRE(catalog.resolve("NF-MD-COG").m_icon.m_family == "md");
  REQUIRE(catalog.resolve("nf-md-cog").m_icon.m_codepoint == 0xF0493);

  for (const std::string value : {"U+F0493", "0xf0493", "f0493",
                                  "\xF3\xB0\x92\x93"}) {
    INFO(value);
    REQUIRE(catalog.resolve(value).m_icon.m_id == "nf-md-cog");
  }
  REQUIRE_THROWS_AS(catalog.resolve("nf-md-not_present"), std::invalid_argument);
  REQUIRE_THROWS_AS(catalog.resolve("U+10FFFF"), std::invalid_argument);
  REQUIRE_THROWS_AS(catalog.resolve("U+D800"), std::invalid_argument);
  REQUIRE_THROWS_AS(catalog.resolve("U+110000"), std::invalid_argument);
  REQUIRE_THROWS_AS(catalog.resolve("\xC0\x80"), std::invalid_argument);
}

TEST_CASE("Search matches aliases without duplicate previews", "[catalog]") {
  const auto catalog = fixtureCatalog();
  findicon::SearchOptions options;
  options.m_queries = {"gear"};
  const auto result = catalog.search(options);
  REQUIRE(result.m_total == 2);
  REQUIRE(result.m_candidates[0].m_icon.m_id == "nf-fa-gear");
  REQUIRE(result.m_candidates[0].m_aliases ==
          std::vector<std::string>{"nf-fa-cog"});
  REQUIRE(result.m_candidates[1].m_icon.m_id == "nf-cod-settings_gear");

  options.m_queries = {"nf-fa-gear"};
  const auto exact = catalog.search(options);
  REQUIRE(exact.m_total == 1);
  REQUIRE(exact.m_candidates[0].m_icon.m_id == "nf-fa-gear");

  options.m_queries = {"md-cog"};
  REQUIRE(ids(catalog.search(options)) ==
          std::vector<std::string>{"nf-md-cog", "nf-md-cog_outline"});
}

TEST_CASE("Search uses OR terms and ranks multiple word matches first",
          "[catalog]") {
  const auto catalog = fixtureCatalog();
  findicon::SearchOptions options;
  options.m_queries = {"settings gear"};
  const auto result = catalog.search(options);
  REQUIRE(result.m_total == 2);
  REQUIRE(result.m_candidates.front().m_icon.m_id == "nf-cod-settings_gear");

  options.m_queries = {"cog", "outline"};
  const auto multiple = catalog.search(options);
  REQUIRE(multiple.m_total == 3);
  REQUIRE(multiple.m_candidates.front().m_icon.m_id == "nf-md-cog_outline");

  options.m_queries = {"cog,cog COG"};
  const auto repeated = catalog.search(options);
  options.m_queries = {"cog"};
  const auto once = catalog.search(options);
  REQUIRE(ids(repeated) == ids(once));
  REQUIRE(repeated.m_candidates.front().m_score == once.m_candidates.front().m_score);
}

TEST_CASE("Search supports Japanese keyword groups and exact codepoints",
          "[catalog]") {
  const auto catalog = fixtureCatalog();
  findicon::SearchOptions options;
  options.m_queries = {"設定"};
  REQUIRE(catalog.search(options).m_total == 4);
  options.m_queries = {"回転"};
  REQUIRE(catalog.search(options).m_total == 2);
  options.m_queries = {"非表示"};
  REQUIRE(ids(catalog.search(options)) ==
          std::vector<std::string>{"nf-md-eye_off"});

  for (const std::string value : {"U+F0493", "f0493",
                                  "\xF3\xB0\x92\x93"}) {
    options.m_queries = {value};
    REQUIRE(ids(catalog.search(options)) ==
            std::vector<std::string>{"nf-md-cog"});
  }
}

TEST_CASE("Family filters select an alias from the requested set", "[catalog]") {
  const auto catalog = fixtureCatalog();
  findicon::SearchOptions options;
  options.m_queries = {"bazel"};
  auto result = catalog.search(options);
  REQUIRE(result.m_total == 1);
  REQUIRE(result.m_candidates[0].m_icon.m_id == "nf-custom-bazel");
  REQUIRE(result.m_candidates[0].m_aliases ==
          std::vector<std::string>{"nf-seti-bazel"});

  options.m_families = {"SETI"};
  result = catalog.search(options);
  REQUIRE(result.m_total == 1);
  REQUIRE(result.m_candidates[0].m_icon.m_id == "nf-seti-bazel");
  REQUIRE(result.m_candidates[0].m_aliases.empty());

  options.m_families = {"unknown"};
  REQUIRE_THROWS_AS(catalog.search(options), std::invalid_argument);
}

TEST_CASE("Pagination counts unique glyphs and remains deterministic",
          "[catalog]") {
  const auto catalog = fixtureCatalog();
  findicon::SearchOptions options;
  options.m_all = true;
  const auto all = catalog.search(options);
  REQUIRE(all.m_total == 9);
  REQUIRE(all.m_candidates.size() == 9);
  const auto allIds = ids(all);
  REQUIRE(std::is_sorted(allIds.begin(), allIds.end()));

  options.m_offset = 2;
  options.m_limit = 3;
  auto page = catalog.search(options);
  REQUIRE(page.m_total == 9);
  REQUIRE(ids(page) == std::vector<std::string>(allIds.begin() + 2,
                                               allIds.begin() + 5));

  options.m_offset = std::numeric_limits<std::size_t>::max();
  options.m_limit = std::numeric_limits<std::size_t>::max();
  page = catalog.search(options);
  REQUIRE(page.m_total == 9);
  REQUIRE(page.m_candidates.empty());

  options = {};
  REQUIRE_THROWS_AS(catalog.search(options), std::invalid_argument);
  options.m_queries = {" \t, "};
  REQUIRE_THROWS_AS(catalog.search(options), std::invalid_argument);
  options.m_queries = {"cog"};
  options.m_limit = 0;
  REQUIRE_THROWS_AS(catalog.search(options), std::invalid_argument);
}

TEST_CASE("Catalog rejects corrupt mappings instead of returning wrong glyphs",
          "[catalog]") {
  const nlohmann::json original = nlohmann::json::parse(FIXTURE);
  const auto reject = [](const nlohmann::json &document) {
    const TemporaryCatalog temporary(document.dump());
    REQUIRE_THROWS_AS(findicon::Catalog::load(temporary.path()),
                      std::runtime_error);
  };

  SECTION("Character and code disagree") {
    auto document = original;
    document["md-cog"]["char"] = "x";
    reject(document);
  }
  SECTION("Surrogate, out of range, signed, and trailing code strings") {
    for (const auto code : {"d800", "110000", "-1", "f013junk", "0xf013"}) {
      auto document = original;
      document["fa-cog"]["code"] = code;
      reject(document);
    }
  }
  SECTION("Character is a sequence instead of one scalar") {
    auto document = original;
    document["fa-cog"]["char"] = "ab";
    reject(document);
  }
  SECTION("Missing or incorrectly typed fields") {
    auto document = original;
    document["fa-cog"].erase("char");
    reject(document);
    document = original;
    document["fa-cog"]["code"] = 61459;
    reject(document);
    document = original;
    document.erase("METADATA");
    reject(document);
  }
  SECTION("Duplicate normalized identifier") {
    auto document = original;
    document["nf-fa-cog"] = document["fa-cog"];
    reject(document);
  }
  SECTION("Empty catalog and unrelated JSON") {
    reject({{"METADATA", {{"version", "empty"}}}});
    reject(nlohmann::json::array());
  }
  SECTION("Malformed JSON") {
    const TemporaryCatalog temporary("{not json");
    REQUIRE_THROWS_AS(findicon::Catalog::load(temporary.path()),
                      std::runtime_error);
  }
}

TEST_CASE("Unicode formatting supports supplementary planes and rejects surrogates",
          "[catalog]") {
  REQUIRE(findicon::formatCodepoint(0x41) == "U+0041");
  REQUIRE(findicon::formatCodepoint(0xF0493) == "U+F0493");
  REQUIRE(findicon::formatCodepoint(0x10FFFF) == "U+10FFFF");
  REQUIRE(findicon::encodeUtf8(0) == std::string(1, '\0'));
  REQUIRE(findicon::encodeUtf8(0x41) == "A");
  REQUIRE(findicon::encodeUtf8(0xE9) == "\xC3\xA9");
  REQUIRE(findicon::encodeUtf8(0xF013) == "\xEF\x80\x93");
  REQUIRE(findicon::encodeUtf8(0xF0493) == "\xF3\xB0\x92\x93");
  REQUIRE(findicon::encodeUtf8(0x10FFFF) == "\xF4\x8F\xBF\xBF");
  for (const std::uint32_t codepoint : {0xD800U, 0xDFFFU, 0x110000U}) {
    REQUIRE_THROWS_AS(findicon::formatCodepoint(codepoint),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(findicon::encodeUtf8(codepoint), std::invalid_argument);
  }
}
