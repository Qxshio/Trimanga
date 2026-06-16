#include "trimanga/previewer.hpp"

#include "preview_ipc.hpp"

#include <filesystem>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <cstdlib>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <spawn.h>
extern char** environ;
#endif

namespace fs = std::filesystem;

namespace trimanga {
namespace {

fs::path executable_path() {
#if defined(__APPLE__)
  std::vector<char> buffer(4096);
  uint32_t size = static_cast<uint32_t>(buffer.size());
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
    buffer.assign(size + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
      return {};
    }
  }
  return fs::weakly_canonical(fs::path(buffer.data()));
#else
  return {};
#endif
}

fs::path preview_executable_path() {
  const fs::path current = executable_path();
  if (current.empty()) {
    return {};
  }
  return current.parent_path() / "trimanga-preview";
}

bool run_preview_process(const fs::path& helper, const fs::path& input_path, const fs::path& output_path) {
#if defined(__APPLE__)
  std::string helper_string = helper.string();
  std::string input_string = input_path.string();
  std::string output_string = output_path.string();
  char* const argv[] = {helper_string.data(), input_string.data(), output_string.data(), nullptr};

  pid_t pid = 0;
  if (posix_spawn(&pid, helper_string.c_str(), nullptr, nullptr, argv, environ) != 0) {
    return false;
  }
  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
  (void)helper;
  (void)input_path;
  (void)output_path;
  return false;
#endif
}

}  // namespace

bool review_candidates(std::vector<Candidate>& candidates) {
  if (candidates.empty()) {
    return false;
  }

  const fs::path helper = preview_executable_path();
  if (helper.empty() || !fs::exists(helper)) {
    return false;
  }

  const fs::path input_path = preview_ipc::unique_preview_path(".in");
  const fs::path output_path = preview_ipc::unique_preview_path(".out");
  if (!preview_ipc::write_preview_input(input_path, candidates)) {
    return false;
  }

  const bool ran = run_preview_process(helper, input_path, output_path);
  const bool read = ran && preview_ipc::read_preview_output(output_path, candidates);

  std::error_code ignored;
  fs::remove(input_path, ignored);
  fs::remove(output_path, ignored);
  return read;
}

}  // namespace trimanga
