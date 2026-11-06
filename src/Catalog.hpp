#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace findicon {

struct Icon {
  std::string m_id;
  std::string m_family;
  std::uint32_t m_codepoint = 0;
};

struct Candidate {
  Icon m_icon;
  std::vector<std::string> m_aliases;
  int m_score = 0;
};

struct SearchOptions {
  std::vector<std::string> m_queries;
  std::vector<std::string> m_families;
  std::size_t m_limit = 24;
  std::size_t m_offset = 0;
  bool m_all = false;
};

struct SearchResult {
  std::vector<Candidate> m_candidates;
  std::size_t m_total = 0;
};

class Catalog {
public:
  static Catalog load(const std::filesystem::path &path);
  [[nodiscard]] const std::vector<Icon> &icons() const { return m_icons; }
  [[nodiscard]] const std::string &version() const { return m_version; }
  [[nodiscard]] SearchResult search(const SearchOptions &options) const;
  [[nodiscard]] Candidate resolve(const std::string &id) const;

private:
  std::string m_version;
  std::vector<Icon> m_icons;
};

[[nodiscard]] std::string formatCodepoint(std::uint32_t codepoint);
[[nodiscard]] std::string encodeUtf8(std::uint32_t codepoint);

} // namespace findicon
