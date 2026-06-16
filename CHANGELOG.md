# Changelog

## 0.1.0

- Initial Trimanga C++ CLI.
- Added folder, image, `.zip`, and `.cbz` scanning.
- Added Apple Vision OCR on macOS.
- Added Tesseract OCR fallback for macOS, Linux, and Windows.
- Added conservative OCR/layout classifier.
- Added review folder export.
- Added JSON output.
- Added CMake, Makefile, smoke tests, CI, and package manager templates.
- Kept normal scan output compact by default, with `--details` for OCR excerpts.
- Parallelized page profiling so `--workers` applies to image analysis as well as OCR.
- Overlapped page profiling and OCR so the scanner does less full-volume waiting between phases.
- Added `--ocr-speed fast` for faster Apple Vision and Tesseract scans.
- Added `--ocr none` for dependency-free visual-only scans.
- Capped Tesseract's internal POSIX OpenMP thread usage to reduce resource spikes.
- Added `--timings` for scan phase benchmarks.
