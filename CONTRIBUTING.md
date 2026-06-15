# Contributing

Trimanga is built around a conservative cleanup model: it should be useful without surprising people or damaging their libraries.

## Development Principles

- Preserve story pages unless there is strong evidence they are removable.
- Prefer reviewable output over destructive automation.
- Avoid manga-specific hardcoding. Detection rules should generalize across publishers, groups, languages, and layouts.
- Keep platform integrations isolated behind small modules.
- Keep CLI output stable enough for scripts.

## Local Workflow

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

On macOS without CMake:

```sh
make
tests/smoke.sh
```

## Pull Request Checklist

- The project builds locally.
- Smoke tests or CTest pass.
- User-facing CLI behavior is documented.
- Detection changes explain expected false-positive and false-negative impact.
- New packaging metadata uses the Trimanga name consistently.
