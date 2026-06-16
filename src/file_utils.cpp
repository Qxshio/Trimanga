#include "trimanga/file_utils.hpp"

#include "miniz.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace fs = std::filesystem;

namespace trimanga {

namespace {

std::string lower_ext(const fs::path& path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return ext;
}

std::string zip_error_message(const std::string& action, const mz_zip_archive& archive) {
  return action + ": " + mz_zip_get_error_string(mz_zip_peek_last_error(const_cast<mz_zip_archive*>(&archive)));
}

bool is_safe_archive_name(const std::string& name) {
  if (name.empty() || name.front() == '/' || name.front() == '\\') {
    return false;
  }
  fs::path path{name};
  for (const auto& part : path) {
    if (part == "..") {
      return false;
    }
  }
  return true;
}

std::vector<fs::path> archive_files_recursive(const fs::path& root) {
  std::vector<fs::path> files;
  for (const auto& entry : fs::recursive_directory_iterator(root)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

}  // namespace

bool is_image_path(const fs::path& path) {
  const std::string ext = lower_ext(path);
  return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp" || ext == ".tif" || ext == ".tiff";
}

bool is_cbz_path(const fs::path& path) {
  return lower_ext(path) == ".cbz" || lower_ext(path) == ".zip";
}

std::vector<fs::path> list_images_recursive(const fs::path& root) {
  std::vector<fs::path> images;
  if (!fs::exists(root)) {
    return images;
  }
  for (const auto& entry : fs::recursive_directory_iterator(root)) {
    if (entry.is_regular_file() && is_image_path(entry.path())) {
      images.push_back(entry.path());
    }
  }
  std::sort(images.begin(), images.end());
  return images;
}

fs::path make_temp_dir(const std::string& prefix) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  std::random_device rd;
  fs::path base = fs::temp_directory_path();
  for (int attempt = 0; attempt < 20; ++attempt) {
    fs::path path = base / (prefix + "-" + std::to_string(stamp) + "-" + std::to_string(rd()));
    std::error_code ec;
    if (fs::create_directories(path, ec)) {
      return path;
    }
  }
  throw std::runtime_error("could not create temporary directory");
}

void copy_file_create_dirs(const fs::path& from, const fs::path& to) {
  fs::create_directories(to.parent_path());
  fs::copy_file(from, to, fs::copy_options::overwrite_existing);
}

class CbzImageReader::Impl {
 public:
  explicit Impl(const fs::path& cbz) {
    if (!mz_zip_reader_init_file(&archive_, cbz.string().c_str(), 0)) {
      throw std::runtime_error(zip_error_message("failed to open archive", archive_));
    }

    const mz_uint file_count = mz_zip_reader_get_num_files(&archive_);
    entries_.reserve(file_count);
    for (mz_uint index = 0; index < file_count; ++index) {
      mz_zip_archive_file_stat stat{};
      if (!mz_zip_reader_file_stat(&archive_, index, &stat)) {
        throw std::runtime_error(zip_error_message("failed to read archive entry", archive_));
      }

      const std::string archive_name = stat.m_filename;
      if (!is_safe_archive_name(archive_name)) {
        throw std::runtime_error("archive contains an unsafe path: " + archive_name);
      }
      if (mz_zip_reader_is_file_a_directory(&archive_, index) || !is_image_path(fs::path(archive_name))) {
        continue;
      }

      ArchiveEntry entry;
      entry.archive_name = archive_name;
      entry.index = index;
      entry.uncompressed_size = static_cast<std::size_t>(stat.m_uncomp_size);
      entries_.push_back(std::move(entry));
    }

    std::sort(entries_.begin(), entries_.end(), [](const ArchiveEntry& left, const ArchiveEntry& right) {
      return left.archive_name < right.archive_name;
    });
  }

  ~Impl() {
    mz_zip_reader_end(&archive_);
  }

  const std::vector<ArchiveEntry>& entries() const {
    return entries_;
  }

  ArchiveImage read_image(const ArchiveEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    ArchiveImage image;
    image.archive_name = entry.archive_name;
    image.bytes.resize(entry.uncompressed_size);
    if (!mz_zip_reader_extract_to_mem(&archive_, static_cast<mz_uint>(entry.index), image.bytes.data(),
                                      image.bytes.size(), 0)) {
      throw std::runtime_error(zip_error_message("failed to read archive entry", archive_));
    }
    return image;
  }

 private:
  mz_zip_archive archive_{};
  std::mutex mutex_;
  std::vector<ArchiveEntry> entries_;
};

CbzImageReader::CbzImageReader(const fs::path& cbz) : impl_(std::make_unique<Impl>(cbz)) {}

CbzImageReader::~CbzImageReader() = default;

const std::vector<ArchiveEntry>& CbzImageReader::entries() const {
  return impl_->entries();
}

ArchiveImage CbzImageReader::read_image(const ArchiveEntry& entry) {
  return impl_->read_image(entry);
}

std::vector<fs::path> extract_cbz(const fs::path& cbz, const fs::path& destination) {
  fs::create_directories(destination);
  std::vector<fs::path> images;

  mz_zip_archive archive{};
  if (!mz_zip_reader_init_file(&archive, cbz.string().c_str(), 0)) {
    throw std::runtime_error(zip_error_message("failed to open archive", archive));
  }

  const mz_uint file_count = mz_zip_reader_get_num_files(&archive);
  for (mz_uint index = 0; index < file_count; ++index) {
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(&archive, index, &stat)) {
      mz_zip_reader_end(&archive);
      throw std::runtime_error(zip_error_message("failed to read archive entry", archive));
    }

    const std::string archive_name = stat.m_filename;
    if (!is_safe_archive_name(archive_name)) {
      mz_zip_reader_end(&archive);
      throw std::runtime_error("archive contains an unsafe path: " + archive_name);
    }
    if (mz_zip_reader_is_file_a_directory(&archive, index)) {
      continue;
    }
    if (!is_image_path(fs::path(archive_name))) {
      continue;
    }

    fs::path output = destination / fs::path(archive_name);
    fs::create_directories(output.parent_path());
    if (!mz_zip_reader_extract_to_file(&archive, index, output.string().c_str(), 0)) {
      mz_zip_reader_end(&archive);
      throw std::runtime_error(zip_error_message("failed to extract archive entry", archive));
    }
    images.push_back(std::move(output));
  }

