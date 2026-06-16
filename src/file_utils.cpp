#include "trimanga/file_utils.hpp"

#include "trimanga/process.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <random>
#include <sstream>
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

#if defined(_WIN32)
std::string powershell_quote(const std::string& value) {
  std::string quoted = "'";
  for (char ch : value) {
    if (ch == '\'') {
      quoted += "''";
    } else {
      quoted += ch;
    }
  }
  quoted += "'";
  return quoted;
}
#endif

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
  ProcessResult result;
#if defined(_WIN32)
  fs::path zip_copy = destination / "_archive.zip";
  fs::copy_file(cbz, zip_copy, fs::copy_options::overwrite_existing);
  result = run_process({"powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
                        "Expand-Archive -LiteralPath " + powershell_quote(zip_copy.string()) +
                            " -DestinationPath " + powershell_quote(destination.string()) + " -Force"});
  std::error_code ignored;
  fs::remove(zip_copy, ignored);
#else
  if (command_exists("unzip")) {
    result = run_process({"unzip", "-qq", "-o", cbz.string(), "-d", destination.string()});
  } else if (command_exists("7z")) {
    result = run_process({"7z", "x", "-y", "-o" + destination.string(), cbz.string()});
  } else {
    throw std::runtime_error("archive extraction requires unzip or 7z");
  }
#endif
  if (result.exit_code != 0) {
    throw std::runtime_error("failed to extract archive: " + cbz.string());
  }
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

  ProcessResult result;
#if defined(_WIN32)
  result = run_process({"powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
                        "Compress-Archive -Path " + powershell_quote((source_dir / "*").string()) +
                            " -DestinationPath " + powershell_quote(temp_archive.string()) + " -Force"});
#else
  if (command_exists("zip")) {
    std::ostringstream command;
    command << "cd " << shell_quote(source_dir.string()) << " && zip -qr " << shell_quote(temp_archive.string()) << " .";
    result = run_process({"sh", "-c", command.str()});
  } else if (command_exists("7z")) {
    std::ostringstream command;
    command << "cd " << shell_quote(source_dir.string()) << " && 7z a -tzip -bd -y "
            << shell_quote(temp_archive.string()) << " .";
    result = run_process({"sh", "-c", command.str()});
  } else {
    throw std::runtime_error("archive rebuild requires zip or 7z");
  }
#endif

  if (result.exit_code != 0 || !fs::exists(temp_archive)) {
    fs::remove(temp_archive, ignored);
    throw std::runtime_error("failed to rebuild archive: " + archive_path.string());
  }

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
