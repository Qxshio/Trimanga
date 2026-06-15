# Scanlation Tool

Scanlation Tool is a standalone C++ command-line scanner for finding likely scanlation credits, recruitment pages, support ads, and similar removable pages inside manga folders or `.cbz` archives.

The detector is intentionally conservative. It combines OCR text, page-layout statistics, volume-wide outlier scoring, and repeated-page visual matching, then biases toward keeping pages unless there is strong evidence that a page is removable.

## Features

- Scans manga folders, single image files, `.cbz`, and `.zip` archives.
- Uses Apple Vision OCR on macOS when available.
- Uses Tesseract OCR as the portable backend on macOS, Linux, and Windows.
- Runs OCR work in parallel with configurable worker count.
- Exports results as a readable table or JSON.
- Copies suspicious pages into a review folder for manual approval.
- Builds as a normal CMake project.
- Includes packaging templates for Homebrew, Scoop, Chocolatey, winget, and Linux CPack packages.

## Requirements

### macOS

- Apple Clang or Xcode Command Line Tools
- CMake 3.22+ recommended
- Tesseract optional, used as fallback
- `unzip` or `7z` for archive extraction

Apple Vision is built into macOS and is preferred by default.

### Linux

- CMake 3.22+
- C++20 compiler such as GCC 11+ or Clang 14+
- Tesseract OCR
- `unzip` or `7z`

Debian/Ubuntu example:

```sh
sudo apt-get update
sudo apt-get install -y cmake g++ tesseract-ocr unzip
```

Fedora example:

```sh
sudo dnf install cmake gcc-c++ tesseract unzip
```

### Windows

- Visual Studio 2022 Build Tools or another C++20 compiler
- CMake 3.22+
- Tesseract OCR installed and available on `PATH`
- PowerShell, included with Windows, for archive extraction

Recommended Tesseract install options:

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
build/scanlation_tool
```

On Windows with Visual Studio generators, the binary is usually under:

```text
build/Release/scanlation_tool.exe
```

### macOS Makefile

If CMake is not installed on macOS, the checked-in Makefile can build the native Apple Vision version:

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

```text
--ocr auto|apple|tesseract   OCR backend to use. Default: auto
--workers N                  Number of OCR workers. Default: 4
--format table|json          Output format. Default: table
--review-dir PATH            Copy suspicious pages into this folder
--keep-temp                  Keep extracted archive temp files
```

Example review workflow:

```sh
./build/scanlation_tool scan "I Hear the Sunspot.cbz" \
  --workers 4 \
  --review-dir /tmp/sunspot-review
```

## Package Builds

CPack is configured for release archives and native Linux packages.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --build build --target package
```

Expected package formats:

- macOS: `.tar.gz`
- Linux: `.tar.gz`, `.deb`, `.rpm`
- Windows: `.zip`

## Package Manager Templates

Packaging templates live in [packaging](packaging):

- Homebrew formula: [packaging/homebrew/scanlation-tool.rb](packaging/homebrew/scanlation-tool.rb)
- Scoop manifest: [packaging/scoop/scanlation-tool.json](packaging/scoop/scanlation-tool.json)
- Chocolatey package: [packaging/chocolatey](packaging/chocolatey)
- winget manifests: [packaging/winget](packaging/winget)
- Linux packaging notes: [packaging/linux/README.md](packaging/linux/README.md)

Before publishing, replace placeholder repository URLs, checksums, maintainers, and release artifacts.

## Platform Notes

macOS has the strongest native support because the project uses Apple Vision for OCR and ImageIO/CoreGraphics for image statistics.

Linux and Windows are supported through the portable Tesseract CLI backend. Their current image-feature extractor is intentionally minimal, so OCR indicators carry more of the decision-making until a cross-platform image backend is added.

Apple Vision may print private TextRecognition model warnings on some macOS installs. The scanner treats the backend as available if Vision returns OCR text; those warnings are not fatal.

## Development

Run the smoke test after building:

```sh
tests/smoke.sh
```

Format with `clang-format` using the checked-in [.clang-format](.clang-format).
