#include "trimanga/ocr.hpp"

#include <stdexcept>

namespace trimanga {

namespace {

class NoOcrBackend final : public IOcrBackend {
 public:
  std::string name() const override { return "None"; }
  bool available() const override { return true; }
  std::string read_text(const std::filesystem::path&) const override { return {}; }
};

}  // namespace

std::unique_ptr<IOcrBackend> make_ocr_backend(OcrPreference preference, OcrSpeed speed) {
  if (preference == OcrPreference::None) {
    return std::make_unique<NoOcrBackend>();
  }

#if defined(TRIMANGA_APPLE)
  if (preference == OcrPreference::AppleVision || preference == OcrPreference::Auto) {
    auto apple = make_apple_vision_backend(speed);
    if (apple && apple->available()) {
      return apple;
    }
    if (preference == OcrPreference::AppleVision) {
      throw std::runtime_error("Apple Vision OCR is not available");
    }
  }
#else
  if (preference == OcrPreference::AppleVision) {
    throw std::runtime_error("Apple Vision OCR is only available on macOS builds");
  }
#endif

  auto tesseract = make_tesseract_backend(speed);
  if (tesseract && tesseract->available()) {
    return tesseract;
  }
  throw std::runtime_error("no OCR backend available; install Tesseract or use macOS Apple Vision");
}

}  // namespace trimanga
