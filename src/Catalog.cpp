#include "Catalog.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace findicon {
namespace {

constexpr std::uint32_t MAX_CODEPOINT = 0x10FFFF;
constexpr std::uint32_t SURROGATE_START = 0xD800;
constexpr std::uint32_t SURROGATE_END = 0xDFFF;
constexpr std::size_t MAX_CATALOG_BYTES = 16 * 1024 * 1024;

[[nodiscard]] bool isScalar(std::uint32_t codepoint) {
  return codepoint <= MAX_CODEPOINT &&
         (codepoint < SURROGATE_START || codepoint > SURROGATE_END);
}

[[nodiscard]] bool isSpace(char value) {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
         value == '\f' || value == '\v';
}

[[nodiscard]] std::string_view trim(std::string_view value) {
  while (!value.empty() && isSpace(value.front())) {
    value.remove_prefix(1);
  }
  while (!value.empty() && isSpace(value.back())) {
    value.remove_suffix(1);
  }
  return value;
}

[[nodiscard]] std::string asciiLower(std::string_view value) {
  std::string result(value);
  for (char &character : result) {
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character - 'A' + 'a');
    }
  }
  return result;
}

[[nodiscard]] bool isNameCharacter(char value) {
  return (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9') || value == '_';
}

[[nodiscard]] std::optional<std::string>
normalizeId(std::string_view value) {
  std::string name = asciiLower(value);
  if (name.starts_with("nf-")) {
    name.erase(0, 3);
  }
  const std::size_t separator = name.find('-');
  if (separator == std::string::npos || separator == 0 ||
      separator + 1 == name.size()) {
    return std::nullopt;
  }
  for (std::size_t index = 0; index < name.size(); ++index) {
    if (index != separator && !isNameCharacter(name[index])) {
      return std::nullopt;
    }
  }
  return "nf-" + name;
}

[[nodiscard]] std::optional<std::uint32_t>
parseHex(std::string_view value) {
  if (value.empty() || value.size() > 6) {
    return std::nullopt;
  }
  std::uint32_t codepoint = 0;
  const auto result =
      std::from_chars(value.data(), value.data() + value.size(), codepoint, 16);
  if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
      !isScalar(codepoint)) {
    return std::nullopt;
  }
  return codepoint;
}

[[nodiscard]] std::optional<std::uint32_t>
decodeSingleCodepoint(std::string_view value) {
  if (value.empty()) {
    return std::nullopt;
  }
  const auto first = static_cast<unsigned char>(value.front());
  std::size_t length = 0;
  std::uint32_t codepoint = 0;
  std::uint32_t minimum = 0;
  if (first < 0x80) {
    length = 1;
    codepoint = first;
  } else if (first >= 0xC2 && first <= 0xDF) {
    length = 2;
    codepoint = first & 0x1F;
    minimum = 0x80;
  } else if (first >= 0xE0 && first <= 0xEF) {
    length = 3;
    codepoint = first & 0x0F;
    minimum = 0x800;
  } else if (first >= 0xF0 && first <= 0xF4) {
    length = 4;
    codepoint = first & 0x07;
    minimum = 0x10000;
  } else {
    return std::nullopt;
  }
  if (value.size() != length) {
    return std::nullopt;
  }
  for (std::size_t index = 1; index < length; ++index) {
    const auto continuation = static_cast<unsigned char>(value[index]);
    if ((continuation & 0xC0) != 0x80) {
      return std::nullopt;
    }
    codepoint = (codepoint << 6) | (continuation & 0x3F);
  }
  if (codepoint < minimum || !isScalar(codepoint)) {
    return std::nullopt;
  }
  return codepoint;
}

