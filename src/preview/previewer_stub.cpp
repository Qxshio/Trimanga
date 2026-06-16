#include "trimanga/previewer.hpp"

namespace trimanga {

bool review_candidates(std::vector<Candidate>&) {
  return false;
}

int run_preview_helper(const std::filesystem::path&, const std::filesystem::path&) {
  return 2;
}

}  // namespace trimanga
