#include "trimanga/scanner.hpp"

#include "trimanga/classifier.hpp"
#include "trimanga/file_utils.hpp"
#include "trimanga/image_features.hpp"
#include "trimanga/detector.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <mutex>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace trimanga {

namespace {

constexpr int kMaxWorkers = 256;

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

class TerminalOutput {
 public:
  explicit TerminalOutput(bool enabled) : enabled_(enabled) {
#if !defined(_WIN32)
    if (enabled_) {
      terminal_.open("/dev/tty");
    }
#endif
  }

  void write(const std::string& message) {
    if (!enabled_) {
      return;
    }
#if !defined(_WIN32)
    if (terminal_.is_open()) {
      terminal_ << message << std::flush;
      return;
    }
#endif
    std::cout << message << std::flush;
  }

 private:
  bool enabled_ = false;
#if !defined(_WIN32)
  std::ofstream terminal_;
#endif
};

class ScopedSystemOutputSilencer {
 public:
  explicit ScopedSystemOutputSilencer(bool enabled) : enabled_(enabled) {
#if !defined(_WIN32)
    if (!enabled_) {
      return;
    }
    fflush(stdout);
    fflush(stderr);
    saved_stdout_fd_ = dup(STDOUT_FILENO);
    saved_fd_ = dup(STDERR_FILENO);
    null_fd_ = open("/dev/null", O_WRONLY);
    if (saved_fd_ >= 0 && null_fd_ >= 0) {
      if (saved_stdout_fd_ >= 0) {
        dup2(null_fd_, STDOUT_FILENO);
      }
      dup2(null_fd_, STDERR_FILENO);
    }
#else
    (void)enabled_;
#endif
  }

  ScopedSystemOutputSilencer(const ScopedSystemOutputSilencer&) = delete;
  ScopedSystemOutputSilencer& operator=(const ScopedSystemOutputSilencer&) = delete;

  ~ScopedSystemOutputSilencer() {
#if !defined(_WIN32)
    if (!enabled_) {
      return;
    }
    fflush(stdout);
    fflush(stderr);
    if (saved_stdout_fd_ >= 0) {
      dup2(saved_stdout_fd_, STDOUT_FILENO);
      close(saved_stdout_fd_);
    }
    if (saved_fd_ >= 0) {
      dup2(saved_fd_, STDERR_FILENO);
      close(saved_fd_);
    }
    if (null_fd_ >= 0) {
      close(null_fd_);
    }
#endif
  }

 private:
  bool enabled_ = false;
#if !defined(_WIN32)
  int saved_stdout_fd_ = -1;
  int saved_fd_ = -1;
  int null_fd_ = -1;
#endif
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

std::string seconds(std::chrono::duration<double> duration) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2) << duration.count() << "s";
  return out.str();
}

FeatureStats stats_for_values(std::vector<double> values) {
  FeatureStats stats;
  if (values.empty()) {
    return stats;
  }
  std::sort(values.begin(), values.end());
  const std::size_t mid = values.size() / 2;
  if (values.size() % 2 == 1) {
    stats.median = values[mid];
  } else {
    stats.median = (values[mid - 1] + values[mid]) / 2.0;
  }
  const double sum = std::accumulate(values.begin(), values.end(), 0.0);
  stats.mean = sum / static_cast<double>(values.size());
  double variance = 0.0;
  for (double value : values) {
    const double diff = value - stats.mean;
    variance += diff * diff;
  }
  stats.stdev = std::sqrt(variance / static_cast<double>(values.size()));
  if (stats.stdev <= 0.0) {
    stats.stdev = 1e-9;
  }
  return stats;
}

