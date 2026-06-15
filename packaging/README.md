# Packaging

This directory contains packaging templates for common Trimanga distribution channels.

The project can build native release archives with CPack:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --build build --target package
```

Publishing notes:

- Homebrew requires a public release tarball and a SHA-256 checksum.
- winget requires versioned installer/archive URLs and hashes.
- Scoop requires a JSON manifest in a bucket repository.
- Chocolatey requires packaging the executable or downloading a release archive from `tools/chocolateyinstall.ps1`.
- Linux packages are generated through CPack as `.tar.gz`, `.deb`, and `.rpm` on Linux.

Replace placeholder checksums and release versions before publishing.
