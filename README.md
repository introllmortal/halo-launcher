<div align="center">
  <img src="program_info/io.github.introllmortal.HaloLauncher_256.png" alt="Halo Launcher" width="132" />
  <h1>Halo Launcher</h1>
  <p>
    <strong>A standalone, bloat-free Minecraft launcher built with Qt 6 and C++20.</strong><br />
    Modern 3-column dark workspace, software 3D skin preview, smart crash diagnostics, and atomic delta-sync modpack updates.
  </p>

  <p>
    <a href="https://github.com/introllmortal/halo-launcher/releases">
      <img src="https://img.shields.io/github/v/release/introllmortal/halo-launcher?style=flat-square&color=16a34a&label=Release" alt="Latest Release" />
    </a>
    <a href="https://github.com/introllmortal/halo-launcher/actions/workflows/build.yml">
      <img src="https://img.shields.io/github/actions/workflow/status/introllmortal/halo-launcher/build.yml?branch=main&style=flat-square&label=CI%20Build" alt="CI Build" />
    </a>
    <a href="LICENSE">
      <img src="https://img.shields.io/badge/License-GPL--3.0-0ea5e9?style=flat-square" alt="GPL-3.0 License" />
    </a>
    <img src="https://img.shields.io/badge/C%2B%2B20-Qt%206-41cd52?style=flat-square&logo=qt&logoColor=white" alt="C++20 / Qt 6" />
    <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-1e293b?style=flat-square" alt="Platforms" />
  </p>

  <p>
    <a href="#overview">Overview</a> •
    <a href="#key-capabilities">Capabilities</a> •
    <a href="#delta-sync-modpack-updates">Delta Sync</a> •
    <a href="#building-from-source">Build from Source</a> •
    <a href="https://github.com/introllmortal/halo-launcher/releases">Downloads</a>
  </p>
</div>

---

## Overview

