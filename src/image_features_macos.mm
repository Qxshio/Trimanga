#include "scanlation_tool/image_features.hpp"

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

namespace scanlation {

namespace {

constexpr int kFeatureWidth = 220;
constexpr int kFeatureHeight = 320;

std::vector<std::uint8_t> rasterize_grayscale(CGImageRef image, int width, int height) {
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height), 255);
  CGColorSpaceRef color_space = CGColorSpaceCreateDeviceGray();
  CGContextRef context = CGBitmapContextCreate(
      pixels.data(),
      static_cast<size_t>(width),
      static_cast<size_t>(height),
      8,
      static_cast<size_t>(width),
      color_space,
      kCGImageAlphaNone);
  CGColorSpaceRelease(color_space);
  if (context == nullptr) {
    return {};
  }
  CGContextSetFillColorWithColor(context, CGColorGetConstantColor(kCGColorWhite));
  CGContextFillRect(context, CGRectMake(0, 0, width, height));
  CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
  CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
  CGContextRelease(context);
  return pixels;
}

CGImageRef load_image(const std::filesystem::path& image_path) {
  NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:image_path.string().c_str()]];
  CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)url, nullptr);
  if (source == nullptr) {
    return nullptr;
  }
  CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
  CFRelease(source);
  return image;
}

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

int count_gutter_bands_rows(const std::vector<std::uint8_t>& pixels, int width, int height) {
  int count = 0;
  int run = 0;
  for (int y = 0; y < height; ++y) {
    int white = 0;
    for (int x = 0; x < width; ++x) {
      if (pixels[static_cast<std::size_t>(y * width + x)] > 248) {
        ++white;
      }
    }
    const double fraction = static_cast<double>(white) / static_cast<double>(width);
    if (fraction >= 0.985) {
      ++run;
    } else {
      if (run >= 4) {
        ++count;
      }
      run = 0;
    }
  }
  if (run >= 4) {
    ++count;
  }
  return count;
}

int count_gutter_bands_cols(const std::vector<std::uint8_t>& pixels, int width, int height) {
  int count = 0;
  int run = 0;
  for (int x = 0; x < width; ++x) {
    int white = 0;
    for (int y = 0; y < height; ++y) {
      if (pixels[static_cast<std::size_t>(y * width + x)] > 248) {
        ++white;
      }
    }
    const double fraction = static_cast<double>(white) / static_cast<double>(height);
    if (fraction >= 0.985) {
      ++run;
    } else {
      if (run >= 4) {
        ++count;
      }
      run = 0;
    }
  }
  if (run >= 4) {
    ++count;
  }
  return count;
}

std::uint64_t dhash(CGImageRef image) {
  constexpr int width = kVisualHashSize + 1;
  constexpr int height = kVisualHashSize;
  std::vector<std::uint8_t> pixels = rasterize_grayscale(image, width, height);
  if (pixels.empty()) {
    return 0;
  }
  std::uint64_t value = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < kVisualHashSize; ++x) {
      value <<= 1U;
      if (pixels[static_cast<std::size_t>(y * width + x)] > pixels[static_cast<std::size_t>(y * width + x + 1)]) {
        value |= 1U;
      }
    }
  }
  return value;
}

}  // namespace

PageFeatures extract_page_features(const std::filesystem::path& image_path) {
  @autoreleasepool {
    CGImageRef image = load_image(image_path);
    if (image == nullptr) {
      return {};
    }
    const double source_width = static_cast<double>(CGImageGetWidth(image));
    const double source_height = static_cast<double>(CGImageGetHeight(image));
    std::vector<std::uint8_t> pixels = rasterize_grayscale(image, kFeatureWidth, kFeatureHeight);
    PageFeatures features;
    features.aspect_ratio = source_width / std::max(1.0, source_height);
    features.visual_hash = dhash(image);
    CGImageRelease(image);
    if (pixels.empty()) {
      return features;
    }

    const double total = static_cast<double>(pixels.size());
    std::array<int, 256> histogram{};
    int ink = 0;
    int dark = 0;
    int white = 0;
    int edges = 0;
    for (int y = 0; y < kFeatureHeight; ++y) {
      for (int x = 0; x < kFeatureWidth; ++x) {
        const auto value = pixels[static_cast<std::size_t>(y * kFeatureWidth + x)];
        ++histogram[value];
        if (value < 245) {
          ++ink;
        }
        if (value < 80) {
          ++dark;
        }
        if (value > 245) {
          ++white;
        }
        if (x > 0 && y > 0) {
          const int left = pixels[static_cast<std::size_t>(y * kFeatureWidth + x - 1)];
          const int up = pixels[static_cast<std::size_t>((y - 1) * kFeatureWidth + x)];
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
    const int horizontal = count_gutter_bands_rows(pixels, kFeatureWidth, kFeatureHeight);
    const int vertical = count_gutter_bands_cols(pixels, kFeatureWidth, kFeatureHeight);
    const int panel_count = std::clamp((horizontal + 1) * (vertical + 1), 1, 12);

    features.entropy = entropy;
    features.ink_coverage = static_cast<double>(ink) / total;
    features.dark_coverage = static_cast<double>(dark) / total;
    features.white_coverage = static_cast<double>(white) / total;
    features.edge_density = static_cast<double>(edges) / total;
    features.artwork_coverage = std::max(0.0, features.ink_coverage - features.dark_coverage * 0.25);
    features.panel_count = static_cast<double>(panel_count);
    features.valid = true;
    return features;
  }
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
  return static_cast<double>(__builtin_popcountll(left ^ right));
}

}  // namespace scanlation
