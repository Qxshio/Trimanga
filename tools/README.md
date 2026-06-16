# Tools

Developer utilities for tuning Trimanga.

## layout_probe

`layout_probe.cpp` prints the layout metrics used by the built-in detector. It is useful when tuning rules against real scanlation/ad pages and nearby story pages.

Build manually from the project root:

```sh
c++ -std=c++20 -Iinclude -Ithird_party/stb \
  tools/layout_probe.cpp src/image/image_loader.cpp \
  -o /tmp/layout_probe
```

Example:

```sh
/tmp/layout_probe /path/to/page001.jpg /path/to/page002.png
```
