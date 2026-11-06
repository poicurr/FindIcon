#include "Catalog.hpp"
#include "Output.hpp"
#include "Preview.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr std::string_view VERSION = "0.1.0";
constexpr int EXIT_NO_MATCHES = 3;
constexpr int EXIT_INCOMPLETE_PREVIEW = 4;

struct Options {
  std::string m_command;
  findicon::SearchOptions m_search;
  std::vector<std::string> m_ids;
  fs::path m_dataDir;
  fs::path m_dataPath;
  fs::path m_outputPath;
  fs::path m_manifestPath;
  findicon::PreviewOptions m_preview;
  bool m_text = false;
  bool m_strict = false;
  bool m_help = false;
  bool m_version = false;
};

[[nodiscard]] std::string pathString(const fs::path &path) {
  const auto utf8 = path.generic_u8string();
  return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
}

[[nodiscard]] fs::path utf8Path(const std::string &value) {
  if (value.empty()) {
    throw std::invalid_argument("A path argument must not be empty");
  }
  return fs::path(std::u8string(value.begin(), value.end()));
}

void printHelp() {
  std::cout << R"(findicon - local Nerd Font search and visual selection

Usage:
  findicon search  <query...> [options]
  findicon preview <query...> --out Candidates.png [options]
  findicon preview --id nf-cod-settings_gear --id U+F0493 --out Selected.png
  findicon info <id-or-codepoint...> [options]
  findicon families [--text]

Search:
  --family <cod,md,...>  Restrict icon families; repeatable
  --limit <1..256>      Maximum candidates (default: 24)
  --offset <number>     Skip ranked unique-codepoint results (default: 0)
  --all                 Browse without a query; still paginated
  --id <id>             Explicit icon ID/codepoint for preview; repeatable

Preview:
  --out <path.png>       Output PNG (default: Candidates.png)
  --manifest <path>      Output JSON (default: same stem as PNG)
  --size <8..96>         Actual-size glyph in pixels (default: 20)
  --large-size <8..256>  Large glyph in pixels (default: 64)
  --columns <1..8>       Contact-sheet columns (default: 4)
  --theme <dark|light>   Sheet colors (default: dark)
  --strict               Exit 4 if any glyph is missing or clipped

Files:
  --data-dir <dir>       Asset directory; overrides FINDICON_DATA_DIR
  --data <path.json>     Override official glyphnames.json
  --font <path.ttf>      Override icon font for preview
  --label-font <path>    Override Latin label font for preview

Output:
  --json                 Machine-readable JSON (default)
  --text                 Compact human-readable output
  --out <path.json>      For search/info/families, also save output as JSON
  --help, -h             Show this help
  --version              Show tool version

Queries are OR-matched, ranked, and deduplicated by codepoint. Exact IDs,
whole words and multiple matches rank ahead of partial matches. A small
Japanese UI keyword dictionary is included; full natural-language search
is not performed. Preview produces both PNG and its numbered JSON mapping.
Exit codes: 0 success; 2 invalid input/I/O error; 3 no matches;
            4 incomplete preview with --strict (files are still written).
)";
}

[[nodiscard]] std::size_t parseNumber(const std::string &value,
                                    std::string_view option,
                                    std::size_t minimum,
                                    std::size_t maximum) {
  std::size_t number = 0;
  const auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), number);
  if (error != std::errc{} || end != value.data() + value.size() ||
      number < minimum || number > maximum) {
    throw std::invalid_argument(std::string(option) + " must be in [" +
                                std::to_string(minimum) + ", " +
                                std::to_string(maximum) + "]");
  }
  return number;
}

void appendCommaSeparated(std::vector<std::string> &values,
                          const std::string &input) {
  std::size_t begin = 0;
  do {
    const auto end = input.find(',', begin);
    const std::string part = input.substr(begin, end - begin);
    if (part.empty()) {
      throw std::invalid_argument("Empty item in comma-separated argument");
    }
    values.push_back(part);
    if (end == std::string::npos) {
      break;
    }
    begin = end + 1;
  } while (true);
}

