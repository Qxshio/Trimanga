#include "preview_ipc.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <random>
#include <string>

namespace fs = std::filesystem;

namespace trimanga::preview_ipc {
namespace {

constexpr std::uint32_t kPreviewFileVersion = 1;

std::uint8_t encode_action(ReviewAction action) {
  return action == ReviewAction::Delete ? 1 : 0;
}

ReviewAction decode_action(std::uint8_t value) {
  return value == 1 ? ReviewAction::Delete : ReviewAction::Undecided;
}

template <typename T>
void write_value(std::ostream& output, const T& value) {
  output.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool read_value(std::istream& input, T& value) {
  input.read(reinterpret_cast<char*>(&value), sizeof(T));
  return static_cast<bool>(input);
}

void write_string(std::ostream& output, const std::string& value) {
  const auto size = static_cast<std::uint64_t>(value.size());
  write_value(output, size);
  output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

bool read_string(std::istream& input, std::string& value) {
  std::uint64_t size = 0;
  if (!read_value(input, size) || size > 1024 * 1024) {
    return false;
  }
  value.assign(static_cast<std::size_t>(size), '\0');
  input.read(value.data(), static_cast<std::streamsize>(size));
  return static_cast<bool>(input);
}

}  // namespace

fs::path unique_preview_path(const std::string& suffix) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  std::random_device random;
  return fs::temp_directory_path() /
         ("trimanga-preview-" + std::to_string(stamp) + "-" + std::to_string(random()) + suffix);
}

bool write_preview_input(const fs::path& path, const std::vector<Candidate>& candidates) {
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    return false;
  }
  write_value(output, kPreviewFileVersion);
  const auto count = static_cast<std::uint64_t>(candidates.size());
  write_value(output, count);
  for (const Candidate& candidate : candidates) {
    write_string(output, candidate.page.image_path.string());
    write_string(output, candidate.page.archive_name);
    write_value(output, encode_action(candidate.review_action));
  }
  return static_cast<bool>(output);
}

bool read_preview_input(const fs::path& path, std::vector<Candidate>& candidates) {
  std::ifstream input(path, std::ios::binary);
  std::uint32_t version = 0;
  std::uint64_t count = 0;
  if (!input || !read_value(input, version) || version != kPreviewFileVersion || !read_value(input, count)) {
    return false;
  }
  candidates.assign(static_cast<std::size_t>(count), Candidate{});
  for (Candidate& candidate : candidates) {
    std::string image_path;
    std::string archive_name;
    std::uint8_t action = 0;
    if (!read_string(input, image_path) || !read_string(input, archive_name) || !read_value(input, action)) {
      return false;
    }
    candidate.page.image_path = fs::path(image_path);
    candidate.page.archive_name = archive_name;
    candidate.review_action = decode_action(action);
  }
  return true;
}

bool write_preview_output(const fs::path& path, bool reviewed, const std::vector<Candidate>& candidates) {
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    return false;
  }
  write_value(output, kPreviewFileVersion);
  write_value(output, static_cast<std::uint8_t>(reviewed ? 1 : 0));
  const auto count = static_cast<std::uint64_t>(candidates.size());
  write_value(output, count);
  for (const Candidate& candidate : candidates) {
    write_value(output, encode_action(candidate.review_action));
  }
  return static_cast<bool>(output);
}

bool read_preview_output(const fs::path& path, std::vector<Candidate>& candidates) {
  std::ifstream input(path, std::ios::binary);
  std::uint32_t version = 0;
  std::uint8_t reviewed = 0;
  std::uint64_t count = 0;
  if (!input || !read_value(input, version) || version != kPreviewFileVersion || !read_value(input, reviewed) ||
      !read_value(input, count) || count != candidates.size() || reviewed == 0) {
    return false;
  }
  for (Candidate& candidate : candidates) {
    std::uint8_t action = 0;
    if (!read_value(input, action)) {
      return false;
    }
    candidate.review_action = decode_action(action);
  }
  return true;
}

}  // namespace trimanga::preview_ipc
