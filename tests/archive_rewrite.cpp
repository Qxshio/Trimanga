#include "trimanga/file_utils.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: archive_rewrite <archive> <entry> [entry...]\n";
    return 2;
  }

  std::unordered_set<std::string> entries;
  for (int index = 2; index < argc; ++index) {
    entries.insert(argv[index]);
  }

  try {
    trimanga::remove_archive_entries(std::filesystem::path(argv[1]), entries);
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
  return 0;
}