[[nodiscard]] Options parseOptions(const std::vector<std::string> &args) {
  Options options;
  bool positionalOnly = false;
  bool previewFlag = false;
  bool searchFlag = false;
  for (std::size_t index = 1; index < args.size(); ++index) {
    const auto &argument = args[index];
    const auto value = [&]() -> const std::string & {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("Missing value for " + argument);
      }
      return args[++index];
    };
    if (!positionalOnly && argument == "--") {
      positionalOnly = true;
    } else if (!positionalOnly &&
               (argument == "--help" || argument == "-h")) {
      options.m_help = true;
    } else if (!positionalOnly && argument == "--version") {
      options.m_version = true;
    } else if (!positionalOnly && argument == "--json") {
      options.m_text = false;
    } else if (!positionalOnly && argument == "--text") {
      options.m_text = true;
    } else if (!positionalOnly && argument == "--all") {
      options.m_search.m_all = true;
      searchFlag = true;
    } else if (!positionalOnly && argument == "--strict") {
      options.m_strict = true;
      previewFlag = true;
    } else if (!positionalOnly && argument == "--family") {
      appendCommaSeparated(options.m_search.m_families, value());
      searchFlag = true;
    } else if (!positionalOnly && argument == "--limit") {
      options.m_search.m_limit = parseNumber(value(), argument, 1, 256);
      searchFlag = true;
    } else if (!positionalOnly && argument == "--offset") {
      options.m_search.m_offset = parseNumber(value(), argument, 0, 1000000000);
      searchFlag = true;
    } else if (!positionalOnly && argument == "--id") {
      appendCommaSeparated(options.m_ids, value());
      previewFlag = true;
    } else if (!positionalOnly && argument == "--data-dir") {
      options.m_dataDir = utf8Path(value());
    } else if (!positionalOnly && argument == "--data") {
      options.m_dataPath = utf8Path(value());
    } else if (!positionalOnly && argument == "--font") {
      options.m_preview.m_fontPath = utf8Path(value());
      previewFlag = true;
    } else if (!positionalOnly && argument == "--label-font") {
      options.m_preview.m_labelFontPath = utf8Path(value());
      previewFlag = true;
    } else if (!positionalOnly && argument == "--out") {
      options.m_outputPath = utf8Path(value());
    } else if (!positionalOnly && argument == "--manifest") {
      options.m_manifestPath = utf8Path(value());
      previewFlag = true;
    } else if (!positionalOnly && argument == "--size") {
      options.m_preview.m_pixelSize =
          static_cast<int>(parseNumber(value(), argument, 8, 96));
      previewFlag = true;
    } else if (!positionalOnly && argument == "--large-size") {
      options.m_preview.m_largePixelSize =
          static_cast<int>(parseNumber(value(), argument, 8, 256));
      previewFlag = true;
    } else if (!positionalOnly && argument == "--columns") {
      options.m_preview.m_columns =
          static_cast<int>(parseNumber(value(), argument, 1, 8));
      previewFlag = true;
    } else if (!positionalOnly && argument == "--theme") {
      const std::string theme = value();
      if (theme != "dark" && theme != "light") {
        throw std::invalid_argument("--theme must be dark or light");
      }
      options.m_preview.m_light = theme == "light";
      previewFlag = true;
    } else if (!positionalOnly && argument.starts_with('-')) {
      throw std::invalid_argument("Unknown option: " + argument);
    } else if (options.m_command.empty()) {
      options.m_command = argument;
    } else {
      options.m_search.m_queries.push_back(argument);
    }
  }
  if (options.m_help || options.m_version || args.size() == 1) {
    options.m_help = options.m_help || args.size() == 1;
    return options;
  }
  if (options.m_command != "search" && options.m_command != "preview" &&
      options.m_command != "info" && options.m_command != "families") {
    throw std::invalid_argument("Expected search, preview, info or families");
  }
  if (previewFlag && options.m_command != "preview") {
    throw std::invalid_argument("Preview-only option used with " +
                                options.m_command);
  }
  if (searchFlag && (options.m_command == "info" ||
                     options.m_command == "families")) {
    throw std::invalid_argument("Search-only option used with " +
                                options.m_command);
  }
  if (!options.m_ids.empty() &&
      (!options.m_search.m_queries.empty() || searchFlag)) {
    throw std::invalid_argument("--id cannot be combined with queries or search "
                                "options; use one selection method");
  }
  if (options.m_command == "families" && !options.m_search.m_queries.empty()) {
    throw std::invalid_argument("families does not accept a query");
  }
  if (options.m_command == "info" && options.m_search.m_queries.empty()) {
    throw std::invalid_argument("info requires an icon ID or codepoint");
  }
  return options;
}

