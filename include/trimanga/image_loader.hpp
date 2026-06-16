#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace trimanga {

struct GrayImage {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> pixels;

  bool valid() const { return width > 0 && height > 0 && pixels.size() == static_cast<std::size_t>(width * height); }
};

struct ColorImage {
  int width = 0;
  int height = 0;
  std::vector<std::uint32_t> pixels;

  bool valid() const { return width > 0 && height > 0 && pixels.size() == static_cast<std::size_t>(width * height); }
};

GrayImage load_grayscale_image(const std::filesystem::path& image_path);
ColorImage load_color_image(const std::filesystem::path& image_path);
GrayImage resize_grayscale(const GrayImage& image, int width, int height);

}  // namespace trimanga
