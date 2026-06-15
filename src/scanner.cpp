#include "trimanga/scanner.hpp"

#include "trimanga/classifier.hpp"
#include "trimanga/file_utils.hpp"
#include "trimanga/image_features.hpp"
#include "trimanga/ocr.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace trimanga {

namespace {

class TempDirectory {
 public:
  TempDirectory() = default;
  explicit TempDirectory(fs::path path) : path_(std::move(path)) {}
  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;
  TempDirectory(TempDirectory&&) = default;
  TempDirectory& operator=(TempDirectory&&) = default;

  ~TempDirectory() {
    if (!keep_ && !path_.empty()) {
      std::error_code ignored;
      fs::remove_all(path_, ignored);
    }
  }

  const fs::path& path() const { return path_; }
  void keep() { keep_ = true; }

 private:
  fs::path path_;
  bool keep_ = false;
};

std::string escape_json(const std::string& value) {
  std::ostringstream out;
  for (char ch : value) {
    switch (ch) {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch);
        } else {
          out << ch;
        }
    }
  }
  return out.str();
}

std::string excerpt(const std::string& text, std::size_t limit = 180) {
  std::string normalized = normalize_text(text);
  if (normalized.size() <= limit) {
    return normalized;
  }
  return normalized.substr(0, limit - 3) + "...";
}

std::string relative_or_filename(const fs::path& path, const fs::path& root) {
  std::error_code ec;
  fs::path relative = fs::relative(path, root, ec);
  if (!ec && !relative.empty() && relative.native().find("..") != 0) {
    return relative.generic_string();
  }
  return path.filename().string();
}

std::vector<PageRef> build_page_refs(const fs::path& input, TempDirectory& temp) {
  std::vector<fs::path> images;
  fs::path label_root;

  if (fs::is_regular_file(input) && is_cbz_path(input)) {
    temp = TempDirectory(make_temp_dir("trimanga"));
    images = extract_cbz(input, temp.path());
    label_root = temp.path();
  } else if (fs::is_directory(input)) {
    images = list_images_recursive(input);
    label_root = input;
  } else if (fs::is_regular_file(input) && is_image_path(input)) {
    images = {input};
    label_root = input.parent_path();
  } else {
    throw std::runtime_error("input must be a manga folder, image file, .cbz, or .zip");
  }

  std::sort(images.begin(), images.end());
  std::vector<PageRef> pages;
  pages.reserve(images.size());
  for (std::size_t index = 0; index < images.size(); ++index) {
    PageRef page;
    page.order = index + 1;
    page.image_path = images[index];
    page.archive_name = relative_or_filename(images[index], label_root);
    page.label = page.archive_name;
    pages.push_back(std::move(page));
  }
  return pages;
}

std::string candidate_key(const Candidate& candidate) {
  return candidate.page.image_path.string();
}

void add_visual_matches(std::vector<Candidate>& candidates, const std::vector<PageRef>& pages) {
  std::vector<std::uint64_t> seed_hashes;
  std::unordered_set<std::string> already_flagged;
  for (const Candidate& candidate : candidates) {
    already_flagged.insert(candidate_key(candidate));
    if (candidate.classification.visual_seed && candidate.page.features.visual_hash.has_value()) {
      seed_hashes.push_back(*candidate.page.features.visual_hash);
    }
  }
  if (seed_hashes.empty()) {
    return;
  }

  for (const PageRef& page : pages) {
    if (!page.features.visual_hash.has_value() || already_flagged.contains(page.image_path.string())) {
      continue;
    }
    int best = 999;
    for (std::uint64_t seed : seed_hashes) {
      best = std::min(best, static_cast<int>(visual_hash_distance(*page.features.visual_hash, seed)));
    }
    if (best <= kVisualMatchDistance) {
      Candidate candidate;
      candidate.page = page;
      candidate.visual_match = true;
      candidate.visual_distance = best;
      candidate.classification.candidate = true;
      candidate.classification.visual_seed = false;
      candidate.classification.score = 7.0;
      candidates.push_back(std::move(candidate));
    }
  }
}

void write_review_folder(const std::vector<Candidate>& candidates, const fs::path& review_dir) {
  std::error_code ignored;
  fs::create_directories(review_dir, ignored);
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const Candidate& candidate = candidates[index];
    std::ostringstream name;
    name << std::setw(4) << std::setfill('0') << (index + 1) << "-"
         << candidate.page.image_path.filename().string();
    copy_file_create_dirs(candidate.page.image_path, review_dir / name.str());
  }
}

std::string bar(double fraction, int width = 18) {
  fraction = std::clamp(fraction, 0.0, 1.0);
  const int filled = static_cast<int>(fraction * width + 0.5);
  std::string value = "[";
  value.append(static_cast<std::size_t>(filled), '#');
  value.append(static_cast<std::size_t>(width - filled), '-');
  value += "]";
  return value;
}

}  // namespace

