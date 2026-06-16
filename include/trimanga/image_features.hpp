#pragma once

#include "trimanga/image_loader.hpp"
#include "trimanga/types.hpp"

#include <filesystem>

namespace trimanga {

PageFeatures extract_page_features(const GrayImage& original);
PageFeatures extract_page_features(const std::filesystem::path& image_path);
VolumeProfile build_volume_profile(std::vector<PageRef>& pages);
double visual_hash_distance(std::uint64_t left, std::uint64_t right);

}  // namespace trimanga
