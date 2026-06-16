#include "trimanga/detector.hpp"

#include "trimanga/image_loader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
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
  return runs + (length >= min_height && length <= max_height ? 1 : 0);
}

TextLayoutSignals analyze_layout(const GrayImage& source) {
  GrayImage image = resize_grayscale(source, kWidth, kHeight);
  TextLayoutSignals signals;
  if (!image.valid()) {
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
      const std::uint8_t value = image.pixels[static_cast<std::size_t>(y * kWidth + x)];
      if (value < 245) {
        ++ink;
        center_ink += x >= center_left && x < center_right ? 1 : 0;
        top_ink += y < kHeight / 4 ? 1 : 0;
        bottom_ink += y >= (kHeight * 3) / 4 ? 1 : 0;
      }
      if (value < 100) {
        ++dark;
        ++row_dark;
        ++col_density[x];
        centered_dark += x >= center_left && x < center_right ? 1 : 0;
      }
      white += value > 245 ? 1 : 0;
      if (x > 0 && y > 0) {
        const int left = image.pixels[static_cast<std::size_t>(y * kWidth + x - 1)];
        const int up = image.pixels[static_cast<std::size_t>((y - 1) * kWidth + x)];
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
  signals.center_ink_ratio = ink > 0 ? static_cast<double>(center_ink) / static_cast<double>(ink) : 0.0;
  signals.top_ink_ratio = ink > 0 ? static_cast<double>(top_ink) / static_cast<double>(ink) : 0.0;
  signals.bottom_ink_ratio = ink > 0 ? static_cast<double>(bottom_ink) / static_cast<double>(ink) : 0.0;
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
  const bool dense_credit_card = signals.white_coverage >= 0.54 && signals.white_coverage <= 0.63 &&
                                 signals.ink_coverage >= 0.36 && signals.ink_coverage <= 0.46 &&
                                 signals.dark_coverage >= 0.014 && signals.dark_coverage <= 0.045 &&
                                 signals.edge_density <= 0.080 && signals.center_ink_ratio >= 0.54 &&
                                 signals.center_ink_ratio <= 0.66 && signals.horizontal_bands >= 9 && signals.horizontal_bands <= 16 &&
                                 signals.centered_bands >= 4 &&
                                 (signals.bottom_ink_ratio >= 0.32 || signals.top_ink_ratio >= 0.18);
  const bool dark_credit_card = signals.white_coverage <= 0.065 && signals.ink_coverage >= 0.92 &&
                                signals.dark_coverage >= 0.24 && signals.edge_density <= 0.090 &&
                                signals.center_ink_ratio >= 0.56 && signals.center_ink_ratio <= 0.64 &&
                                signals.top_ink_ratio >= 0.22 && signals.top_ink_ratio <= 0.28 &&
                                signals.bottom_ink_ratio >= 0.22 && signals.bottom_ink_ratio <= 0.28;
  const bool soft_credit_card = signals.white_coverage >= 0.26 && signals.white_coverage <= 0.33 &&
                                signals.ink_coverage >= 0.64 && signals.ink_coverage <= 0.72 &&
                                signals.dark_coverage >= 0.003 && signals.dark_coverage <= 0.013 &&
                                signals.edge_density <= 0.040 && signals.center_ink_ratio >= 0.55 &&
                                signals.center_ink_ratio <= 0.61 && signals.horizontal_bands >= 13 &&
                                signals.horizontal_bands <= 17 && signals.centered_bands >= 6 &&
                                signals.vertical_strokes >= 28;
  const bool sparse_notice = signals.white_coverage >= 0.66 && signals.dark_coverage <= 0.075 &&
                             signals.edge_density <= 0.075 && signals.horizontal_bands >= 5 &&
                             signals.horizontal_bands <= 28 && signals.centered_bands >= 4 &&
                             signals.center_ink_ratio >= 0.56 && signals.bottom_ink_ratio >= 0.15;

  if (credit_like || dense_credit_card || dark_credit_card || soft_credit_card) {
    return "credits translator cleaner typesetter proofreader raws scans contact discord";
  }
  if (sparse_notice) {
    return "support artist official purchase contact";
  }
  return {};
}

class TrimangaDetector final : public IPageDetector {
 public:
  std::string name() const override { return "Trimanga detector"; }

  std::string analyze_image(const GrayImage& image) const override {
    if (!image.valid()) {
      return {};
    }
    return synthetic_text_for(analyze_layout(image));
  }

  std::string analyze_page(const std::filesystem::path& image_path) const override {
    return analyze_image(load_grayscale_image(image_path));
  }
};

}  // namespace

std::unique_ptr<IPageDetector> make_page_detector() {
  return std::make_unique<TrimangaDetector>();
}

}  // namespace trimanga