ScanResult scan(const fs::path& input, const ScanOptions& options) {
  if (options.workers < 1) {
    throw std::runtime_error("workers must be at least 1");
  }
  if (!fs::exists(input)) {
    throw std::runtime_error("input does not exist: " + input.string());
  }

  TempDirectory temp;
  std::vector<PageRef> pages = build_page_refs(input, temp);
  if (options.keep_temp) {
    temp.keep();
  }
  if (pages.empty()) {
    ScanResult empty;
    empty.input = input;
    empty.detector_version = kDetectorVersion;
    empty.ocr_backend = "none";
    return empty;
  }

  VolumeProfile profile = build_volume_profile(pages);
  auto backend = make_ocr_backend(options.ocr);

  ScanResult result;
  result.input = input;
  result.detector_version = kDetectorVersion;
  result.ocr_backend = backend->name();
  result.scanned_pages = pages.size();

  std::atomic_size_t next{0};
  std::atomic_size_t done{0};
  std::mutex candidates_mutex;
  std::mutex progress_mutex;
  const int workers = std::min<int>(options.workers, static_cast<int>(pages.size()));
  std::vector<std::thread> threads;
  threads.reserve(static_cast<std::size_t>(workers));

  for (int worker = 0; worker < workers; ++worker) {
    threads.emplace_back([&] {
      while (true) {
        const std::size_t index = next.fetch_add(1);
        if (index >= pages.size()) {
          return;
        }
        const PageRef& page = pages[index];
        std::string text = backend->read_text(page.image_path);
        Classification classification = classify_page(text, page.features, profile);
        if (classification.candidate) {
          Candidate candidate;
          candidate.page = page;
          candidate.classification = classification;
          candidate.text_excerpt = excerpt(text);
          std::lock_guard<std::mutex> lock(candidates_mutex);
          result.candidates.push_back(std::move(candidate));
        }
        const std::size_t finished = done.fetch_add(1) + 1;
        if (options.format == OutputFormat::Table) {
          std::lock_guard<std::mutex> lock(progress_mutex);
          std::cerr << "\rScanning " << bar(static_cast<double>(finished) / static_cast<double>(pages.size())) << ' '
                    << finished << '/' << pages.size() << std::flush;
        }
      }
    });
  }

  for (std::thread& thread : threads) {
    thread.join();
  }
  if (options.format == OutputFormat::Table) {
    std::cerr << "\rScanning " << bar(1.0) << ' ' << pages.size() << '/' << pages.size() << "\n";
  }

  add_visual_matches(result.candidates, pages);
  std::sort(result.candidates.begin(), result.candidates.end(), [](const Candidate& left, const Candidate& right) {
    return left.page.order < right.page.order;
  });

  if (options.review_dir.has_value()) {
    write_review_folder(result.candidates, *options.review_dir);
  }
  return result;
}

void print_result_table(const ScanResult& result) {
  std::cout << "Trimanga " << result.detector_version << "\n";
  std::cout << "Input: " << result.input << "\n";
  std::cout << "OCR: " << result.ocr_backend << "\n";
  std::cout << "Pages scanned: " << result.scanned_pages << "\n";
  std::cout << "Pages recommended for review: " << result.candidates.size() << "\n\n";

  if (result.candidates.empty()) {
    std::cout << "No suspicious pages found.\n";
    return;
  }

  std::cout << std::left << std::setw(8) << "Page" << std::setw(10) << "Score" << std::setw(10) << "Reason"
            << "File\n";
  std::cout << std::string(78, '-') << "\n";
  for (const Candidate& candidate : result.candidates) {
    std::string reason = candidate.visual_match ? "visual" : "ocr";
    std::cout << std::left << std::setw(8) << candidate.page.order << std::setw(10) << std::fixed
              << std::setprecision(1) << candidate.classification.score << std::setw(10) << reason
              << candidate.page.archive_name << "\n";
    if (!candidate.text_excerpt.empty()) {
      std::cout << "          " << candidate.text_excerpt << "\n";
    }
  }
}

void print_result_json(const ScanResult& result) {
  std::cout << "{\n";
  std::cout << "  \"input\": \"" << escape_json(result.input.string()) << "\",\n";
  std::cout << "  \"ocr_backend\": \"" << escape_json(result.ocr_backend) << "\",\n";
  std::cout << "  \"detector_version\": \"" << escape_json(result.detector_version) << "\",\n";
  std::cout << "  \"scanned_pages\": " << result.scanned_pages << ",\n";
  std::cout << "  \"candidates\": [\n";
  for (std::size_t index = 0; index < result.candidates.size(); ++index) {
    const Candidate& candidate = result.candidates[index];
    std::cout << "    {\n";
    std::cout << "      \"page\": " << candidate.page.order << ",\n";
    std::cout << "      \"file\": \"" << escape_json(candidate.page.archive_name) << "\",\n";
    std::cout << "      \"score\": " << std::fixed << std::setprecision(3) << candidate.classification.score << ",\n";
    std::cout << "      \"indicator\": " << candidate.classification.indicator << ",\n";
    std::cout << "      \"story_confidence\": " << std::fixed << std::setprecision(3) << candidate.classification.story << ",\n";
    std::cout << "      \"outlier_distance\": " << std::fixed << std::setprecision(3) << candidate.classification.outlier << ",\n";
    std::cout << "      \"visual_match\": " << (candidate.visual_match ? "true" : "false") << ",\n";
    std::cout << "      \"visual_distance\": " << candidate.visual_distance << ",\n";
    std::cout << "      \"excerpt\": \"" << escape_json(candidate.text_excerpt) << "\"\n";
    std::cout << "    }" << (index + 1 == result.candidates.size() ? "" : ",") << "\n";
  }
  std::cout << "  ]\n";
  std::cout << "}\n";
}

}  // namespace trimanga
