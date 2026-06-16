#pragma once

#include "trimanga/types.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace trimanga {

class IOcrBackend {
 public:
  virtual ~IOcrBackend() = default;
  virtual std::string name() const = 0;
  virtual bool available() const = 0;
  virtual std::string read_text(const std::filesystem::path& image_path) const = 0;
};

std::unique_ptr<IOcrBackend> make_ocr_backend(OcrPreference preference, OcrSpeed speed);
std::unique_ptr<IOcrBackend> make_trimanga_ocr_backend();
std::unique_ptr<IOcrBackend> make_tesseract_backend(OcrSpeed speed);

#if defined(TRIMANGA_APPLE)
std::unique_ptr<IOcrBackend> make_apple_vision_backend(OcrSpeed speed);
#endif

}  // namespace trimanga
