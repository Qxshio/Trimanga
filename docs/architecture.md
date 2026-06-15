# Architecture

The project is intentionally split into small, replaceable pieces:

- `IOcrBackend`: chooses Apple Vision or Tesseract and returns page text.
- `image_features`: loads image dimensions, approximate visual statistics, and a perceptual hash.
- `classifier`: converts OCR text plus page features into a conservative scanlation/ad decision.
- `scanner`: orchestrates archive extraction, feature profiling, parallel OCR, classification, and second-pass visual matching.
- `file_utils` and `process`: platform utilities kept out of domain logic.

The second-pass visual matcher only uses high-confidence pages as seeds. That keeps repeated scanlation pages detectable without letting weak OCR noise cascade into large false-positive runs.