[[nodiscard]] fs::path executablePath(const std::string &argumentZero) {
#if defined(_WIN32)
  std::vector<wchar_t> buffer(32768);
  const auto length = GetModuleFileNameW(nullptr, buffer.data(),
                                        static_cast<DWORD>(buffer.size()));
  if (length > 0 && length < buffer.size()) {
    return fs::path(std::wstring(buffer.data(), length));
  }
#elif defined(__linux__)
  std::error_code error;
  const auto executable = fs::read_symlink("/proc/self/exe", error);
  if (!error) {
    return executable;
  }
#elif defined(__APPLE__)
  std::uint32_t length = 0;
  _NSGetExecutablePath(nullptr, &length);
  std::vector<char> buffer(length);
  if (_NSGetExecutablePath(buffer.data(), &length) == 0) {
    return fs::weakly_canonical(utf8Path(buffer.data()));
  }
#endif
  return fs::absolute(utf8Path(argumentZero));
}

[[nodiscard]] fs::path discoverDataDir(const fs::path &executable,
                                      const Options &options) {
  if (!options.m_dataDir.empty()) {
    return options.m_dataDir;
  }
#if defined(_WIN32)
  if (const auto *environment = _wgetenv(L"FINDICON_DATA_DIR");
      environment && *environment) {
    return fs::path(environment);
  }
#else
  if (const char *environment = std::getenv("FINDICON_DATA_DIR");
      environment && *environment) {
    return utf8Path(environment);
  }
#endif
  const fs::path parent = executable.parent_path();
  for (const auto &directory :
       {parent / "assets", parent.parent_path() / "share" / "findicon"}) {
    if (fs::is_regular_file(directory / "GlyphNames.json")) {
      return directory;
    }
  }
  return parent / "assets";
}

[[nodiscard]] bool sameFile(const fs::path &a, const fs::path &b) {
  if (a.empty() || b.empty()) {
    return false;
  }
  std::error_code error;
  if (fs::equivalent(a, b, error) && !error) {
    return true;
  }
  return fs::weakly_canonical(fs::absolute(a)) ==
         fs::weakly_canonical(fs::absolute(b));
}

void checkOutput(const fs::path &output,
                 const std::vector<fs::path> &inputs) {
  if (output.empty()) {
    throw std::invalid_argument("Output filename must not be empty");
  }
  for (const auto &input : inputs) {
    if (sameFile(output, input)) {
      throw std::invalid_argument("Output would overwrite an input file: " +
                                  pathString(output));
    }
  }
  if (fs::is_directory(output)) {
    throw std::invalid_argument("Output is a directory: " + pathString(output));
  }
  if (!output.parent_path().empty()) {
    fs::create_directories(output.parent_path());
  }
}

void saveJson(const fs::path &output, const Json &value) {
  std::ofstream file(output, std::ios::binary | std::ios::trunc);
  if (!file) {
    throw std::runtime_error("Cannot open JSON output: " + pathString(output));
  }
  file << value.dump(2) << '\n';
  file.close();
  if (!file) {
    throw std::runtime_error("Cannot write JSON output: " + pathString(output));
  }
}

[[nodiscard]] Json candidateJson(const findicon::Candidate &candidate,
                                 std::size_t index) {
  return {{"index", index + 1},
          {"id", candidate.m_icon.m_id},
          {"family", candidate.m_icon.m_family},
          {"codepoint", findicon::formatCodepoint(candidate.m_icon.m_codepoint)},
          {"codepoint_decimal", candidate.m_icon.m_codepoint},
          {"utf8", findicon::encodeUtf8(candidate.m_icon.m_codepoint)},
          {"aliases", candidate.m_aliases},
          {"score", candidate.m_score}};
}

[[nodiscard]] findicon::SearchResult
resolveIds(const findicon::Catalog &catalog,
           const std::vector<std::string> &ids) {
  if (ids.size() > 256) {
    throw std::invalid_argument("At most 256 explicit icons are allowed");
  }
  findicon::SearchResult result;
  std::set<std::uint32_t> seen;
  for (const auto &id : ids) {
    auto candidate = catalog.resolve(id);
    if (seen.insert(candidate.m_icon.m_codepoint).second) {
      result.m_candidates.push_back(std::move(candidate));
    }
  }
  result.m_total = result.m_candidates.size();
  return result;
}

