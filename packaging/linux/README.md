# Linux Packaging

Linux packages are generated with CPack from the top-level CMake project.

```sh
sudo apt-get install -y cmake g++ unzip
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target package
```

Expected package outputs on Linux:

- `trimanga-0.1.0-Linux.tar.gz`
- `.deb`
- `.rpm`

The generated `.deb` and `.rpm` packages declare `unzip` as the only runtime extraction dependency.
