# Contributing to Halo Launcher

Thank you for your interest in improving **Halo Launcher**.

## Before You Start

- Search [existing issues](https://github.com/introllmortal/halo-launcher/issues) before opening a new report or pull request.
- Keep pull requests focused on a single bug fix, UI polish, or feature.
- Do not submit unreviewed or auto-generated code that you cannot explain or test locally.

## Code Style & Quality

- **C++20 / Qt 6** codebase formatted according to [`.clang-format`](.clang-format) (`clang-format`).
- Naming conventions:
  - Types & classes: `PascalCase` (`InstanceInspector`)
  - Member variables: `m_camelCase` (`m_instance`); static members: `s_camelCase`
  - Methods, functions, variables: `camelCase`
  - Constants & macros: `SCREAMING_SNAKE_CASE`
- Preserve existing GPL-3.0 SPDX headers and add standard Halo Launcher copyright headers to new source files.

## Verification Checklist

Before opening a pull request:

1. Build `HaloLauncher` cleanly in `Release` or `Debug` without introducing compiler warnings.
2. Run the unit test suite:
   ```bash
   ctest --test-dir build --build-config Release --output-on-failure
   ```
3. Format modified C++ sources with `clang-format`.
