#include "trimanga/file_utils.hpp"

#include "miniz.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

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

std::vector<fs::path> extract_cbz(const fs::path& cbz, const fs::path& destination) {
  fs::create_directories(destination);

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

    fs::path output = destination / fs::path(archive_name);
    if (mz_zip_reader_is_file_a_directory(&archive, index)) {
      fs::create_directories(output);
      continue;
    }

    fs::create_directories(output.parent_path());
    if (!mz_zip_reader_extract_to_file(&archive, index, output.string().c_str(), 0)) {
      mz_zip_reader_end(&archive);
      throw std::runtime_error(zip_error_message("failed to extract archive entry", archive));
    }
  }

  mz_zip_reader_end(&archive);
  return list_images_recursive(destination);
}

void replace_archive_from_directory(const fs::path& source_dir, const fs::path& archive_path) {
  if (!fs::is_directory(source_dir)) {
    throw std::runtime_error("archive rebuild source is not a directory: " + source_dir.string());
  }

  fs::path temp_archive = archive_path;
  temp_archive += ".trimanga-tmp.zip";
  std::error_code ignored;
  fs::remove(temp_archive, ignored);

  mz_zip_archive archive{};
  if (!mz_zip_writer_init_file(&archive, temp_archive.string().c_str(), 0)) {
    throw std::runtime_error(zip_error_message("failed to create archive", archive));
  }

  for (const fs::path& file : archive_files_recursive(source_dir)) {
    const std::string archive_name = fs::relative(file, source_dir).generic_string();
    if (!mz_zip_writer_add_file(&archive, archive_name.c_str(), file.string().c_str(), nullptr, 0,
                                MZ_BEST_SPEED)) {
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

std::string path_label(const fs::path& path) {
  return path.filename().string();
}

}  // namespace trimanga
