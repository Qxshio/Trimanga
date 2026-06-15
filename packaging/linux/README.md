# Linux Packaging

Linux packages are generated with CPack from the top-level CMake project.

```sh
sudo apt-get install -y cmake g++ tesseract-ocr unzip
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target package
```

Expected package outputs on Linux:

- `scanlation-tool-0.1.0-Linux.tar.gz`
- `.deb`
- `.rpm`

The generated `.deb` declares `tesseract-ocr` and `unzip` as runtime dependencies.
The generated `.rpm` declares `tesseract` and `unzip`.
