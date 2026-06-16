#include "trimanga/image_loader.hpp"

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kWidth = 360;
constexpr int kHeight = 520;

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

void probe(const std::filesystem::path& path) {
  trimanga::GrayImage source = trimanga::load_grayscale_image(path);
  trimanga::GrayImage image = trimanga::resize_grayscale(source, kWidth, kHeight);
  if (!image.valid()) {
    return;
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
  std::cout << path.filename().string()
            << " white=" << std::fixed << std::setprecision(3) << static_cast<double>(white) / total
            << " ink=" << static_cast<double>(ink) / total
            << " dark=" << static_cast<double>(dark) / total
            << " edge=" << static_cast<double>(edges) / total
            << " center=" << (ink > 0 ? static_cast<double>(center_ink) / ink : 0.0)
            << " top=" << (ink > 0 ? static_cast<double>(top_ink) / ink : 0.0)
            << " bottom=" << (ink > 0 ? static_cast<double>(bottom_ink) / ink : 0.0)
            << " hb=" << count_runs(row_density, 0.012, 0.32, 2, 18)
            << " cb=" << count_runs(centered_row_density, 0.014, 0.35, 2, 18)
            << " vs=" << count_runs(col_density, 0.012, 0.28, 1, 10) << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  for (int index = 1; index < argc; ++index) {
    probe(argv[index]);
  }
}
