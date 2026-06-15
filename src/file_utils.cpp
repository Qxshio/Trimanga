#include "scanlation_tool/file_utils.hpp"

#include "scanlation_tool/process.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace scanlation {

namespace {

std::string lower_ext(const fs::path& path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return ext;
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
  const ProcessResult result = run_process({"unzip", "-qq", "-o", cbz.string(), "-d", destination.string()});
  if (result.exit_code != 0) {
    throw std::runtime_error("failed to extract archive with unzip: " + cbz.string());
  }
  return list_images_recursive(destination);
}

std::string path_label(const fs::path& path) {
  return path.filename().string();
}

}  // namespace scanlation