[[nodiscard]] std::optional<std::uint32_t>
parseCodepointQuery(std::string_view value) {
  if (value.starts_with("U+") || value.starts_with("u+") ||
      value.starts_with("0x") || value.starts_with("0X")) {
    return parseHex(value.substr(2));
  }
  if (value.size() >= 4 && value.size() <= 6) {
    if (const auto codepoint = parseHex(value)) {
      return codepoint;
    }
  }
  // A raw non-ASCII glyph is useful input; ASCII words and letters remain
  // name searches. resolve() also accepts single ASCII characters.
  if (!value.empty() && static_cast<unsigned char>(value.front()) >= 0x80) {
    return decodeSingleCodepoint(value);
  }
  return std::nullopt;
}

[[nodiscard]] std::vector<std::string> splitQuery(std::string_view value) {
  std::vector<std::string> terms;
  while (!value.empty()) {
    const std::size_t length = value.find_first_of(" \t\n\r\f\v,");
    const std::string_view term = value.substr(0, length);
    if (!term.empty()) {
      terms.push_back(asciiLower(term));
    }
    if (length == std::string_view::npos) {
      break;
    }
    value.remove_prefix(length + 1);
  }
  return terms;
}

struct QueryTerm {
  std::vector<std::string> m_alternatives;
  std::optional<std::uint32_t> m_codepoint;
};

[[nodiscard]] QueryTerm makeTerm(const std::string &query) {
  static const std::map<std::string, std::vector<std::string>> SYNONYMS = {
      {"設定", {"settings", "gear", "cog", "tune"}},
      {"移動", {"move", "arrow", "axis"}},
      {"回転", {"rotate", "rotation", "orbit"}},
      {"拡縮", {"resize", "expand", "scale"}},
      {"拡大", {"zoom_in", "expand", "maximize"}},
      {"縮小", {"zoom_out", "collapse", "minimize"}},
      {"保存", {"save", "floppy", "content_save"}},
      {"削除", {"delete", "trash", "remove"}},
      {"検索", {"search", "magnify"}},
      {"表示", {"eye", "visible", "visibility"}},
      {"非表示", {"eye_off", "eye_closed", "eye_slash", "hide"}},
      {"再生", {"play", "run"}},
      {"停止", {"stop", "pause"}},
  };
  if (const auto found = SYNONYMS.find(query); found != SYNONYMS.end()) {
    return {found->second, std::nullopt};
  }
  return {{query}, parseCodepointQuery(query)};
}

[[nodiscard]] int matchName(const Icon &icon, const std::string &term) {
  const std::string_view id(icon.m_id);
  const std::string_view shortId = id.substr(3);
  const std::string_view name = shortId.substr(icon.m_family.size() + 1);
  if (id == term || shortId == term) {
    return 100000;
  }
  if (name == term) {
    return 400;
  }

  // Prefix matching on full identifiers should not expand "md-cog" into
  // every icon in the md family.
  if (id.starts_with(term) || shortId.starts_with(term)) {
    return 80;
  }

  int score = 0;
  std::size_t position = shortId.find(term);
  while (position != std::string_view::npos) {
    const std::size_t end = position + term.size();
    const bool wordStart = position == 0 || shortId[position - 1] == '_' ||
                           shortId[position - 1] == '-';
    const bool wordEnd = end == shortId.size() || shortId[end] == '_' ||
                         shortId[end] == '-';
    score = std::max(score, wordStart && wordEnd ? 200 : wordStart ? 80 : 20);
    position = shortId.find(term, position + 1);
  }
  return score;
}

[[nodiscard]] int matchIcon(const Icon &icon,
                            const std::vector<QueryTerm> &terms) {
  int score = 0;
  int matchingTerms = 0;
  for (const QueryTerm &term : terms) {
    int termScore = term.m_codepoint == icon.m_codepoint ? 100000 : 0;
    for (const std::string &alternative : term.m_alternatives) {
      termScore = std::max(termScore, matchName(icon, alternative));
    }
    if (termScore > 0) {
      score += termScore;
      ++matchingTerms;
    }
  }
  return score + std::max(matchingTerms - 1, 0) * 300;
}

