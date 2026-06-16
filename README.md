# Trimanga

Trimanga is a command-line tool for cleaning manga archives. It scans folders, images, `.zip`, and `.cbz` files for likely scanlation credits, recruitment pages, support ads and repeated release clutter, then gives you a reviewable set of pages. By default it only offers suggestions; interactive review can safely apply delete decisions after you confirm them.

It is built for people who care about clean, portable manga libraries: fewer interruption pages, smaller archives, better reader navigation.

<img width="400" height="500" alt="image" src="https://github.com/user-attachments/assets/8a3fff19-1e5f-4ef8-b7bb-bad128055c9a" />

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
trimanga scan "./I Hear the Sunspot"
```

Output JSON for scripts:

```sh
trimanga scan "./Volume 01.cbz" --format json --no-preview
```

Run a report-only scan without opening the preview window:

```sh
trimanga scan "./Volume 01.cbz" --no-preview
```

Options:

```text
--speed eco|balanced|fast|fastest
                             Scan speed. Default: balanced
--format table|json          Output format. Default: table
--review-dir PATH            Copy suspicious pages into this folder
--no-preview                 Do not open the image review window
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
By default, Trimanga opens the preview window when candidates are found. The preview opens as a carousel: mouse wheel, arrow keys, or `H/J/K/L` move through matches; unmarked pages are kept by default; `Space`, `X`, or `D` toggles the delete mark; `T` or `A` toggles all candidates between delete and keep; hold `Esc` to confirm. Folder and single-image inputs move deleted files into `.trimanga-trash`. `.cbz` and `.zip` inputs are rebuilt with marked pages removed.
Use `--no-preview` for scripts, CI, SSH/headless sessions, or report-only runs. When this flag is set, Trimanga still performs the full scan and prints the candidate report, but it skips the interactive image review window and does not apply delete decisions.
Archive support is built into Trimanga, so `.cbz` and `.zip` scanning/rebuilding does not require system `zip`, `unzip`, `7z`, or PowerShell archive commands.
On macOS, the Make build copies the SDL preview runtime into `build/lib` and rewrites `build/trimanga` to load that local copy, so the built preview binary does not depend on Homebrew's SDL path at runtime.

Speed controls how aggressively Trimanga creates workers:

- `eco`: lower resource use
- `balanced`: based on your CPU
- `fast`: more aggressive parallelism
- `fastest`: one worker per page

`fastest` is intentionally extreme. On a 1,156-page archive, it can create 1,156 workers. It may finish quickly on some machines, but it can also cause heat, memory pressure, or sluggish terminal behavior.

## Analysis Process

Trimanga's scan has five main phases:

1. Prepare input.
   Folders and single images are walked directly. Archives are read as manga page entries; report-only `.cbz` scans can stream images from the archive without extracting when preview, review export, and temp retention are not needed.

2. Analyze pages in parallel.
   The selected `--speed` controls how many workers are created. Each worker loads a page as grayscale, extracts page-level visual features, and runs the built-in Trimanga detector. This detector is not OCR; it looks for visual and layout patterns common to cleanup pages, such as centered credit blocks, sparse notices, recruitment cards, support pages, purchase reminders, and release bumpers.

3. Build the volume profile.
   After page features are collected, Trimanga builds a profile of the whole input so each page can be judged against the surrounding volume. This helps identify extreme visual outliers while keeping normal story pages biased toward "keep".

4. Classify candidate pages.
   Trimanga combines detector signals, page features, story-page confidence, and the volume profile. Strong cleanup evidence is required before a page is recommended. Borderline pages are intentionally kept out of the candidate list.

5. Add repeated visual matches and review output.
   After classification, Trimanga adds visual matches for pages that resemble confirmed cleanup candidates. It then sorts candidates by page order, opens the preview window unless `--no-preview` is set, optionally copies candidates to `--review-dir`, and applies confirmed review actions only if the preview was completed.

Use `--timings` to see elapsed time for prepare, page profile, analysis, visual matching, review copy, and total scan time. In the timing report, "Analyze" includes archive streaming, image loading, page feature extraction, and detector passes.

## Safety Model

Trimanga does not delete pages without review. It identifies pages that deserve attention and opens a preview window by default when candidates are found. Folder and single-image inputs move pages marked Delete into a local `.trimanga-trash` folder. `.cbz` and `.zip` inputs are rebuilt after confirmation with marked pages removed. Use `--no-preview` when you want report-only behavior.

The classifier is tuned around three principles:

- Strong evidence is required before a page is flagged.
- Normal manga-story features reduce confidence.
- Borderline pages should be kept.

## Platform Notes

Trimanga's detector, archive reader, archive rewriter, command-line report path, and SDL review launcher are designed to be cross-platform across macOS, Linux, and Windows. It does not require external recognition engines or bundled OCR models.

The native review window is built with SDL2. The launcher looks for a sibling `trimanga-preview` executable on macOS/Linux and `trimanga-preview.exe` on Windows, so packaged builds need to ship both the scanner and preview helper together.

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
