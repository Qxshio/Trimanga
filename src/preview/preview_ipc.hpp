#pragma once

#include "trimanga/types.hpp"

#include <filesystem>
#include <vector>

namespace trimanga::preview_ipc {

std::filesystem::path unique_preview_path(const std::string& suffix);
bool write_preview_input(const std::filesystem::path& path, const std::vector<Candidate>& candidates);
bool read_preview_input(const std::filesystem::path& path, std::vector<Candidate>& candidates);
bool write_preview_output(const std::filesystem::path& path, bool reviewed, const std::vector<Candidate>& candidates);
bool read_preview_output(const std::filesystem::path& path, std::vector<Candidate>& candidates);

}  // namespace trimanga::preview_ipc
