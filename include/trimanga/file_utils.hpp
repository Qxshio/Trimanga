#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace trimanga {

bool is_image_path(const std::filesystem::path& path);
bool is_cbz_path(const std::filesystem::path& path);
std::vector<std::filesystem::path> list_images_recursive(const std::filesystem::path& root);
std::filesystem::path make_temp_dir(const std::string& prefix);
void copy_file_create_dirs(const std::filesystem::path& from, const std::filesystem::path& to);
std::vector<std::filesystem::path> extract_cbz(const std::filesystem::path& cbz, const std::filesystem::path& destination);
std::string path_label(const std::filesystem::path& path);

}  // namespace trimanga
