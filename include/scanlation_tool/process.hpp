#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace scanlation {

struct ProcessResult {
  int exit_code = -1;
  std::string stdout_text;
};

ProcessResult run_process(const std::vector<std::string>& args, int timeout_seconds = 60);
std::string shell_quote(const std::string& value);
bool command_exists(const std::string& command);

}  // namespace scanlation
