# Scanlation Tool

Standalone C++ scanner for detecting scanlation credits, recruitment pages, support ads, and similar removable pages in manga folders or `.cbz` archives.

## Goals

- Keep detection conservative: false positives are worse than missed pages.
- Prefer Apple Vision OCR on macOS, with Tesseract as a portable fallback.
- Use reusable signals, not manga-specific titles, group names, or page numbers.
- Keep the project independent from the Python manga downloader.

## Build

```sh
cmake -S . -B build
cmake --build build
```

On macOS without CMake installed, use the checked-in Makefile:

```sh
make
```

## Usage

```sh
./build/scanlation_tool scan "/path/to/manga.cbz"
./build/scanlation_tool scan "/path/to/manga-folder" --format json
./build/scanlation_tool scan "/path/to/manga.cbz" --review-dir /tmp/review
```

Options:

- `--ocr auto|apple|tesseract`
- `--workers N`
- `--format table|json`
- `--review-dir PATH` copies detected pages into a numbered review folder

## Current Platform Notes

macOS builds use Apple Vision through Objective-C++ and ImageIO/CoreGraphics for image analysis. Other platforms can still use Tesseract OCR, but image-feature support is intentionally minimal until a cross-platform image backend is added.

Apple Vision may print private TextRecognition model warnings on some macOS installs. The scanner treats the backend as available if Vision returns OCR text; those warnings are not fatal.
