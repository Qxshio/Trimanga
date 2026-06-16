# Trimanga

Trimanga is a command-line tool for cleaning manga archives. It scans folders, images, `.zip`, and `.cbz` files for likely scanlation credits, recruitment pages, support ads and repeated release clutter, then gives you a reviewable set of pages. Trimanga will never alter any of the provided files, only offer suggestions.

It is built for people who care about clean, portable manga libraries: fewer interruption pages, smaller archives, better reader navigation.

## Why Trimanga Exists

Digital manga collections grow fast. A single library can contain thousands of pages that are not part of the actual story: release credits, recruitment pages, Discord promos, donation pages, duplicated group bumpers and archive filler. These pages are not always bad, but they often become noise once the book is already in your personal library.

Removing that clutter matters because it:

- Keeps reading focused by reducing non-story interruptions.
- Saves disk space across large collections.
- Makes `.cbz` archives cleaner for e-readers, tablets and library managers.
- Reduces duplicate pages that appear across many chapters or volumes.
- Helps preserve a polished, book-like archive without hand-checking every page.

Trimanga is intentionally conservative. False positives are more harmful than missed ads, so the tool is designed to recommend pages for review rather than aggressively remove content. You are responsible for removing any of the files suggested.

## What It Detects

Trimanga combines OCR, page-layout statistics, volume-wide outlier scoring and visual similarity matching to flag likely removable pages, including:

- Scanlation credit pages
- Recruitment pages
- Discord/social/contact pages
- Donation or support pages
- Official purchase/support reminder pages
- Repeated release cards or bumpers
- Extreme visual outliers compared with the rest of the volume

Normal manga pages with panels, speech bubbles, artwork and dialogue are given a strong negative score so they are biased toward being kept.

## Supported Inputs

- Manga folders, scanned recursively
- Single image files
- `.cbz` archives
- `.zip` archives

Supported image types:

```text
.jpg .jpeg .png .webp .tif .tiff
```

## Features

- Native Apple Vision OCR on macOS.
- Tesseract OCR fallback on macOS, Linux and Windows.
- Parallel OCR workers for faster scans.
- Conservative classifier tuned for review-first workflows.
- JSON output for automation.

## Requirements

### macOS

- Apple Clang or Xcode Command Line Tools
- CMake 3.22+ recommended
- Tesseract optional, used as fallback
- `unzip` or `7z` for archive extraction

Apple Vision is built into macOS and is preferred by default.

```sh
xcode-select --install
brew install cmake tesseract unzip
```

### Linux

- CMake 3.22+
- C++20 compiler such as GCC 11+ or Clang 14+
- Tesseract OCR
- `unzip` or `7z`

Debian/Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y cmake g++ tesseract-ocr unzip
```

Fedora:

```sh
sudo dnf install cmake gcc-c++ tesseract unzip
```

### Windows

- Visual Studio 2022 Build Tools or another C++20 compiler
- CMake 3.22+
- Tesseract OCR installed and available on `PATH`
- PowerShell, included with Windows, for archive extraction

Recommended Tesseract install:

```powershell
winget install UB-Mannheim.TesseractOCR
```

or:

```powershell
choco install tesseract
```

## Build

### CMake

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The binary will be created at:

```text
build/trimanga
```

On Windows with Visual Studio generators, the binary is usually under:

```text
build/Release/trimanga.exe
```

### macOS Makefile

If CMake is not installed on macOS, the checked-in Makefile can build the native Apple Vision version:

```sh
make
```

## Usage

Scan a `.cbz`:

```sh
trimanga scan "I Hear the Sunspot.cbz"
```

Scan a manga folder and copy suspicious pages into a review folder:

```sh
trimanga scan "./I Hear the Sunspot" \
  --workers 4 \
  --review-dir /tmp/sunspot-review
```

Output JSON for scripts:

```sh
trimanga scan "./Volume 01.cbz" --format json
```

Options:

```text
--ocr auto|apple|tesseract   OCR backend to use. Default: auto
--workers N                  Number of OCR workers. Default: 4
--format table|json          Output format. Default: table
--review-dir PATH            Copy suspicious pages into this folder
--details                    Include OCR excerpts in table output
--timings                    Print phase timings after the scan
--keep-temp                  Keep extracted archive temp files
```

The default table output is intentionally compact. Use `--details` when you want to inspect the OCR text that caused each page to be flagged.
Use `--timings` to see where time is spent across extraction, page profiling, OCR, visual matching, and review export.

## Package Builds

CPack is configured for release archives and native Linux packages:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --build build --target package
```

Expected package formats:

- macOS: `.tar.gz`
- Linux: `.tar.gz`, `.deb`, `.rpm`
- Windows: `.zip`

## Safety Model

Trimanga does not delete pages by default. It identifies pages that deserve attention and can copy them to a review folder. This keeps the cleanup workflow auditable and avoids destructive surprises.

The classifier is tuned around three principles:

- Strong evidence is required before a page is flagged.
- Normal manga-story features reduce confidence.
- Borderline pages should be kept.

## Platform Notes

macOS has the strongest native support because Trimanga uses Apple Vision for OCR and ImageIO/CoreGraphics for image statistics.

Linux and Windows are supported through the portable Tesseract CLI backend. Their current image-feature extractor is intentionally minimal, so OCR indicators carry more of the decision-making until a cross-platform image backend is added.

Apple Vision may print private TextRecognition model warnings on some macOS installs. Trimanga treats the backend as available if Vision returns OCR text; those warnings are not fatal.

## Development

Run the smoke test after building:

```sh
tests/smoke.sh
```

Run CTest when building with CMake:

```sh
ctest --test-dir build --output-on-failure
```

Format with `clang-format` using the checked-in [.clang-format](.clang-format).
