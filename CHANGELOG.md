# Changelog

## 0.1.0

- Initial Trimanga C++ CLI.
- Added folder, image, `.zip`, and `.cbz` scanning.
- Added built-in scanlation and ad-page detector.
- Added conservative layout classifier.
- Added review folder export.
- Added JSON output.
- Added CMake, Makefile, smoke tests, CI, and package manager templates.
- Kept normal scan output compact by default, with `--details` for detector signals.
- Parallelized page profiling and page analysis.
- Removed external recognition dependencies.
- Added a portable image decoder for consistent macOS, Linux and Windows behavior.
- Added `--timings` for scan phase benchmarks.
