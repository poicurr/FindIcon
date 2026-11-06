#pragma once

#include <filesystem>
#include <string>

namespace findicon {

// Stages a PNG and its manifest beside their destinations. commit() restores
// previous files if an ordinary filesystem operation fails. This is not a
// crash-atomic transaction across two files or protection against concurrent
// writers to the same destinations.
class PreviewOutput {
public:
  PreviewOutput(std::filesystem::path pngPath,
                std::filesystem::path manifestPath);
  ~PreviewOutput();

  PreviewOutput(const PreviewOutput &) = delete;
  PreviewOutput &operator=(const PreviewOutput &) = delete;
  PreviewOutput(PreviewOutput &&) = delete;
  PreviewOutput &operator=(PreviewOutput &&) = delete;

  [[nodiscard]] const std::filesystem::path &pngTemporary() const noexcept;
  [[nodiscard]] const std::filesystem::path &manifestTemporary() const noexcept;

  // Both temporary files must be complete, closed, nonempty regular files.
  // A PreviewOutput accepts at most one commit attempt.
  void commit();

private:
  struct Entry {
    std::filesystem::path m_destination;
    std::filesystem::path m_directory;
    std::filesystem::path m_temporary;
    std::filesystem::path m_backup;
    bool m_backedUp = false;
    bool m_installed = false;
  };

  static void prepare(Entry &entry, const char *name);
  static void validateTemporary(const Entry &entry);
  static void backup(Entry &entry);
  static void install(Entry &entry);
  static bool restore(Entry &entry, std::string &errors);
  void cleanup() noexcept;

  Entry m_png;
  Entry m_manifest;
  bool m_attempted = false;
  bool m_preserveBackups = false;
};

} // namespace findicon
