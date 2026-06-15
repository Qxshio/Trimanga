#pragma once

#include "scanlation_tool/types.hpp"

#include <filesystem>

namespace scanlation {

ScanResult scan(const std::filesystem::path& input, const ScanOptions& options);
void print_result_table(const ScanResult& result);
void print_result_json(const ScanResult& result);

}  // namespace scanlation