[[nodiscard]] Candidate makeCandidate(const Icon &representative,
                                      const std::vector<const Icon *> &group,
                                      int score) {
  Candidate candidate{representative, {}, score};
  for (const Icon *icon : group) {
    if (icon->m_id != representative.m_id) {
      candidate.m_aliases.push_back(icon->m_id);
    }
  }
  std::sort(candidate.m_aliases.begin(), candidate.m_aliases.end());
  return candidate;
}

} // namespace

Catalog Catalog::load(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Cannot open icon catalog: " + path.string());
  }
  // Bound allocations for accidentally supplied binaries or unrelated files.
  stream.seekg(0, std::ios::end);
  const std::streampos length = stream.tellg();
  if (length < 0 || static_cast<std::uintmax_t>(length) > MAX_CATALOG_BYTES) {
    throw std::runtime_error("Icon catalog exceeds the 16 MiB limit: " +
                             path.string());
  }
  stream.seekg(0, std::ios::beg);

  nlohmann::json document;
  try {
    document = nlohmann::json::parse(stream);
  } catch (const nlohmann::json::exception &error) {
    throw std::runtime_error("Invalid catalog JSON in " + path.string() +
                             ": " + error.what());
  }
  if (!document.is_object()) {
    throw std::runtime_error("Icon catalog must be a JSON object");
  }
  const auto metadata = document.find("METADATA");
  if (metadata == document.end() || !metadata->is_object() ||
      !metadata->contains("version") || !(*metadata)["version"].is_string() ||
      (*metadata)["version"].get_ref<const std::string &>().empty()) {
    throw std::runtime_error("Icon catalog requires METADATA.version");
  }

  Catalog catalog;
  catalog.m_version = (*metadata)["version"].get<std::string>();
  catalog.m_icons.reserve(document.size() - 1);
  std::set<std::string> identifiers;
  for (const auto &[key, value] : document.items()) {
    if (key == "METADATA") {
      continue;
    }
    const auto id = normalizeId(key);
    if (!id) {
      throw std::runtime_error("Invalid icon identifier in catalog: " + key);
    }
    if (!identifiers.insert(*id).second) {
      throw std::runtime_error("Duplicate normalized icon identifier: " + *id);
    }
    if (!value.is_object() || !value.contains("code") ||
        !value["code"].is_string() || !value.contains("char") ||
        !value["char"].is_string()) {
      throw std::runtime_error("Icon requires string code and char fields: " +
                               key);
    }
    const auto codepoint =
        parseHex(value["code"].get_ref<const std::string &>());
    if (!codepoint) {
      throw std::runtime_error("Invalid Unicode codepoint for icon: " + key);
    }
    const auto character =
        decodeSingleCodepoint(value["char"].get_ref<const std::string &>());
    if (!character || *character != *codepoint) {
      throw std::runtime_error("Icon char does not match its codepoint: " + key);
    }
    const std::size_t separator = id->find('-', 3);
    catalog.m_icons.push_back(
        {*id, id->substr(3, separator - 3), *codepoint});
  }
  if (catalog.m_icons.empty()) {
    throw std::runtime_error("Icon catalog contains no icons");
  }
  std::sort(catalog.m_icons.begin(), catalog.m_icons.end(),
            [](const Icon &left, const Icon &right) {
              return left.m_id < right.m_id;
            });
  return catalog;
}

