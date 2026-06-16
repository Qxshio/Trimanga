# OCR Backends

Trimanga is designed so OCR backends can be swapped without changing the scanner pipeline.

## Current Backends

### Apple Vision

Used on macOS when available. This is the best current backend for Trimanga because it is native, fast, private to the OS, and does not require a bundled OCR dependency.

Apple Vision cannot be forked into Trimanga. It is a closed-source Apple framework backed by private system models and runtime components. Trimanga can call it through the public Vision API, but cannot redistribute or modify the implementation.

### Tesseract

Used as the portable fallback. Tesseract can be accurate, but it is slow and can be resource-heavy. Trimanga caps Tesseract's internal OpenMP thread count on POSIX systems so it does not multiply CPU usage inside Trimanga's own worker pool.

Use fast mode to reduce Tesseract work:

```sh
trimanga scan "./Volume.cbz" --ocr tesseract --ocr-speed fast
```

### None

Disables OCR:

```sh
trimanga scan "./Volume.cbz" --ocr none
```

This has no OCR dependency and is the lightest mode, but it only catches visual/statistical outliers and repeated pages seeded by non-text signals. It will miss many credit pages that require reading text.

## Realistic Alternatives

### Windows Native OCR

Windows has a built-in OCR API through `Windows.Media.Ocr`. A future C++/WinRT backend would avoid Tesseract on Windows and keep Trimanga dependency-light.

### Linux OCR

Linux does not have a universal built-in OCR framework equivalent to Apple Vision. Practical options are:

- Keep Tesseract as an optional fallback.
- Add an optional ONNX Runtime backend using a compact OCR model.
- Add distro packages for an OCR model/runtime pair.

An ONNX backend would still be a dependency, but it can be faster and more controllable than spawning Tesseract processes.

### Custom OCR

Building a high-quality OCR engine inside Trimanga is not realistic for this project. OCR requires text detection, orientation handling, recognition models, language handling, training data, and platform-specific acceleration. The maintainable approach is to keep a clean backend interface and use native OCR where possible.