void printSelection(const Json &output, bool text) {
  if (!text) {
    std::cout << output.dump(2) << '\n';
    return;
  }
  std::cout << "Nerd Fonts " << output.at("catalog_version").get<std::string>()
            << ": " << output.at("count") << " of " << output.at("total")
            << " unique codepoints\n";
  for (const auto &candidate : output.at("candidates")) {
    std::cout << candidate.at("index") << '\t'
              << candidate.at("id").get<std::string>() << '\t'
              << candidate.at("codepoint").get<std::string>();
    if (candidate.contains("supported") &&
        !candidate.at("supported").get<bool>()) {
      std::cout << "\tMISSING";
    }
    if (candidate.value("clipped", false)) {
      std::cout << "\tCLIPPED";
    }
    std::cout << '\n';
  }
  if (output.contains("preview")) {
    std::cout << "PNG: "
              << output.at("preview").at("image").get<std::string>() << '\n'
              << "JSON: "
              << output.at("preview").at("manifest").get<std::string>()
              << '\n';
  }
}

int run(const std::vector<std::string> &args) {
  auto options = parseOptions(args);
  if (options.m_help) {
    printHelp();
    return 0;
  }
  if (options.m_version) {
    std::cout << "findicon " << VERSION << '\n';
    return 0;
  }
  const auto dataDir = discoverDataDir(executablePath(args.front()), options);
  if (options.m_dataPath.empty()) {
    options.m_dataPath = dataDir / "GlyphNames.json";
  }
  if (options.m_preview.m_fontPath.empty()) {
    options.m_preview.m_fontPath = dataDir / "SymbolsNerdFont-Regular.ttf";
  }
  if (options.m_preview.m_labelFontPath.empty()) {
    options.m_preview.m_labelFontPath = dataDir / "LabelFont.ttf";
  }
  const std::vector<fs::path> inputs = {options.m_dataPath,
                                       options.m_preview.m_fontPath,
                                       options.m_preview.m_labelFontPath};
  const auto catalog = findicon::Catalog::load(options.m_dataPath);
  if (options.m_command == "families") {
    std::map<std::string, std::set<std::uint32_t>> families;
    std::map<std::string, std::size_t> names;
    for (const auto &icon : catalog.icons()) {
      families[icon.m_family].insert(icon.m_codepoint);
      ++names[icon.m_family];
    }
    Json entries = Json::array();
    for (const auto &[family, codepoints] : families) {
      entries.push_back({{"family", family}, {"names", names[family]},
                         {"unique_codepoints", codepoints.size()}});
    }
    const Json output = {{"schema_version", 1},
                          {"catalog_version", catalog.version()},
                          {"families", entries}};
    if (!options.m_outputPath.empty()) {
      checkOutput(options.m_outputPath, inputs);
      saveJson(options.m_outputPath, output);
    }
    if (options.m_text) {
      for (const auto &entry : entries) {
        std::cout << entry.at("family").get<std::string>() << '\t'
                  << entry.at("names") << " names\t"
                  << entry.at("unique_codepoints") << " codepoints\n";
      }
    } else {
      std::cout << output.dump(2) << '\n';
    }
    return 0;
  }
  const auto result =
      options.m_command == "info"
          ? resolveIds(catalog, options.m_search.m_queries)
          : (!options.m_ids.empty() ? resolveIds(catalog, options.m_ids)
                                 : catalog.search(options.m_search));
  Json candidates = Json::array();
  for (std::size_t index = 0; index < result.m_candidates.size(); ++index) {
    candidates.push_back(candidateJson(result.m_candidates[index], index));
  }
  Json output = {{"schema_version", 1},
                 {"catalog_version", catalog.version()},
                 {"query", options.m_search.m_queries},
                 {"family_filter", options.m_search.m_families},
                 {"total", result.m_total},
                 {"offset", options.m_search.m_offset},
                 {"count", candidates.size()},
                 {"has_more", options.m_search.m_offset < result.m_total &&
                                  candidates.size() <
                                      result.m_total - options.m_search.m_offset},
                 {"candidates", candidates}};
  if (options.m_command != "preview") {
    if (!options.m_outputPath.empty()) {
      checkOutput(options.m_outputPath, inputs);
      saveJson(options.m_outputPath, output);
    }
    printSelection(output, options.m_text);
    return result.m_candidates.empty() ? EXIT_NO_MATCHES : 0;
  }
  if (result.m_candidates.empty()) {
    output["preview"] = nullptr;
    if (options.m_text) {
      std::cout << "No matching icons. No preview files were written.\n";
    } else {
      std::cout << output.dump(2) << '\n';
    }
    return EXIT_NO_MATCHES;
  }
  if (options.m_outputPath.empty()) {
    options.m_outputPath = "Candidates.png";
  }
  std::string extension = pathString(options.m_outputPath.extension());
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return c >= 'A' && c <= 'Z' ?
                                               static_cast<char>(c + 32) :
                                               static_cast<char>(c); });
  if (extension != ".png") {
    throw std::invalid_argument("Preview --out must have a .png extension");
  }
  if (options.m_manifestPath.empty()) {
    options.m_manifestPath = options.m_outputPath;
    options.m_manifestPath.replace_extension(".json");
  }
  checkOutput(options.m_outputPath, inputs);
  checkOutput(options.m_manifestPath, inputs);
  if (sameFile(options.m_outputPath, options.m_manifestPath)) {
    throw std::invalid_argument("PNG and manifest paths must differ");
  }
  findicon::PreviewOutput stagedOutput(options.m_outputPath,
                                       options.m_manifestPath);
  options.m_preview.m_outputPath = stagedOutput.pngTemporary();
  const auto preview =
      findicon::renderPreview(result.m_candidates, options.m_preview);
  if (preview.m_cells.size() != result.m_candidates.size()) {
    throw std::logic_error("Preview cell count does not match the selection");
  }
  std::size_t missingCount = 0;
  std::size_t clippedCount = 0;
  for (std::size_t index = 0; index < preview.m_cells.size(); ++index) {
    const auto &cell = preview.m_cells[index];
    if (cell.m_id != result.m_candidates[index].m_icon.m_id) {
      throw std::logic_error("Preview cell ID does not match the selection");
    }
    auto &candidate = output["candidates"].at(index);
    candidate["supported"] = cell.m_supported;
    candidate["clipped"] = cell.m_clipped;
    candidate["cell"] = {{"x", cell.m_x}, {"y", cell.m_y},
                          {"width", cell.m_width}, {"height", cell.m_height}};
    candidate["metrics"] = {{"advance", cell.m_advance},
                             {"bearing_x", cell.m_bearingX},
                             {"bearing_y", cell.m_bearingY}};
    missingCount += cell.m_supported ? 0U : 1U;
    clippedCount += cell.m_clipped ? 1U : 0U;
  }
  output["preview"] = {
      {"image", pathString(fs::absolute(options.m_outputPath))},
      {"manifest", pathString(fs::absolute(options.m_manifestPath))},
      {"font", pathString(fs::absolute(options.m_preview.m_fontPath))},
      {"label_font", pathString(fs::absolute(options.m_preview.m_labelFontPath))},
      {"font_family", preview.m_fontFamily},
      {"font_style", preview.m_fontStyle},
      {"font_version", preview.m_fontVersion},
      {"width", preview.m_width}, {"height", preview.m_height},
      {"pixel_size", options.m_preview.m_pixelSize},
      {"large_pixel_size", options.m_preview.m_largePixelSize},
      {"theme", options.m_preview.m_light ? "light" : "dark"},
      {"missing_count", missingCount}, {"clipped_count", clippedCount}};
  saveJson(stagedOutput.manifestTemporary(), output);
  stagedOutput.commit();
  printSelection(output, options.m_text);
  return options.m_strict && (missingCount != 0 || clippedCount != 0)
             ? EXIT_INCOMPLETE_PREVIEW
             : 0;
}

