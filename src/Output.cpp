#include "Output.hpp"

#include <atomic>
#include <chrono>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace findicon {
namespace {

namespace fs = std::filesystem;

[[nodiscard]] std::string pathString(const fs::path &path) {
  const auto utf8 = path.generic_u8string();
  return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
}

[[nodiscard]] fs::path destinationPath(const fs::path &path) {
  if (path.empty()) {
    throw std::invalid_argument("Preview output paths must not be empty");
  }
  fs::path destination = fs::absolute(path).lexically_normal();
  if (!destination.has_filename()) {
    throw std::invalid_argument("Preview output requires a filename: " +
                                pathString(path));
  }
  return destination;
}

[[nodiscard]] bool sameFile(const fs::path &left, const fs::path &right) {
  std::error_code error;
  if (fs::equivalent(left, right, error)) {
    return true;
  }
  if (error && error != std::errc::no_such_file_or_directory) {
    throw fs::filesystem_error("Cannot compare preview output paths", left,
                               right, error);
  }
  return fs::weakly_canonical(left) == fs::weakly_canonical(right);
}

[[nodiscard]] fs::path createStagingDirectory(const fs::path &parent) {
  static std::atomic<unsigned long long> s_sequence{0};
  constexpr unsigned int MAX_ATTEMPTS = 100;
  fs::create_directories(parent);
  for (unsigned int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
    const auto timestamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream name;
    name << ".findicon-" << std::hex << timestamp << '-'
         << s_sequence.fetch_add(1);
    const fs::path candidate = parent / name.str();
    std::error_code error;
    // Directory creation reserves the namespace atomically on both POSIX and
    // Windows; ordinary ofstream creation would not provide exclusive access.
    if (fs::create_directory(candidate, error)) {
      return candidate;
    }
    if (error && error != std::errc::file_exists) {
      throw fs::filesystem_error("Cannot create preview staging directory",
                                 candidate, error);
    }
  }
  throw std::runtime_error("Cannot reserve a unique preview staging directory");
}

void appendError(std::string &errors, const std::string &operation,
                 const fs::path &path, const std::error_code &error) {
  if (!errors.empty()) {
    errors += "; ";
  }
  errors += operation + " " + pathString(path) + ": " + error.message();
}

} // namespace

PreviewOutput::PreviewOutput(fs::path pngPath, fs::path manifestPath) {
  m_png.m_destination = destinationPath(pngPath);
  m_manifest.m_destination = destinationPath(manifestPath);
  if (sameFile(m_png.m_destination, m_manifest.m_destination)) {
    throw std::invalid_argument("PNG and manifest paths must differ");
  }
  try {
    prepare(m_png, "Image.png");
    prepare(m_manifest, "Manifest.json");
  } catch (...) {
    cleanup();
    throw;
  }
}

PreviewOutput::~PreviewOutput() { cleanup(); }

const fs::path &PreviewOutput::pngTemporary() const noexcept {
  return m_png.m_temporary;
}

const fs::path &PreviewOutput::manifestTemporary() const noexcept {
  return m_manifest.m_temporary;
}

void PreviewOutput::prepare(Entry &entry, const char *name) {
  entry.m_directory = createStagingDirectory(entry.m_destination.parent_path());
  entry.m_temporary = entry.m_directory / name;
  entry.m_backup = entry.m_directory / "Previous";
}

void PreviewOutput::validateTemporary(const Entry &entry) {
  if (!fs::is_regular_file(fs::symlink_status(entry.m_temporary)) ||
      fs::file_size(entry.m_temporary) == 0) {
    throw std::runtime_error("Preview staging file is missing or empty: " +
                             pathString(entry.m_temporary));
  }
}

