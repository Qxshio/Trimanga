# Architecture

Trimanga is intentionally split into small, replaceable pieces:

- `IPageDetector`: returns cleanup-oriented text signals for each page.
- `image_loader`: decodes supported images through the vendored portable image decoder.
- `page_features`: loads image dimensions, approximate visual statistics, and a perceptual hash.
- `scanlation_detector`: extracts cleanup signals from page layout.
- `classifier`: converts detector signals plus page features into a conservative cleanup recommendation.
- `scanner`: orchestrates archive extraction, feature profiling, parallel page analysis, classification, and second-pass visual matching.
- `file_utils` and `process`: platform utilities kept out of domain logic.

The second-pass visual matcher only uses high-confidence pages as seeds. That keeps repeated clutter pages detectable without letting weak detector noise cascade into large false-positive runs.
