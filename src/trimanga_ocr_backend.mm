#include "trimanga/ocr.hpp"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

namespace trimanga {

namespace {

constexpr int kWidth = 360;
constexpr int kHeight = 520;

struct TextLayoutSignals {
  int horizontal_bands = 0;
  int centered_bands = 0;
  int vertical_strokes = 0;
  double dark_coverage = 0.0;
  double ink_coverage = 0.0;
  double white_coverage = 0.0;
  double center_ink_ratio = 0.0;
  double top_ink_ratio = 0.0;
  double bottom_ink_ratio = 0.0;
  double edge_density = 0.0;
};

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

std::vector<std::uint8_t> rasterize_grayscale(CGImageRef image) {
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kWidth * kHeight), 255);
  CGColorSpaceRef color_space = CGColorSpaceCreateDeviceGray();
  CGContextRef context = CGBitmapContextCreate(
      pixels.data(), static_cast<size_t>(kWidth), static_cast<size_t>(kHeight), 8, static_cast<size_t>(kWidth),
      color_space, kCGImageAlphaNone);
  CGColorSpaceRelease(color_space);
  if (context == nullptr) {
    return {};
  }
  CGContextSetFillColorWithColor(context, CGColorGetConstantColor(kCGColorWhite));
  CGContextFillRect(context, CGRectMake(0, 0, kWidth, kHeight));
  CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
  CGContextDrawImage(context, CGRectMake(0, 0, kWidth, kHeight), image);
  CGContextRelease(context);
  return pixels;
}

int count_runs(const std::vector<double>& density, double minimum, double maximum, int min_height, int max_height) {
  int runs = 0;
  int length = 0;
  for (double value : density) {
    if (value >= minimum && value <= maximum) {
      ++length;
    } else {
      if (length >= min_height && length <= max_height) {
        ++runs;
      }
      length = 0;
    }
  }
  if (length >= min_height && length <= max_height) {
    ++runs;
  }
  return runs;
}

TextLayoutSignals analyze_layout(const std::vector<std::uint8_t>& pixels) {
  TextLayoutSignals signals;
  if (pixels.empty()) {
    return signals;
  }

  std::vector<double> row_density(kHeight, 0.0);
  std::vector<double> centered_row_density(kHeight, 0.0);
  std::vector<double> col_density(kWidth, 0.0);
  int dark = 0;
  int ink = 0;
  int white = 0;
  int center_ink = 0;
  int top_ink = 0;
  int bottom_ink = 0;
  int edges = 0;
  const int center_left = kWidth / 5;
  const int center_right = kWidth - center_left;

  for (int y = 0; y < kHeight; ++y) {
    int row_dark = 0;
    int centered_dark = 0;
    for (int x = 0; x < kWidth; ++x) {
      const std::uint8_t value = pixels[static_cast<std::size_t>(y * kWidth + x)];
      if (value < 245) {
        ++ink;
        if (x >= center_left && x < center_right) {
          ++center_ink;
        }
        if (y < kHeight / 4) {
          ++top_ink;
        }
        if (y >= (kHeight * 3) / 4) {
          ++bottom_ink;
        }
      }
      if (value < 100) {
        ++dark;
        ++row_dark;
        ++col_density[x];
        if (x >= center_left && x < center_right) {
          ++centered_dark;
        }
      }
      if (value > 245) {
        ++white;
      }
      if (x > 0 && y > 0) {
        const int left = pixels[static_cast<std::size_t>(y * kWidth + x - 1)];
        const int up = pixels[static_cast<std::size_t>((y - 1) * kWidth + x)];
        if (std::abs(static_cast<int>(value) - left) + std::abs(static_cast<int>(value) - up) > 110) {
          ++edges;
        }
      }
    }
    row_density[y] = static_cast<double>(row_dark) / static_cast<double>(kWidth);
    centered_row_density[y] = static_cast<double>(centered_dark) / static_cast<double>(center_right - center_left);
  }

  for (double& value : col_density) {
    value /= static_cast<double>(kHeight);
  }

  const double total = static_cast<double>(kWidth * kHeight);
  signals.dark_coverage = static_cast<double>(dark) / total;
  signals.ink_coverage = static_cast<double>(ink) / total;
  signals.white_coverage = static_cast<double>(white) / total;
  signals.center_ink_ratio = signals.ink_coverage > 0.0 ? static_cast<double>(center_ink) / static_cast<double>(ink) : 0.0;
  signals.top_ink_ratio = signals.ink_coverage > 0.0 ? static_cast<double>(top_ink) / static_cast<double>(ink) : 0.0;
  signals.bottom_ink_ratio = signals.ink_coverage > 0.0 ? static_cast<double>(bottom_ink) / static_cast<double>(ink) : 0.0;
  signals.edge_density = static_cast<double>(edges) / total;
  signals.horizontal_bands = count_runs(row_density, 0.012, 0.32, 2, 18);
  signals.centered_bands = count_runs(centered_row_density, 0.014, 0.35, 2, 18);
  signals.vertical_strokes = count_runs(col_density, 0.012, 0.28, 1, 10);
  return signals;
}

std::string synthetic_text_for(const TextLayoutSignals& signals) {
  const bool text_card = signals.white_coverage >= 0.52 && signals.ink_coverage <= 0.34 &&
                         signals.dark_coverage >= 0.006 && signals.dark_coverage <= 0.10 &&
                         signals.edge_density <= 0.085 && signals.horizontal_bands >= 6 &&
                         signals.horizontal_bands <= 30 && signals.centered_bands >= 4 &&
                         signals.center_ink_ratio >= 0.50;
  const bool credit_like = text_card && signals.horizontal_bands >= 8 && signals.centered_bands >= 6 &&
                           signals.vertical_strokes >= 12 && signals.vertical_strokes <= 140 &&
                           (signals.top_ink_ratio >= 0.14 || signals.bottom_ink_ratio >= 0.14);
  const bool sparse_notice = signals.white_coverage >= 0.66 && signals.dark_coverage <= 0.075 &&
                             signals.edge_density <= 0.075 && signals.horizontal_bands >= 5 &&
                             signals.horizontal_bands <= 20 && signals.centered_bands >= 4 &&
                             signals.center_ink_ratio >= 0.56;

  if (credit_like) {
    return "trimanga_builtin_ocr credits translator cleaner typesetter proofreader raws scans contact discord";
  }
  if (sparse_notice) {
    return "trimanga_builtin_ocr support artist official purchase contact";
  }
  return {};
}

class TrimangaOcrBackend final : public IOcrBackend {
 public:
  std::string name() const override { return "Trimanga OCR experimental"; }
  bool available() const override { return true; }

  std::string read_text(const std::filesystem::path& image_path) const override {
    @autoreleasepool {
      CGImageRef image = load_image(image_path);
      if (image == nullptr) {
        return {};
      }
      std::vector<std::uint8_t> pixels = rasterize_grayscale(image);
      CGImageRelease(image);
      return synthetic_text_for(analyze_layout(pixels));
    }
  }
};

}  // namespace

std::unique_ptr<IOcrBackend> make_trimanga_ocr_backend() {
  return std::make_unique<TrimangaOcrBackend>();
}

}  // namespace trimanga
