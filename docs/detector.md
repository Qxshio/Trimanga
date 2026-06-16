# Detector

Trimanga uses one built-in detector across macOS, Linux and Windows.

The detector is specialized for manga cleanup. Instead of trying to read every word on a page, it looks for the visual structures that usually matter for archive cleanup:

- centered credit blocks
- sparse notice pages
- recruitment-card layouts
- support or purchase reminder cards
- repeated release bumpers
- pages that are strong outliers compared with the rest of the volume

This keeps Trimanga fast, dependency-free and predictable. The tradeoff is that the detector is intentionally narrower than a full text-recognition engine. It should be tuned against real scanlation/ad pages and false positives over time.

## Design Goals

- No platform-specific recognition dependency.
- No command-line recognition dependency.
- No bundled recognition model.
- Same behavior on all supported platforms.
- Strong bias toward keeping story pages.
- Review-first output instead of destructive cleanup.