template <typename Func>
void parallel_pages(std::vector<PageRef>& pages, int workers, Func&& func) {
  std::atomic_size_t next{0};
  const int actual_workers = std::min<int>(std::max(1, workers), static_cast<int>(pages.size()));
  std::vector<std::thread> threads;
  threads.reserve(static_cast<std::size_t>(actual_workers));
  for (int worker = 0; worker < actual_workers; ++worker) {
    threads.emplace_back([&] {
      while (true) {
        const std::size_t index = next.fetch_add(1);
        if (index >= pages.size()) {
          return;
        }
        func(pages[index], index);
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
}

void extract_features_parallel(std::vector<PageRef>& pages, int workers) {
  if (pages.empty()) {
    return;
  }
  parallel_pages(pages, workers, [](PageRef& page, std::size_t) {
    page.features = extract_page_features(page.image_path);
  });
}

VolumeProfile build_volume_profile_from_features(const std::vector<PageRef>& pages) {
  std::vector<double> aspect;
  std::vector<double> edge;
  std::vector<double> entropy;
  std::vector<double> ink;
  std::vector<double> dark;
  std::vector<double> white;
  std::vector<double> art;
  std::vector<double> panels;
  aspect.reserve(pages.size());
  edge.reserve(pages.size());
  entropy.reserve(pages.size());
  ink.reserve(pages.size());
  dark.reserve(pages.size());
  white.reserve(pages.size());
  art.reserve(pages.size());
  panels.reserve(pages.size());

  for (const PageRef& page : pages) {
    if (!page.features.valid) {
      continue;
    }
    aspect.push_back(page.features.aspect_ratio);
    edge.push_back(page.features.edge_density);
    entropy.push_back(page.features.entropy);
    ink.push_back(page.features.ink_coverage);
    dark.push_back(page.features.dark_coverage);
    white.push_back(page.features.white_coverage);
    art.push_back(page.features.artwork_coverage);
    panels.push_back(page.features.panel_count);
  }

  VolumeProfile profile;
  profile.aspect_ratio = stats_for_values(std::move(aspect));
  profile.edge_density = stats_for_values(std::move(edge));
  profile.entropy = stats_for_values(std::move(entropy));
  profile.ink_coverage = stats_for_values(std::move(ink));
  profile.dark_coverage = stats_for_values(std::move(dark));
  profile.white_coverage = stats_for_values(std::move(white));
  profile.artwork_coverage = stats_for_values(std::move(art));
  profile.panel_count = stats_for_values(std::move(panels));
  profile.valid = pages.end() != std::find_if(pages.begin(), pages.end(), [](const PageRef& page) {
                    return page.features.valid;
                  });
  return profile;
}

void status_line(const ScanOptions& options, const std::string& message) {
  if (options.format == OutputFormat::Table && options.verbose) {
    std::cout << message << "\n";
  }
}

void progress_line(const ScanOptions& options, TerminalOutput& terminal, std::size_t finished, std::size_t total) {
  if (options.format != OutputFormat::Table || !options.progress) {
    return;
  }
  std::ostringstream line;
  line << "\r\033[2KScanning pages " << bar(static_cast<double>(finished) / static_cast<double>(total)) << ' '
       << finished << '/' << total;
  terminal.write(line.str());
}

int worker_count_for(const ScanOptions& options, std::size_t page_count) {
  if (page_count == 0) {
    return 1;
  }
  return std::min<int>({std::max(1, options.workers), static_cast<int>(page_count), kMaxWorkers});
}

}  // namespace

ScanResult scan(const fs::path& input, const ScanOptions& options) {
  const auto started_at = std::chrono::steady_clock::now();
  if (options.workers < 1) {
    throw std::runtime_error("workers must be at least 1");
  }
  if (!fs::exists(input)) {
    throw std::runtime_error("input does not exist: " + input.string());
  }

  status_line(options, "Preparing input...");

  TempDirectory temp;
  std::vector<PageRef> pages = build_page_refs(input, temp);
  const auto prepared_at = std::chrono::steady_clock::now();
  if (options.keep_temp) {
    temp.keep();
  }
  if (pages.empty()) {
    ScanResult empty;
    empty.input = input;
    empty.detector_version = kDetectorVersion;
    empty.detector = "Trimanga detector";
    empty.details = options.details;
    empty.timings = options.timings;
    empty.prepare_time = prepared_at - started_at;
    empty.total_time = prepared_at - started_at;
    return empty;
  }

  const int workers = worker_count_for(options, pages.size());
  auto detector = make_page_detector();

  ScanResult result;
  result.input = input;
  result.detector_version = kDetectorVersion;
  result.detector = detector->name();
  result.scanned_pages = pages.size();
  result.details = options.details;
  result.timings = options.timings;
  result.prepare_time = prepared_at - started_at;

  std::vector<std::string> page_texts(pages.size());
  std::atomic_size_t next_analysis{0};
  std::atomic_size_t done_analysis{0};
  std::mutex progress_mutex;
  std::vector<std::thread> threads;
  threads.reserve(static_cast<std::size_t>(workers));

  status_line(options, "Found " + std::to_string(pages.size()) + " pages. Analyzing with " + std::to_string(workers) +
                           " workers...");
  if (options.verbose && options.workers > workers) {
    status_line(options, "Worker request capped at " + std::to_string(workers) + " to keep the terminal responsive.");
  }
  status_line(options, "Using Trimanga's built-in scanlation and ad detector.");
  TerminalOutput terminal(options.format == OutputFormat::Table && options.progress);
  ScopedSystemOutputSilencer silence_system_warnings(false);

  const auto profile_started_at = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point profile_finished_at;
  std::thread profile_thread([&] {
    extract_features_parallel(pages, workers);
    profile_finished_at = std::chrono::steady_clock::now();
  });

  const auto analysis_started_at = std::chrono::steady_clock::now();
  for (int worker = 0; worker < workers; ++worker) {
    threads.emplace_back([&] {
      while (true) {
        const std::size_t index = next_analysis.fetch_add(1);
        if (index >= pages.size()) {
          return;
        }
        const PageRef& page = pages[index];
        page_texts[index] = detector->analyze_page(page.image_path);
        const std::size_t finished = done_analysis.fetch_add(1) + 1;
        if (options.format == OutputFormat::Table && options.progress) {
          std::lock_guard<std::mutex> lock(progress_mutex);
          progress_line(options, terminal, finished, pages.size());
        }
      }
    });
  }

  for (std::thread& thread : threads) {
    thread.join();
  }
  profile_thread.join();
  if (options.format == OutputFormat::Table && options.progress) {
    progress_line(options, terminal, pages.size(), pages.size());
    terminal.write("\n");
  }

  VolumeProfile profile = build_volume_profile_from_features(pages);
  std::mutex candidates_mutex;
  std::atomic_size_t next_classify{0};
  std::vector<std::thread> classify_threads;
  classify_threads.reserve(static_cast<std::size_t>(workers));
  for (int worker = 0; worker < workers; ++worker) {
    classify_threads.emplace_back([&] {
      while (true) {
        const std::size_t index = next_classify.fetch_add(1);
        if (index >= pages.size()) {
          return;
        }
        Classification classification = classify_page(page_texts[index], pages[index].features, profile);
        if (!classification.candidate) {
          continue;
        }
        Candidate candidate;
        candidate.page = pages[index];
        candidate.classification = classification;
        candidate.text_excerpt = excerpt(page_texts[index]);
        std::lock_guard<std::mutex> lock(candidates_mutex);
        result.candidates.push_back(std::move(candidate));
      }
    });
  }
  for (std::thread& thread : classify_threads) {
    thread.join();
  }
  const auto classified_at = std::chrono::steady_clock::now();
  result.profile_time = profile_finished_at - profile_started_at;
  result.analysis_time = classified_at - analysis_started_at;

  add_visual_matches(result.candidates, pages);
  const auto visual_matched_at = std::chrono::steady_clock::now();
  result.visual_match_time = visual_matched_at - classified_at;
  std::sort(result.candidates.begin(), result.candidates.end(), [](const Candidate& left, const Candidate& right) {
    return left.page.order < right.page.order;
  });

  if (options.review_dir.has_value()) {
    write_review_folder(result.candidates, *options.review_dir);
    result.copied_review_pages = true;
  }
  const auto finished_at = std::chrono::steady_clock::now();
  result.review_copy_time = finished_at - visual_matched_at;
  result.total_time = finished_at - started_at;
  return result;
}

void print_result_table(const ScanResult& result) {
  std::cout << "Trimanga " << result.detector_version << "\n";
  std::cout << "Input: " << result.input << "\n";
  std::cout << "Detector: " << result.detector << "\n";
  std::cout << "Pages scanned: " << result.scanned_pages << "\n";
  std::cout << "Pages recommended for review: " << result.candidates.size() << "\n\n";

  if (result.timings) {
    std::cout << "Timings\n";
    std::cout << "  Prepare:       " << seconds(result.prepare_time) << "\n";
    std::cout << "  Page profile:  " << seconds(result.profile_time) << "\n";
    std::cout << "  Analyze:       " << seconds(result.analysis_time) << "\n";
    std::cout << "  Visual match:  " << seconds(result.visual_match_time) << "\n";
    if (result.copied_review_pages) {
      std::cout << "  Review copy:   " << seconds(result.review_copy_time) << "\n";
    }
    std::cout << "  Total:         " << seconds(result.total_time) << "\n\n";
    std::cout << "Profile and page analysis can overlap, so phase times may not add up to total time.\n\n";
  }

  if (result.candidates.empty()) {
    std::cout << "No review candidates found.\n";
    return;
  }

  std::cout << std::left << std::setw(8) << "Page" << std::setw(10) << "Score" << std::setw(10) << "Reason"
            << "File\n";
  std::cout << std::string(72, '-') << "\n";
  for (const Candidate& candidate : result.candidates) {
    std::string reason = candidate.visual_match ? "repeat" : "signal";
    std::cout << std::left << std::setw(8) << candidate.page.order << std::setw(10) << std::fixed
              << std::setprecision(1) << candidate.classification.score << std::setw(10) << reason
              << candidate.page.archive_name << "\n";
    if (result.details && !candidate.text_excerpt.empty()) {
      std::cout << "          " << candidate.text_excerpt << "\n";
    }
  }
  if (!result.details) {
    std::cout << "\nRun again with --details to include detector signals for each review candidate.\n";
  }
}

void print_result_json(const ScanResult& result) {
  std::cout << "{\n";
  std::cout << "  \"input\": \"" << escape_json(result.input.string()) << "\",\n";
  std::cout << "  \"detector\": \"" << escape_json(result.detector) << "\",\n";
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
