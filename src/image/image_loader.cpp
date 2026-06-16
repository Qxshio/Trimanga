#include "trimanga/image_loader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cmath>

namespace trimanga {

GrayImage load_grayscale_image(const std::filesystem::path& image_path) {
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* data = stbi_load(image_path.string().c_str(), &width, &height, &channels, 0);
  if (data == nullptr || width <= 0 || height <= 0 || channels <= 0) {
    if (data != nullptr) {
      stbi_image_free(data);
    }
    return {};
  }

  GrayImage image;
  image.width = width;
  image.height = height;
  image.pixels.resize(static_cast<std::size_t>(width * height));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int offset = (y * width + x) * channels;
      const int red = data[offset];
      const int green = channels > 1 ? data[offset + 1] : red;
      const int blue = channels > 2 ? data[offset + 2] : red;
      image.pixels[static_cast<std::size_t>(y * width + x)] =
          static_cast<std::uint8_t>(std::clamp((red * 30 + green * 59 + blue * 11) / 100, 0, 255));
    }
  }
  stbi_image_free(data);
  return image;
}

GrayImage resize_grayscale(const GrayImage& image, int width, int height) {
  if (!image.valid() || width <= 0 || height <= 0) {
    return {};
  }
  GrayImage resized;
  resized.width = width;
  resized.height = height;
  resized.pixels.resize(static_cast<std::size_t>(width * height));
  for (int y = 0; y < height; ++y) {
    const int src_y = std::clamp(static_cast<int>((static_cast<double>(y) / height) * image.height), 0, image.height - 1);
    for (int x = 0; x < width; ++x) {
      const int src_x = std::clamp(static_cast<int>((static_cast<double>(x) / width) * image.width), 0, image.width - 1);
      resized.pixels[static_cast<std::size_t>(y * width + x)] =
          image.pixels[static_cast<std::size_t>(src_y * image.width + src_x)];
    }
  }
  return resized;
}

}  // namespace trimanga
