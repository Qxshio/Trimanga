#include "trimanga/image_loader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#if defined(TRIMANGA_WITH_TURBOJPEG)
#include <turbojpeg.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace trimanga {

namespace {

#if defined(TRIMANGA_WITH_TURBOJPEG)
bool looks_like_jpeg(const std::uint8_t* data, std::size_t size) {
  return data != nullptr && size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

struct TurboJpegHandle {
  TurboJpegHandle() : handle(tj3Init(TJINIT_DECOMPRESS)) {
    if (handle != nullptr) {
      tj3Set(handle, TJPARAM_FASTDCT, 1);
      tj3Set(handle, TJPARAM_FASTUPSAMPLE, 1);
    }
  }

  TurboJpegHandle(const TurboJpegHandle&) = delete;
  TurboJpegHandle& operator=(const TurboJpegHandle&) = delete;

  ~TurboJpegHandle() {
    tj3Destroy(handle);
  }

  tjhandle handle = nullptr;
};

GrayImage load_jpeg_grayscale_turbojpeg(const std::uint8_t* data, std::size_t size) {
  if (!looks_like_jpeg(data, size)) {
    return {};
  }
  thread_local TurboJpegHandle turbojpeg;
  tjhandle handle = turbojpeg.handle;
  if (handle == nullptr) {
    return {};
  }
  GrayImage image;
  if (tj3DecompressHeader(handle, data, size) != 0) {
    return {};
  }
  const int width = tj3Get(handle, TJPARAM_JPEGWIDTH);
  const int height = tj3Get(handle, TJPARAM_JPEGHEIGHT);
  if (width <= 0 || height <= 0) {
    return {};
  }
  image.width = width;
  image.height = height;
  image.pixels.resize(static_cast<std::size_t>(width * height));
  if (tj3Decompress8(handle, data, size, image.pixels.data(), 0, TJPF_GRAY) != 0) {
    return {};
  }
  return image;
}
#endif

GrayImage grayscale_from_stbi(unsigned char* data, int width, int height, int channels) {
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

}  // namespace

GrayImage load_grayscale_image(const std::filesystem::path& image_path) {
#if defined(TRIMANGA_WITH_TURBOJPEG)
  {
    FILE* file = std::fopen(image_path.string().c_str(), "rb");
    if (file != nullptr) {
      std::array<std::uint8_t, 3> header{};
      const std::size_t read = std::fread(header.data(), 1, header.size(), file);
      std::fclose(file);
      if (read == header.size() && looks_like_jpeg(header.data(), header.size())) {
        std::vector<std::uint8_t> bytes;
        FILE* jpeg_file = std::fopen(image_path.string().c_str(), "rb");
        if (jpeg_file != nullptr) {
          std::fseek(jpeg_file, 0, SEEK_END);
          const long size = std::ftell(jpeg_file);
          std::rewind(jpeg_file);
          if (size > 0 && size <= std::numeric_limits<int>::max()) {
            bytes.resize(static_cast<std::size_t>(size));
            if (std::fread(bytes.data(), 1, bytes.size(), jpeg_file) == bytes.size()) {
              GrayImage image = load_jpeg_grayscale_turbojpeg(bytes.data(), bytes.size());
              if (image.valid()) {
                std::fclose(jpeg_file);
                return image;
              }
            }
          }
          std::fclose(jpeg_file);
        }
      }
    }
  }
#endif
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* data = stbi_load(image_path.string().c_str(), &width, &height, &channels, 0);
  return grayscale_from_stbi(data, width, height, channels);
}

GrayImage load_grayscale_image(const std::vector<std::uint8_t>& encoded_image) {
  if (encoded_image.empty() || encoded_image.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return {};
  }
#if defined(TRIMANGA_WITH_TURBOJPEG)
  {
    GrayImage image = load_jpeg_grayscale_turbojpeg(encoded_image.data(), encoded_image.size());
    if (image.valid()) {
      return image;
    }
  }
#endif
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* data =
      stbi_load_from_memory(encoded_image.data(), static_cast<int>(encoded_image.size()), &width, &height, &channels, 0);
  return grayscale_from_stbi(data, width, height, channels);
}

ColorImage load_color_image(const std::filesystem::path& image_path) {
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* data = stbi_load(image_path.string().c_str(), &width, &height, &channels, 4);
  if (data == nullptr || width <= 0 || height <= 0) {
    if (data != nullptr) {
      stbi_image_free(data);
    }
    return {};
  }

  ColorImage image;
  image.width = width;
  image.height = height;
  image.pixels.resize(static_cast<std::size_t>(width * height));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int offset = (y * width + x) * 4;
      const std::uint32_t red = data[offset];
      const std::uint32_t green = data[offset + 1];
      const std::uint32_t blue = data[offset + 2];
      const std::uint32_t alpha = data[offset + 3];
      image.pixels[static_cast<std::size_t>(y * width + x)] =
          (alpha << 24U) | (red << 16U) | (green << 8U) | blue;
    }
  }
  stbi_image_free(data);
  return image;
}

GrayImage resize_grayscale(const GrayImage& image, int width, int height) {
  if (!image.valid() || width <= 0 || height <= 0) {
    return {};
  }
  std::vector<int> x_map(static_cast<std::size_t>(width));
  std::vector<int> y_map(static_cast<std::size_t>(height));
  for (int x = 0; x < width; ++x) {
    x_map[static_cast<std::size_t>(x)] =
        std::clamp(static_cast<int>((static_cast<double>(x) / width) * image.width), 0, image.width - 1);
  }
  for (int y = 0; y < height; ++y) {
    y_map[static_cast<std::size_t>(y)] =
        std::clamp(static_cast<int>((static_cast<double>(y) / height) * image.height), 0, image.height - 1);
  }
  GrayImage resized;
  resized.width = width;
  resized.height = height;
  resized.pixels.resize(static_cast<std::size_t>(width * height));
  for (int y = 0; y < height; ++y) {
    const int src_y = y_map[static_cast<std::size_t>(y)];
    const auto src_row = static_cast<std::size_t>(src_y * image.width);
    const auto dst_row = static_cast<std::size_t>(y * width);
    for (int x = 0; x < width; ++x) {
      resized.pixels[dst_row + static_cast<std::size_t>(x)] =
          image.pixels[src_row + static_cast<std::size_t>(x_map[static_cast<std::size_t>(x)])];
    }
  }
  return resized;
}

}  // namespace trimanga
