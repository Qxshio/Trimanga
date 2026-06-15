#include "trimanga/process.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
#include <io.h>
#else
#include <sys/wait.h>
#endif

namespace trimanga {

std::string shell_quote(const std::string& value) {
#if defined(_WIN32)
  std::string quoted = "\"";
  for (char ch : value) {
    if (ch == '"' || ch == '\\') {
      quoted += '\\';
    }
    quoted += ch;
  }
  quoted += "\"";
  return quoted;
#else
  std::string quoted = "'";
  for (char ch : value) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted += ch;
    }
  }
  quoted += "'";
  return quoted;
#endif
}

ProcessResult run_process(const std::vector<std::string>& args, int /*timeout_seconds*/) {
  if (args.empty()) {
    return {-1, ""};
  }
  std::ostringstream command;
  for (std::size_t index = 0; index < args.size(); ++index) {
    if (index != 0) {
      command << ' ';
    }
    command << shell_quote(args[index]);
  }
#if defined(_WIN32)
  command << " 2>NUL";
#else
  command << " 2>/dev/null";
#endif

#if defined(_WIN32)
  FILE* pipe = _popen(command.str().c_str(), "r");
#else
  FILE* pipe = popen(command.str().c_str(), "r");
#endif
  if (pipe == nullptr) {
    return {-1, ""};
  }

  std::string output;
  std::array<char, 4096> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }
  int status =
#if defined(_WIN32)
      _pclose(pipe);
#else
      pclose(pipe);
#endif
  int exit_code = status;
#if defined(_WIN32)
  if (exit_code < 0) {
    exit_code = -1;
  }
#else
  if (WIFEXITED(status)) {
    exit_code = WEXITSTATUS(status);
  }
#endif
  return {exit_code, output};
}

bool command_exists(const std::string& command) {
#if defined(_WIN32)
  const ProcessResult result = run_process({"where", command});
#else
  const ProcessResult result = run_process({"which", command});
#endif
  return result.exit_code == 0 && !result.stdout_text.empty();
}

}  // namespace trimanga
