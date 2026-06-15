#pragma once

#include "scanlation_tool/types.hpp"

#include <string>
#include <vector>

namespace scanlation {

Classification classify_page(const std::string& text, const PageFeatures& features, const VolumeProfile& profile);
std::vector<std::string> text_words(const std::string& text);
std::string normalize_text(const std::string& text);

}  // namespace scanlation
