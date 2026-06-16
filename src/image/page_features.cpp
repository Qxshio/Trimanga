#include "trimanga/image_features.hpp"

#include "trimanga/image_loader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

namespace trimanga {

namespace {

constexpr int kFeatureWidth = 220;
constexpr int kFeatureHeight = 320;

double median(std::vector<double> values) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const std::size_t mid = values.size() / 2;
  if (values.size() % 2 == 1) {
    return values[mid];
  }
  return (values[mid - 1] + values[mid]) / 2.0;
}

FeatureStats stats_for(std::vector<double> values) {
  FeatureStats stats;
  if (values.empty()) {
    return stats;
  }
  stats.mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
  stats.median = median(values);
  double variance = 0.0;
  for (double value : values) {
    const double diff = value - stats.mean;
    variance += diff * diff;
  }
  stats.stdev = std::sqrt(variance / static_cast<double>(values.size()));
  if (stats.stdev <= 0.0) {
    stats.stdev = 1e-9;
  }
  return stats;
}

int count_gutter_bands_rows(const GrayImage& image) {
  int count = 0;
  int run = 0;
  for (int y = 0; y < image.height; ++y) {
    int white = 0;
    for (int x = 0; x < image.width; ++x) {
      if (image.pixels[static_cast<std::size_t>(y * image.width + x)] > 248) {
        ++white;
      }
    }
    const double fraction = static_cast<double>(white) / static_cast<double>(image.width);
    if (fraction >= 0.985) {
      ++run;
    } else {
      if (run >= 4) {
        ++count;
      }
      run = 0;
    }
  }
  return count + (run >= 4 ? 1 : 0);
}

int count_gutter_bands_cols(const GrayImage& image) {
  int count = 0;
  int run = 0;
  for (int x = 0; x < image.width; ++x) {
    int white = 0;
    for (int y = 0; y < image.height; ++y) {
      if (image.pixels[static_cast<std::size_t>(y * image.width + x)] > 248) {
        ++white;
      }
    }
    const double fraction = static_cast<double>(white) / static_cast<double>(image.height);
    if (fraction >= 0.985) {
      ++run;
    } else {
      if (run >= 4) {
        ++count;
      }
      run = 0;
    }
  }
  return count + (run >= 4 ? 1 : 0);
}

std::uint64_t dhash(const GrayImage& source) {
  GrayImage image = resize_grayscale(source, kVisualHashSize + 1, kVisualHashSize);
  if (!image.valid()) {
    return 0;
  }
  std::uint64_t value = 0;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < kVisualHashSize; ++x) {
      value <<= 1U;
      if (image.pixels[static_cast<std::size_t>(y * image.width + x)] >
          image.pixels[static_cast<std::size_t>(y * image.width + x + 1)]) {
        value |= 1U;
      }
    }
  }
  return value;
}

int popcount64(std::uint64_t value) {
  int count = 0;
  while (value != 0) {
    value &= value - 1;
    ++count;
  }
  return count;
}

}  // namespace

PageFeatures extract_page_features(const GrayImage& original) {
  if (!original.valid()) {
    return {};
  }
  GrayImage image = resize_grayscale(original, kFeatureWidth, kFeatureHeight);
  PageFeatures features;
  features.aspect_ratio = static_cast<double>(original.width) / std::max(1, original.height);
  features.visual_hash = dhash(original);
  if (!image.valid()) {
    return features;
  }

  const double total = static_cast<double>(image.pixels.size());
  std::array<int, 256> histogram{};
  int ink = 0;
  int dark = 0;
  int white = 0;
  int edges = 0;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const auto value = image.pixels[static_cast<std::size_t>(y * image.width + x)];
      ++histogram[value];
      ink += value < 245 ? 1 : 0;
      dark += value < 80 ? 1 : 0;
      white += value > 245 ? 1 : 0;
      if (x > 0 && y > 0) {
        const int left = image.pixels[static_cast<std::size_t>(y * image.width + x - 1)];
        const int up = image.pixels[static_cast<std::size_t>((y - 1) * image.width + x)];
        if (std::abs(static_cast<int>(value) - left) + std::abs(static_cast<int>(value) - up) > 90) {
          ++edges;
        }
      }
    }
  }

  double entropy = 0.0;
  for (int count : histogram) {
    if (count == 0) {
      continue;
    }
    const double p = static_cast<double>(count) / total;
    entropy -= p * std::log2(p);
  }
  const int horizontal = count_gutter_bands_rows(image);
  const int vertical = count_gutter_bands_cols(image);
  features.entropy = entropy;
  features.ink_coverage = static_cast<double>(ink) / total;
  features.dark_coverage = static_cast<double>(dark) / total;
  features.white_coverage = static_cast<double>(white) / total;
  features.edge_density = static_cast<double>(edges) / total;
  features.artwork_coverage = std::max(0.0, features.ink_coverage - features.dark_coverage * 0.25);
  features.panel_count = static_cast<double>(std::clamp((horizontal + 1) * (vertical + 1), 1, 12));
  features.valid = true;
  return features;
}

PageFeatures extract_page_features(const std::filesystem::path& image_path) {
  return extract_page_features(load_grayscale_image(image_path));
}

VolumeProfile build_volume_profile(std::vector<PageRef>& pages) {
  std::vector<double> aspect;
  std::vector<double> edge;
  std::vector<double> entropy;
  std::vector<double> ink;
  std::vector<double> dark;
  std::vector<double> white;
  std::vector<double> art;
  std::vector<double> panels;
  for (PageRef& page : pages) {
    page.features = extract_page_features(page.image_path);
    if (!page.features.valid) {
      continue;
    }
    aspect.push_back(page.features.aspect_ratio);
    edge.push_back(page.features.edge_density);
    entropy.push_back(page.features.entropy);
    ink.push_back(page.features.ink_coverage);
    dark.push_back(page.features.dark_coverage);
    white.push_back(page.features.white_coverage);
    art.push_back(page.features.artwork_coverage);
    panels.push_back(page.features.panel_count);
  }
  VolumeProfile profile;
  profile.aspect_ratio = stats_for(aspect);
  profile.edge_density = stats_for(edge);
  profile.entropy = stats_for(entropy);
  profile.ink_coverage = stats_for(ink);
  profile.dark_coverage = stats_for(dark);
  profile.white_coverage = stats_for(white);
  profile.artwork_coverage = stats_for(art);
  profile.panel_count = stats_for(panels);
  profile.valid = !aspect.empty();
  return profile;
}

double visual_hash_distance(std::uint64_t left, std::uint64_t right) {
  return static_cast<double>(popcount64(left ^ right));
}

}  // namespace trimanga