SearchResult Catalog::search(const SearchOptions &options) const {
  if (options.m_limit == 0) {
    throw std::invalid_argument("Search limit must be greater than zero");
  }
  std::set<std::string> queryStrings;
  for (const std::string &query : options.m_queries) {
    for (std::string term : splitQuery(query)) {
      queryStrings.insert(std::move(term));
    }
  }
  if (queryStrings.empty() && !options.m_all) {
    throw std::invalid_argument("Provide a search query or use --all");
  }
  if (queryStrings.size() > 128) {
    throw std::invalid_argument("A search may contain at most 128 terms");
  }
  std::vector<QueryTerm> terms;
  terms.reserve(queryStrings.size());
  for (const std::string &query : queryStrings) {
    terms.push_back(makeTerm(query));
  }

  std::set<std::string> availableFamilies;
  for (const Icon &icon : m_icons) {
    availableFamilies.insert(icon.m_family);
  }
  std::set<std::string> families;
  for (const std::string &family : options.m_families) {
    const std::string normalized = asciiLower(trim(family));
    if (!availableFamilies.contains(normalized)) {
      throw std::invalid_argument("Unknown icon family: " + family);
    }
    families.insert(normalized);
  }

  std::map<std::uint32_t, std::vector<const Icon *>> groups;
  for (const Icon &icon : m_icons) {
    if (families.empty() || families.contains(icon.m_family)) {
      groups[icon.m_codepoint].push_back(&icon);
    }
  }

  std::vector<Candidate> candidates;
  candidates.reserve(groups.size());
  for (const auto &[codepoint, group] : groups) {
    static_cast<void>(codepoint);
    const Icon *representative = group.front();
    int bestScore = 0;
    for (const Icon *icon : group) {
      const int score = matchIcon(*icon, terms);
      if (score > bestScore) {
        bestScore = score;
        representative = icon;
      }
    }
    if (bestScore > 0 || terms.empty()) {
      candidates.push_back(makeCandidate(*representative, group, bestScore));
    }
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate &left, const Candidate &right) {
              if (left.m_score != right.m_score) {
                return left.m_score > right.m_score;
              }
              return left.m_icon.m_id < right.m_icon.m_id;
            });

  SearchResult result;
  result.m_total = candidates.size();
  const std::size_t begin = std::min(options.m_offset, candidates.size());
  const std::size_t count =
      std::min(options.m_limit, candidates.size() - begin);
  result.m_candidates.reserve(count);
  for (std::size_t index = begin; index < begin + count; ++index) {
    result.m_candidates.push_back(std::move(candidates[index]));
  }
  return result;
}

Candidate Catalog::resolve(const std::string &value) const {
  const std::string_view query = trim(value);
  if (query.empty()) {
    throw std::invalid_argument("Icon identifier must not be empty");
  }
  const auto normalized = normalizeId(query);
  const Icon *representative = nullptr;
  if (normalized) {
    const auto found = std::lower_bound(
        m_icons.begin(), m_icons.end(), *normalized,
        [](const Icon &icon, const std::string &id) { return icon.m_id < id; });
    if (found != m_icons.end() && found->m_id == *normalized) {
      representative = &*found;
    }
  }

  auto codepoint = parseCodepointQuery(query);
  if (!codepoint && !normalized) {
    codepoint = decodeSingleCodepoint(query);
  }
  if (!representative && codepoint) {
    for (const Icon &icon : m_icons) {
      if (icon.m_codepoint == *codepoint) {
        representative = &icon;
        break;
      }
    }
  }
  if (!representative) {
    throw std::invalid_argument("Icon is not present in the catalog: " + value);
  }

  std::vector<const Icon *> group;
  for (const Icon &icon : m_icons) {
    if (icon.m_codepoint == representative->m_codepoint) {
      group.push_back(&icon);
    }
  }
  return makeCandidate(*representative, group, 0);
}

std::string formatCodepoint(std::uint32_t codepoint) {
  if (!isScalar(codepoint)) {
    throw std::invalid_argument("Codepoint is not a Unicode scalar value");
  }
  std::ostringstream stream;
  stream << "U+" << std::uppercase << std::hex << std::setfill('0')
         << std::setw(4) << codepoint;
  return stream.str();
}

std::string encodeUtf8(std::uint32_t codepoint) {
  if (!isScalar(codepoint)) {
    throw std::invalid_argument("Codepoint is not a Unicode scalar value");
  }
  std::string result;
  if (codepoint <= 0x7F) {
    result.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FF) {
    result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
    result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0xFFFF) {
    result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
    result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else {
    result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
    result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
  return result;
}

} // namespace findicon
