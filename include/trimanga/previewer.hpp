#pragma once

#include "trimanga/types.hpp"

#include <vector>

namespace trimanga {

bool review_candidates(std::vector<Candidate>& candidates);
bool run_sdl_review_window(std::vector<Candidate>& candidates);

}  // namespace trimanga
