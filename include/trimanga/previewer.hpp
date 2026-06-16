#pragma once

#include "trimanga/types.hpp"

#include <filesystem>
#include <vector>

namespace trimanga {

bool review_candidates(std::vector<Candidate>& candidates);
int run_preview_helper(const std::filesystem::path& input_path, const std::filesystem::path& output_path);

}  // namespace trimanga