int reportError(const std::exception &error) {
  // cerr is tied to cout by default. After a failed stdout write, reporting
  // the error must not trigger another throwing flush on the same stream.
  std::cout.exceptions(std::ios::goodbit);
  std::cerr.tie(nullptr);
  std::cerr << "findicon: " << error.what() << '\n';
  return 2;
}

} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
  try {
    std::cout.exceptions(std::ios::badbit | std::ios::failbit);
    std::vector<std::string> args;
    for (int index = 0; index < argc; ++index) {
      const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                             argv[index], -1, nullptr, 0,
                                             nullptr, nullptr);
      if (length <= 0) {
        throw std::runtime_error("Cannot convert command line to UTF-8");
      }
      std::string value(static_cast<std::size_t>(length), '\0');
      if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[index], -1,
                              value.data(), length, nullptr, nullptr) <= 0) {
        throw std::runtime_error("Cannot convert command line to UTF-8");
      }
      value.pop_back();
      args.push_back(std::move(value));
    }
    const int result = run(args);
    std::cout.flush();
    return result;
  } catch (const std::exception &error) {
    return reportError(error);
  }
}
#else
int main(int argc, char **argv) {
  try {
    std::cout.exceptions(std::ios::badbit | std::ios::failbit);
    const int result = run(std::vector<std::string>(argv, argv + argc));
    std::cout.flush();
    return result;
  } catch (const std::exception &error) {
    return reportError(error);
  }
}
#endif
