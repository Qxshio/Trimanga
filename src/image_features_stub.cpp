#include "trimanga/image_features.hpp"

namespace trimanga {

PageFeatures extract_page_features(const std::filesystem::path&) {
  return {};
}

VolumeProfile build_volume_profile(std::vector<PageRef>& pages) {
  for (PageRef& page : pages) {
    page.features = {};
  }
  return {};
}

double visual_hash_distance(std::uint64_t left, std::uint64_t right) {
  std::uint64_t value = left ^ right;
  int count = 0;
  while (value != 0) {
    value &= value - 1;
    ++count;
  }
  return static_cast<double>(count);
}

}  // namespace trimanga
