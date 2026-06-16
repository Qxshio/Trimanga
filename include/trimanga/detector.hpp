#pragma once

#include "trimanga/image_loader.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace trimanga {

class IPageDetector {
 public:
  virtual ~IPageDetector() = default;
  virtual std::string name() const = 0;
  virtual std::string analyze_image(const GrayImage& image) const = 0;
  virtual std::string analyze_page(const std::filesystem::path& image_path) const = 0;
};

std::unique_ptr<IPageDetector> make_page_detector();

}  // namespace trimanga
