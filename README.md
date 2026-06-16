# Trimanga

Trimanga is a command-line tool for cleaning manga archives. It scans folders, images, `.zip`, and `.cbz` files for likely scanlation credits, recruitment pages, support ads and repeated release clutter, then gives you a reviewable set of pages. By default it only offers suggestions; interactive review can safely apply delete decisions after you confirm them.

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

Trimanga combines a built-in page detector, layout statistics, volume-wide outlier scoring and visual similarity matching to flag likely removable pages, including:

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

- Built-in cross-platform detector with no external recognition runtime.
- Parallel page analysis for faster scans.
- Conservative classifier tuned for review-first workflows.
- Optional graphical carousel review window with check/delete decisions.
- JSON output for automation.

## Requirements

### macOS

- Apple Clang or Xcode Command Line Tools
- CMake 3.22+ recommended
- SDL2, optional, for the graphical previewer

```sh
xcode-select --install
brew install cmake sdl2
```

### Linux

- CMake 3.22+
- C++20 compiler such as GCC 11+ or Clang 14+
- SDL2 development headers, optional, for the graphical previewer

Debian/Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y cmake g++ libsdl2-dev
```

Fedora:

```sh
sudo dnf install cmake gcc-c++ SDL2-devel
```

### Windows

- Visual Studio 2022 Build Tools or another C++20 compiler
- CMake 3.22+
- PowerShell, included with Windows, for archive extraction

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

If CMake is not installed on macOS, the checked-in Makefile can build Trimanga:

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
  --speed balanced \
  --review-dir /tmp/sunspot-review
```

Scan and review candidate pages in a native window:

```sh
trimanga scan "./I Hear the Sunspot" --preview
```

Output JSON for scripts:

```sh
trimanga scan "./Volume 01.cbz" --format json
```

Options:

```text
--speed eco|balanced|fast|fastest
                             Scan speed. Default: balanced
--format table|json          Output format. Default: table
--review-dir PATH            Copy suspicious pages into this folder
--preview                    Open an image review window for Keep/Delete decisions
--details                    Include detector signals in table output
--timings                    Print phase timings after the scan
--progress                   Show live progress while scanning
--verbose                    Show setup/status messages while scanning
--quiet                      Only print final results. Default
--keep-temp                  Keep extracted archive temp files
```

The default table output is intentionally compact. Use `--details` when you want to inspect the detector signals that caused each page to be flagged.
Use `--timings` to see where time is spent across extraction, page profiling, page analysis, visual matching and review export.
Use `--progress` or `--verbose` when you want live scan feedback.
Use `--preview` when you want to inspect every candidate image immediately. The preview opens as a carousel: mouse wheel, arrow keys, or `H/J/K/L` move through matches; unmarked pages are kept by default; `Space`, `X`, or `D` toggles the delete mark; `T` or `A` toggles all candidates between delete and keep; hold `Esc` to confirm. Folder and single-image inputs move deleted files into `.trimanga-trash`. `.cbz` and `.zip` inputs are rebuilt with marked pages removed.
Archive support is built into Trimanga, so `.cbz` and `.zip` scanning/rebuilding does not require system `zip`, `unzip`, `7z`, or PowerShell archive commands.
On macOS, the Make build copies the SDL preview runtime into `build/lib` and rewrites `build/trimanga` to load that local copy, so the built preview binary does not depend on Homebrew's SDL path at runtime.

Speed controls how aggressively Trimanga creates workers:

- `eco`: lower resource use
- `balanced`: based on your CPU
- `fast`: more aggressive parallelism
- `fastest`: one worker per page

`fastest` is intentionally extreme. On a 1,156-page archive, it can create 1,156 workers. It may finish quickly on some machines, but it can also cause heat, memory pressure, or sluggish terminal behavior.

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

Trimanga does not delete pages by default. It identifies pages that deserve attention and can copy them to a review folder. With `--preview`, folder and single-image inputs move pages marked Delete into a local `.trimanga-trash` folder. `.cbz` and `.zip` inputs are rebuilt after confirmation with marked pages removed.

The classifier is tuned around three principles:

- Strong evidence is required before a page is flagged.
- Normal manga-story features reduce confidence.
- Borderline pages should be kept.

## Platform Notes

Trimanga uses the same built-in detector on macOS, Linux and Windows. It does not require external recognition engines or bundled models.

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
