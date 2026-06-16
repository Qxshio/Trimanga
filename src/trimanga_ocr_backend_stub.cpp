#include "trimanga/ocr.hpp"

#include <memory>

namespace trimanga {

namespace {

class TrimangaOcrUnavailable final : public IOcrBackend {
 public:
  std::string name() const override { return "Trimanga OCR experimental"; }
  bool available() const override { return false; }
  std::string read_text(const std::filesystem::path&) const override { return {}; }
};

}  // namespace

std::unique_ptr<IOcrBackend> make_trimanga_ocr_backend() {
  return std::make_unique<TrimangaOcrUnavailable>();
}

}  // namespace trimanga
