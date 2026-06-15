#include "scanlation_tool/ocr.hpp"

#include "scanlation_tool/process.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <set>
#include <sstream>
#include <string>

namespace scanlation {

namespace {

std::string squash_ws(std::string value) {
  std::string out;
  bool space = false;
  for (char ch : value) {
    const bool is_space = std::isspace(static_cast<unsigned char>(ch));
    if (is_space) {
      if (!space && !out.empty()) {
        out += ' ';
      }
      space = true;
    } else {
      out += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      space = false;
    }
  }
  return out;
}

class TesseractBackend final : public IOcrBackend {
 public:
  std::string name() const override { return "Tesseract"; }

  bool available() const override {
    return command_exists("tesseract") || std::filesystem::exists("/opt/homebrew/bin/tesseract") ||
           std::filesystem::exists("/usr/local/bin/tesseract");
  }

  std::string read_text(const std::filesystem::path& image_path) const override {
    std::string binary = "tesseract";
    if (!command_exists(binary)) {
      if (std::filesystem::exists("/opt/homebrew/bin/tesseract")) {
        binary = "/opt/homebrew/bin/tesseract";
      } else if (std::filesystem::exists("/usr/local/bin/tesseract")) {
        binary = "/usr/local/bin/tesseract";
      }
    }

    std::set<std::string> seen;
    std::string combined;
    for (const std::string& psm : {"6", "11", "12"}) {
      ProcessResult result = run_process({binary, image_path.string(), "stdout", "--psm", psm}, 30);
      if (result.exit_code != 0) {
        continue;
      }
      std::string text = squash_ws(result.stdout_text);
      if (!text.empty() && !seen.contains(text)) {
        seen.insert(text);
        if (!combined.empty()) {
          combined += ' ';
        }
        combined += text;
      }
    }
    return combined;
  }
};

}  // namespace

std::unique_ptr<IOcrBackend> make_tesseract_backend() {
  return std::make_unique<TesseractBackend>();
}

}  // namespace scanlation
