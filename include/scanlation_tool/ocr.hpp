#pragma once

#include "scanlation_tool/types.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace scanlation {

class IOcrBackend {
 public:
  virtual ~IOcrBackend() = default;
  virtual std::string name() const = 0;
  virtual bool available() const = 0;
  virtual std::string read_text(const std::filesystem::path& image_path) const = 0;
};

std::unique_ptr<IOcrBackend> make_ocr_backend(OcrPreference preference);
std::unique_ptr<IOcrBackend> make_tesseract_backend();

#if defined(SCANLATION_TOOL_APPLE)
std::unique_ptr<IOcrBackend> make_apple_vision_backend();
#endif

}  // namespace scanlation
