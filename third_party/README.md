# Third-Party Code

Trimanga vendors small source dependencies only when they keep installation simple.

## stb

- Path: `third_party/stb/stb_image.h`
- Upstream: https://github.com/nothings/stb
- Purpose: portable image decoding for the built-in detector
- License: public domain or MIT, as stated in the header

This lets Trimanga decode common manga image formats without platform-specific image frameworks or external recognition runtimes.

## miniz

- Path: `third_party/miniz`
- Upstream: https://github.com/richgel999/miniz
- Purpose: built-in `.zip` and `.cbz` extraction/rebuilding
- License: MIT, as stated in the source headers

This lets Trimanga scan and rewrite manga archives without shelling out to `zip`, `unzip`, `7z`, or platform archive tools.