void PreviewOutput::backup(Entry &entry) {
  std::error_code error;
  const auto status = fs::symlink_status(entry.m_destination, error);
  if (error && error != std::errc::no_such_file_or_directory) {
    throw fs::filesystem_error("Cannot inspect previous preview output",
                               entry.m_destination, error);
  }
  if (!fs::exists(status)) {
    return;
  }
  // Preserve symlinks as directory entries, including dangling symlinks.
  // Never move a directory, device or other special file into the backup.
  if (!fs::is_regular_file(status) && !fs::is_symlink(status)) {
    throw std::runtime_error("Preview destination is not a regular file: " +
                             pathString(entry.m_destination));
  }
  fs::rename(entry.m_destination, entry.m_backup);
  entry.m_backedUp = true;
}

void PreviewOutput::install(Entry &entry) {
  fs::rename(entry.m_temporary, entry.m_destination);
  entry.m_installed = true;
}

bool PreviewOutput::restore(Entry &entry, std::string &errors) {
  std::error_code removeError;
  if (entry.m_installed) {
    fs::remove(entry.m_destination, removeError);
    if (!removeError) {
      entry.m_installed = false;
    }
  }
  if (entry.m_backedUp) {
    std::error_code restoreError;
    fs::rename(entry.m_backup, entry.m_destination, restoreError);
    if (!restoreError) {
      entry.m_backedUp = false;
      entry.m_installed = false;
      return true;
    }
    appendError(errors, "Could not restore", entry.m_destination, restoreError);
    return false;
  }
  if (removeError) {
    appendError(errors, "Could not remove new output", entry.m_destination,
                 removeError);
    return false;
  }
  return true;
}

void PreviewOutput::commit() {
  if (m_attempted) {
    throw std::logic_error("Preview output commit was already attempted");
  }
  m_attempted = true;
  // Incomplete generation cannot disturb either previous output.
  validateTemporary(m_png);
  validateTemporary(m_manifest);
  if (sameFile(m_png.m_destination, m_manifest.m_destination)) {
    throw std::invalid_argument("PNG and manifest paths must differ");
  }

  try {
    backup(m_png);
    install(m_png);
    // The PNG now exists: equivalent() catches case-insensitive aliases even
    // when neither destination existed during the constructor's checks.
    if (sameFile(m_png.m_destination, m_manifest.m_destination)) {
      throw std::invalid_argument("PNG and manifest refer to the same file");
    }
    backup(m_manifest);
    install(m_manifest);
  } catch (...) {
    const std::exception_ptr original = std::current_exception();
    std::string errors;
    // Mark backups protected before recovery. Even if reporting an error
    // throws, stack unwinding must not delete a remaining previous file.
    m_preserveBackups = true;
    const bool manifestRestored = restore(m_manifest, errors);
    const bool pngRestored = restore(m_png, errors);
    if (manifestRestored && pngRestored) {
      m_preserveBackups = false;
      std::rethrow_exception(original);
    }
    std::string message = "Preview commit failed and rollback was incomplete: " +
                          errors;
    for (const Entry *entry : {&m_png, &m_manifest}) {
      if (entry->m_backedUp) {
        message += "; previous file retained at " + pathString(entry->m_backup);
      }
    }
    throw std::runtime_error(message);
  }
  cleanup();
}

void PreviewOutput::cleanup() noexcept {
  for (Entry *entry : {&m_png, &m_manifest}) {
    if (entry->m_directory.empty()) {
      continue;
    }
    try {
      std::error_code error;
      if (!entry->m_temporary.empty()) {
        fs::remove(entry->m_temporary, error);
      }
      if (entry->m_backedUp && !m_preserveBackups) {
        error.clear();
        fs::remove(entry->m_backup, error);
        if (!error) {
          entry->m_backedUp = false;
        }
      }
      // Only remove empty directories. A failed rollback's backups and any
      // unrelated entries must survive cleanup.
      error.clear();
      fs::remove(entry->m_directory, error);
    } catch (...) {
      // Cleanup must not mask an I/O error or destroy retained backups.
    }
  }
}

} // namespace findicon
