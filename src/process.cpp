#include "scanlation_tool/process.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <sys/wait.h>

namespace scanlation {

std::string shell_quote(const std::string& value) {
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
  command << " 2>/dev/null";

  FILE* pipe = popen(command.str().c_str(), "r");
  if (pipe == nullptr) {
    return {-1, ""};
  }

  std::string output;
  std::array<char, 4096> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }
  int status = pclose(pipe);
  int exit_code = status;
  if (WIFEXITED(status)) {
    exit_code = WEXITSTATUS(status);
  }
  return {exit_code, output};
}

bool command_exists(const std::string& command) {
  const ProcessResult result = run_process({"which", command});
  return result.exit_code == 0 && !result.stdout_text.empty();
}

}  // namespace scanlation