  mz_zip_reader_end(&archive);
  std::sort(images.begin(), images.end());
  return images;
}

void replace_archive_file(const fs::path& temp_archive, const fs::path& archive_path) {
  std::error_code ignored;
  fs::path backup = archive_path;
  backup += ".trimanga-bak";
  fs::remove(backup, ignored);
  fs::rename(archive_path, backup);
  try {
    fs::rename(temp_archive, archive_path);
  } catch (...) {
    fs::rename(backup, archive_path, ignored);
    fs::remove(temp_archive, ignored);
    throw;
  }
  fs::remove(backup, ignored);
}

fs::path temp_archive_path_for(const fs::path& archive_path) {
  fs::path temp_archive = archive_path;
  temp_archive += ".trimanga-tmp.zip";
  return temp_archive;
}

void remove_archive_entries(const fs::path& archive_path, const std::unordered_set<std::string>& removed_entries) {
  if (removed_entries.empty()) {
    return;
  }

  fs::path temp_archive = temp_archive_path_for(archive_path);
  std::error_code ignored;
  fs::remove(temp_archive, ignored);

  mz_zip_archive source{};
  if (!mz_zip_reader_init_file(&source, archive_path.string().c_str(), 0)) {
    throw std::runtime_error(zip_error_message("failed to open archive", source));
  }

  mz_zip_archive destination{};
  if (!mz_zip_writer_init_file(&destination, temp_archive.string().c_str(), 0)) {
    const std::string message = zip_error_message("failed to create archive", destination);
    mz_zip_reader_end(&source);
    throw std::runtime_error(message);
  }

  const mz_uint file_count = mz_zip_reader_get_num_files(&source);
  for (mz_uint index = 0; index < file_count; ++index) {
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(&source, index, &stat)) {
      const std::string message = zip_error_message("failed to read archive entry", source);
      mz_zip_writer_end(&destination);
      mz_zip_reader_end(&source);
      fs::remove(temp_archive, ignored);
      throw std::runtime_error(message);
    }

    const std::string archive_name = stat.m_filename;
    if (!is_safe_archive_name(archive_name)) {
      mz_zip_writer_end(&destination);
      mz_zip_reader_end(&source);
      fs::remove(temp_archive, ignored);
      throw std::runtime_error("archive contains an unsafe path: " + archive_name);
    }
    if (removed_entries.contains(archive_name)) {
      continue;
    }

    if (!mz_zip_writer_add_from_zip_reader(&destination, &source, index)) {
      const std::string message = zip_error_message("failed to copy archive entry", destination);
      mz_zip_writer_end(&destination);
      mz_zip_reader_end(&source);
      fs::remove(temp_archive, ignored);
      throw std::runtime_error(message);
    }
  }

  if (!mz_zip_writer_finalize_archive(&destination)) {
    const std::string message = zip_error_message("failed to finalize archive", destination);
    mz_zip_writer_end(&destination);
    mz_zip_reader_end(&source);
    fs::remove(temp_archive, ignored);
    throw std::runtime_error(message);
  }
  mz_zip_writer_end(&destination);
  mz_zip_reader_end(&source);
  replace_archive_file(temp_archive, archive_path);
}

void replace_archive_from_directory(const fs::path& source_dir, const fs::path& archive_path) {
  if (!fs::is_directory(source_dir)) {
    throw std::runtime_error("archive rebuild source is not a directory: " + source_dir.string());
  }

  fs::path temp_archive = temp_archive_path_for(archive_path);
  std::error_code ignored;
  fs::remove(temp_archive, ignored);

  mz_zip_archive archive{};
  if (!mz_zip_writer_init_file(&archive, temp_archive.string().c_str(), 0)) {
    throw std::runtime_error(zip_error_message("failed to create archive", archive));
  }

  for (const fs::path& file : archive_files_recursive(source_dir)) {
    const std::string archive_name = fs::relative(file, source_dir).generic_string();
    if (!mz_zip_writer_add_file(&archive, archive_name.c_str(), file.string().c_str(), nullptr, 0, MZ_BEST_SPEED)) {
      const std::string message = zip_error_message("failed to add archive entry", archive);
      mz_zip_writer_end(&archive);
      fs::remove(temp_archive, ignored);
      throw std::runtime_error(message);
    }
  }

  if (!mz_zip_writer_finalize_archive(&archive)) {
    const std::string message = zip_error_message("failed to finalize archive", archive);
    mz_zip_writer_end(&archive);
    fs::remove(temp_archive, ignored);
    throw std::runtime_error(message);
  }
  mz_zip_writer_end(&archive);
  replace_archive_file(temp_archive, archive_path);
}

std::string path_label(const fs::path& path) {
  return path.filename().string();
}

}  // namespace trimanga
