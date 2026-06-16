#pragma once

#include <cstdint>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace trimanga {

struct PageFeatures {
  double aspect_ratio = 0.0;
  double edge_density = 0.0;
  double entropy = 0.0;
  double ink_coverage = 0.0;
  double dark_coverage = 0.0;
  double white_coverage = 0.0;
  double artwork_coverage = 0.0;
  double panel_count = 0.0;
  std::optional<std::uint64_t> visual_hash;
  bool valid = false;
};

struct PageRef {
  std::size_t order = 0;
  std::filesystem::path image_path;
  std::string archive_name;
  std::string label;
  PageFeatures features;
};

struct FeatureStats {
  double mean = 0.0;
  double median = 0.0;
  double stdev = 1e-9;
};

struct VolumeProfile {
  FeatureStats aspect_ratio;
  FeatureStats edge_density;
  FeatureStats entropy;
  FeatureStats ink_coverage;
  FeatureStats dark_coverage;
  FeatureStats white_coverage;
  FeatureStats artwork_coverage;
  FeatureStats panel_count;
  bool valid = false;
};

struct Classification {
  bool candidate = false;
  bool visual_seed = false;
  double score = 0.0;
  double story = 0.0;
  double outlier = 0.0;
  int indicator = 0;
};

struct Candidate {
  PageRef page;
  Classification classification;
  std::string text_excerpt;
  bool visual_match = false;
  int visual_distance = 0;
};

enum class OutputFormat {
  Table,
  Json,
};

struct ScanOptions {
  int workers = 4;
  OutputFormat format = OutputFormat::Table;
  std::optional<std::filesystem::path> review_dir;
  bool keep_temp = false;
  bool details = false;
  bool timings = false;
  bool progress = false;
  bool verbose = false;
};

struct ScanResult {
  std::filesystem::path input;
  std::string detector;
  std::string detector_version;
  std::size_t scanned_pages = 0;
  bool details = false;
  bool timings = false;
  bool copied_review_pages = false;
  std::chrono::duration<double> prepare_time{};
  std::chrono::duration<double> profile_time{};
  std::chrono::duration<double> analysis_time{};
  std::chrono::duration<double> visual_match_time{};
  std::chrono::duration<double> review_copy_time{};
  std::chrono::duration<double> total_time{};
  std::vector<Candidate> candidates;
};

inline constexpr const char* kDetectorVersion = "2026-06-16-cpp-v1";
inline constexpr int kVisualHashSize = 16;
inline constexpr int kVisualMatchDistance = 2;

}  // namespace trimanga
