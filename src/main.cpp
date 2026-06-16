#include "trimanga/scanner.hpp"
#include "trimanga/previewer.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

void print_help() {
  std::cout
      << "Trimanga 0.1.0\n"
      << "Clean manga archives by finding likely scanlation credits, ad pages, and release clutter.\n\n"
      << "Usage:\n"
      << "  trimanga scan <folder-or-cbz> [options]\n\n"
      << "Options:\n"
      << "  --speed <eco|balanced|fast|fastest>\n"
      << "                                  Scan speed. Default: balanced\n"
      << "  --format <table|json>          Output format. Default: table\n"
      << "  --review-dir <path>            Copy suspicious pages into this folder\n"
      << "  --preview                      Open an image review window for Keep/Delete decisions\n"
      << "  --details                      Include detector signals in table output\n"
      << "  --timings                      Print phase timings after the scan\n"
      << "  --progress                     Show live progress while scanning\n"
      << "  --verbose                      Show setup/status messages while scanning\n"
      << "  --quiet                        Only print final results. Default\n"
      << "  --keep-temp                    Keep extracted archive temp files\n"
      << "  --help                         Show this help\n"
      << "  --version                      Show version\n\n"
      << "Examples:\n"
      << "  trimanga scan ~/Documents/Mangas/Volume.cbz --review-dir /tmp/review\n"
      << "  trimanga scan ./MangaFolder --preview\n"
      << "  trimanga scan ./MangaFolder --speed fastest --format json\n";
}

trimanga::OutputFormat parse_format(const std::string& value) {
  if (value == "table") {
    return trimanga::OutputFormat::Table;
  }
  if (value == "json") {
    return trimanga::OutputFormat::Json;
  }
  throw std::runtime_error("invalid output format: " + value);
}

trimanga::ScanSpeed parse_speed(const std::string& value) {
  if (value == "eco" || value == "quiet" || value == "cool") {
    return trimanga::ScanSpeed::Eco;
  }
  if (value == "balanced" || value == "normal") {
    return trimanga::ScanSpeed::Balanced;
  }
  if (value == "fast") {
    return trimanga::ScanSpeed::Fast;
  }
  if (value == "fastest" || value == "max" || value == "ludicrous") {
    return trimanga::ScanSpeed::Fastest;
  }
  throw std::runtime_error("invalid speed: " + value);
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
      std::cout << "Trimanga 0.1.0\n";
      return 0;
    }
    if (std::string(argv[1]) == "--preview-helper") {
      if (argc != 4) {
        return 2;
      }
      return trimanga::run_preview_helper(fs::path(argv[2]), fs::path(argv[3]));
    }
    if (std::string(argv[1]) != "scan") {
      throw std::runtime_error("unknown command: " + std::string(argv[1]));
    }
    if (argc <= 2) {
      throw std::runtime_error("scan requires a folder, image, .cbz, or .zip input");
    }

    fs::path input = argv[2];
    trimanga::ScanOptions options;
    for (int index = 3; index < argc; ++index) {
      const std::string arg = argv[index];
      if (arg == "--help" || arg == "-h") {
        print_help();
        return 0;
      }
      if (arg == "--speed") {
        options.speed = parse_speed(require_value(index, argc, argv, arg));
      } else if (arg == "--format") {
        options.format = parse_format(require_value(index, argc, argv, arg));
      } else if (arg == "--review-dir") {
        options.review_dir = fs::path(require_value(index, argc, argv, arg));
      } else if (arg == "--preview") {
        options.preview = true;
      } else if (arg == "--details") {
        options.details = true;
      } else if (arg == "--timings") {
        options.timings = true;
      } else if (arg == "--progress") {
        options.progress = true;
      } else if (arg == "--verbose") {
        options.verbose = true;
        options.progress = true;
      } else if (arg == "--quiet") {
        options.verbose = false;
        options.progress = false;
      } else if (arg == "--keep-temp") {
        options.keep_temp = true;
      } else {
        throw std::runtime_error("unknown option: " + arg);
      }
    }

    trimanga::ScanResult result = scan(input, options);
    if (options.format == trimanga::OutputFormat::Json) {
      trimanga::print_result_json(result);
    } else {
      trimanga::print_result_table(result);
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\n\n";
    print_help();
    return 1;
  }
}
