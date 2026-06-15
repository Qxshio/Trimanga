#include "scanlation_tool/scanner.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

void print_help() {
  std::cout
      << "scanlation_tool 0.1.0\n\n"
      << "Usage:\n"
      << "  scanlation_tool scan <folder-or-cbz> [options]\n\n"
      << "Options:\n"
      << "  --ocr <auto|apple|tesseract>   OCR backend to use. Default: auto\n"
      << "  --workers <n>                  Number of OCR workers. Default: 4\n"
      << "  --format <table|json>          Output format. Default: table\n"
      << "  --review-dir <path>            Copy suspicious pages into this folder\n"
      << "  --keep-temp                    Keep extracted archive temp files\n"
      << "  --help                         Show this help\n"
      << "  --version                      Show version\n\n"
      << "Examples:\n"
      << "  scanlation_tool scan ~/Documents/Mangas/Volume.cbz --review-dir /tmp/review\n"
      << "  scanlation_tool scan ./MangaFolder --ocr apple --workers 2 --format json\n";
}

scanlation::OcrPreference parse_ocr(const std::string& value) {
  if (value == "auto") {
    return scanlation::OcrPreference::Auto;
  }
  if (value == "apple" || value == "apple-vision" || value == "vision") {
    return scanlation::OcrPreference::AppleVision;
  }
  if (value == "tesseract") {
    return scanlation::OcrPreference::Tesseract;
  }
  throw std::runtime_error("invalid OCR backend: " + value);
}

scanlation::OutputFormat parse_format(const std::string& value) {
  if (value == "table") {
    return scanlation::OutputFormat::Table;
  }
  if (value == "json") {
    return scanlation::OutputFormat::Json;
  }
  throw std::runtime_error("invalid output format: " + value);
}

std::string require_value(int& index, int argc, char** argv, const std::string& option) {
  if (index + 1 >= argc) {
    throw std::runtime_error(option + " requires a value");
  }
  ++index;
  return argv[index];
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc <= 1 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
      print_help();
      return 0;
    }
    if (std::string(argv[1]) == "--version") {
      std::cout << "scanlation_tool 0.1.0\n";
      return 0;
    }
    if (std::string(argv[1]) != "scan") {
      throw std::runtime_error("unknown command: " + std::string(argv[1]));
    }
    if (argc <= 2) {
      throw std::runtime_error("scan requires a folder, image, .cbz, or .zip input");
    }

    fs::path input = argv[2];
    scanlation::ScanOptions options;
    for (int index = 3; index < argc; ++index) {
      const std::string arg = argv[index];
      if (arg == "--help" || arg == "-h") {
        print_help();
        return 0;
      }
      if (arg == "--ocr") {
        options.ocr = parse_ocr(require_value(index, argc, argv, arg));
      } else if (arg == "--workers") {
        options.workers = std::max(1, std::stoi(require_value(index, argc, argv, arg)));
      } else if (arg == "--format") {
        options.format = parse_format(require_value(index, argc, argv, arg));
      } else if (arg == "--review-dir") {
        options.review_dir = fs::path(require_value(index, argc, argv, arg));
      } else if (arg == "--keep-temp") {
        options.keep_temp = true;
      } else {
        throw std::runtime_error("unknown option: " + arg);
      }
    }

    scanlation::ScanResult result = scan(input, options);
    if (options.format == scanlation::OutputFormat::Json) {
      scanlation::print_result_json(result);
    } else {
      scanlation::print_result_table(result);
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\n\n";
    print_help();
    return 1;
  }
}