**Halo Launcher** is an independent open-source Minecraft launcher and instance manager developed by [@introllmortal](https://github.com/introllmortal).

Designed as a fast, distraction-free desktop application in native **C++20 / Qt 6**, it removes third-party server promotions, donation popups, and legacy toolbar bloat in favor of a clean 3-column dark workspace built for both solo players and modded communities.

---

## Key Capabilities

| Area | What Halo Launcher Provides |
| :--- | :--- |
| **Modern Dark Workspace** | Custom 3-column layout powered by `HaloTheme`: streamlined `LauncherSidebar` navigation, searchable instance grid (`InstanceView`), collapsible seasonal mascot, and right-hand `InstanceInspector` with one-click Play/Stop, Settings, Export, Shortcuts, and Log Sharing. |
| **Smart Crash Assistant** | Native crash diagnostics (`CrashAnalyzer`) classifying Out-of-Memory errors, Java version mismatches, duplicate mods, missing dependencies, corrupted configs, NeoForge/Fabric/Quilt mixin conflicts, and GPU driver crashes — paired with one-click fixes (RAM tuning, Java switch, mod/config/shader reset, dedicated GPU hint) and one-click [`mclo.gs`](https://mclo.gs) log upload. |
| **Atomic Delta Sync** | Content-addressable modpack updater (`DeltaSyncTask`) with SHA-1 file verification, staged downloads, pre-update backup snapshots, and a rollback journal that preserves `saves/`, `screenshots/`, and `options.txt`. |
| **CPU 3D Skin & Cape Viewer** | Custom software 3D rasterizer (`SkinOpenGLWindow` / `BoxGeometry`) with interactive drag orbit, zoom, classic/slim arms, capes, and elytra — engineered to work reliably without OpenGL/ANGLE context failures. |
| **Flexible Accounts** | Native support for **Microsoft (MSA OAuth2)**, **Ely.by (OAuth2 with PKCE + automatic skin/cape patching)**, and **Offline** profiles. |
| **Mod & Loader Ecosystem** | Full support for **NeoForge**, **Forge**, **Fabric**, and **Quilt** alongside direct browsing/updating from **Modrinth** and **CurseForge**, automated Java downloads, and complete **English / Russian** UI localization. |

---

## Delta Sync Modpack Updates

Halo Launcher supports GitHub Releases and custom JSON update manifests directly in each instance's **Managed Pack** tab. Instead of re-downloading multi-gigabyte archives on every patch, `DeltaSyncTask` verifies local file hashes and downloads only modified or new files while safely backing up replaced content.

<details>
<summary><strong>Example <code>updates.json</code> manifest schema</strong></summary>

```json
{
  "name": "Modpack Name",
  "id": "modpack-id",
  "versions": [
    {
      "version": "1.0.0",
      "name": "1.0.0",
      "tag_name": "v1.0.0",
      "released": "2026-10-18T12:00:00Z",
      "changelog": "## Version 1.0.0\n\n- Initial release.\n- Performance tuning and mod updates.",
      "url": "https://github.com/owner/modpack/releases/download/v1.0.0/modpack-1.0.0.zip",
      "delta_manifest_url": "https://github.com/owner/modpack/releases/download/v1.0.0/delta_manifest.json"
    }
  ]
}
```

Maintainers distributing custom modpacks can automate packaging, delta manifest generation, and GitHub Release publishing with [`scripts/publish_modpack.ps1`](scripts/publish_modpack.ps1).

</details>

---

## Building from Source

### Prerequisites

- **C++20 compiler**: MSVC (Visual Studio 2022/2026, amd64), Clang, or GCC
- **CMake 3.25+** (`3.28+` when using `CMakePresets.json`) & **Ninja**
- **Extra CMake Modules (ECM)**
- **Qt 6.8+** (`Core`, `Widgets`, `Concurrent`, `Network`, `NetworkAuth`, `Xml`, `Test`, plus `qtimageformats` / `Svg`)
- **JDK 17** (required to compile `libraries/launcher` and `libraries/javacheck`)
- **vcpkg** on Windows MSVC (managed via `vcpkg.json` for `cmark`, `libarchive`, `libqrencode`, `tomlplusplus`, `zlib`)

### Clone & Build

```bash
git clone --recursive https://github.com/introllmortal/halo-launcher.git
cd halo-launcher
```

#### Windows (MSVC + vcpkg)

```powershell
# Using CMake Presets (requires VCPKG_ROOT and Qt 6 on PATH / CMAKE_PREFIX_PATH)
cmake --preset windows_msvc
cmake --build --preset windows_msvc --config Release --parallel
cmake --install build --config Release
```

#### Linux

```bash
cmake --preset linux
cmake --build --preset linux --config Release --parallel
```

#### Running Tests

On Windows, ensure Qt 6 and vcpkg runtime DLLs are on `PATH` before invoking `ctest` (otherwise test binaries fail with `STATUS_DLL_NOT_FOUND` / `0xc0000135`):

```powershell
# Windows (PowerShell)
$env:PATH = "C:\Qt\<version>\msvc2022_64\bin;$PWD\build\vcpkg_installed\x64-windows\bin;$env:PATH"
ctest --test-dir build --build-config Release --output-on-failure
```

```bash
# Linux
ctest --test-dir build --output-on-failure
```

Automated release builds for Windows and Linux are configured in [`.github/workflows/build.yml`](.github/workflows/build.yml) and [`.github/workflows/release.yml`](.github/workflows/release.yml).

---

## Repository Structure

```text
├── buildconfig/    # Compile-time branding, version, and API metadata templates
├── cmake/          # Find modules and build helpers
├── launcher/       # Core C++/Qt 6 application (UI, diagnostics, delta sync, Minecraft/auth logic)
├── libraries/      # Java launch wrappers and vendored C/C++ dependencies
├── program_info/   # Application icons, manifests, NSIS installer script, desktop/metainfo files
├── scripts/        # Maintenance & modpack publishing scripts
└── tests/          # Unit tests (CTest / QTest, including CrashAnalyzer, DeltaSync, SkinRasterizer)
```

---

## License & Acknowledgments

- **License:** Distributed under the **GNU General Public License v3.0 ([GPL-3.0-only](LICENSE))**. See [COPYING.md](COPYING.md) for third-party notices.
- **Maintainer:** [introllmortal](https://github.com/introllmortal)
- **Upstream Heritage:** Halo Launcher is an independent derivative project built upon the open-source foundation of [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher), [PineconeMC](https://github.com/ElyPrismLauncher/Launcher), and MultiMC. It is not affiliated with or endorsed by Mojang Studios, Microsoft, or Prism Launcher.
