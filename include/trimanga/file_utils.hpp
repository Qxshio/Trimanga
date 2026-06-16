#pragma once

#include <filesystem>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace trimanga {

struct ArchiveImage {
  std::string archive_name;
  std::vector<std::uint8_t> bytes;
};

struct ArchiveEntry {
  std::string archive_name;
  std::size_t index = 0;
  std::size_t uncompressed_size = 0;
};

class CbzImageReader {
 public:
  explicit CbzImageReader(const std::filesystem::path& cbz);
  ~CbzImageReader();
  CbzImageReader(const CbzImageReader&) = delete;
  CbzImageReader& operator=(const CbzImageReader&) = delete;

  const std::vector<ArchiveEntry>& entries() const;
  ArchiveImage read_image(const ArchiveEntry& entry);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

bool is_image_path(const std::filesystem::path& path);
bool is_cbz_path(const std::filesystem::path& path);
std::vector<std::filesystem::path> list_images_recursive(const std::filesystem::path& root);
std::filesystem::path make_temp_dir(const std::string& prefix);
void copy_file_create_dirs(const std::filesystem::path& from, const std::filesystem::path& to);
std::vector<std::filesystem::path> extract_cbz(const std::filesystem::path& cbz, const std::filesystem::path& destination);
void remove_archive_entries(const std::filesystem::path& archive_path, const std::unordered_set<std::string>& removed_entries);
void replace_archive_from_directory(const std::filesystem::path& source_dir, const std::filesystem::path& archive_path);
std::string path_label(const std::filesystem::path& path);

}  // namespace trimanga
