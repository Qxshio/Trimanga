#include "trimanga/previewer.hpp"

#include "preview_ipc.hpp"

#include <filesystem>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 3) {
    return 2;
  }

  std::vector<trimanga::Candidate> candidates;
  if (!trimanga::preview_ipc::read_preview_input(std::filesystem::path(argv[1]), candidates)) {
    return 3;
  }

  const bool reviewed = trimanga::run_sdl_review_window(candidates);
  if (!trimanga::preview_ipc::write_preview_output(std::filesystem::path(argv[2]), reviewed, candidates)) {
    return 4;
  }
  return reviewed ? 0 : 5;
}
