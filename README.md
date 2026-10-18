# Halo Launcher

<p align="center">
  <img alt="Halo Launcher Logo" src="/program_info/io.github.introllmortal.HaloLauncher_256.png" width="128">
</p>

<p align="center">
  <b>Fast, clean, and customizable Minecraft launcher with native Ely.by & Microsoft account support, modpack management, and seamless updates.</b>
</p>

<p align="center">
  <img alt="License" src="https://img.shields.io/badge/License-GPL--3.0-blue.svg">
  <img alt="Platform" src="https://img.shields.io/badge/Platform-Windows%20x64-green.svg">
  <img alt="Framework" src="https://img.shields.io/badge/Framework-Qt6%20%7C%20C%2B%2B20-orange.svg">
</p>

---

## 🚀 About

**Halo Launcher** is a lightweight, high-performance, and clean Minecraft launcher built on top of Prism Launcher.

It has been stripped of third-party advertisements, sponsor banners, external donation popups, and intrusive news feeds. The launcher is designed for both standalone play and easy distribution among friends and gaming communities, featuring built-in support for custom modpacks and differential updates.

---

## ✨ Features

* **Universal Account Management:**
  * **Ely.by:** Secure OAuth2 authorization with automatic skin and cape rendering across multiplayer servers.
  * **Microsoft (MSA):** Official Minecraft license authentication via Microsoft OAuth2.
  * **Offline Mode:** Instant offline game launch without internet connectivity.
* **Integrated Modpack Updates (GitHub & Custom Providers):**
  * Dedicated update tab in each instance's settings: version selection, formatted changelogs, and one-click updates.
  * Differential downloading: only updated or added mods and configs are downloaded, saving bandwidth and time.
  * **Data Integrity:** player singleplayer worlds (`saves/`), screenshots, and key bindings (`options.txt`) are never overwritten.
* **Clean & Modern User Interface:**
  * Zero server ads, sponsor widgets, or promotional links.
  * Snappy, responsive interface powered by Qt 6.
* **Full Mod Loader Support:**
  * NeoForge, Fabric, Forge, and Quilt.
  * Isolated Java runtimes, fine-grained JVM memory allocation, and custom launch arguments.

---

## 📦 Building from Source

The project builds using **CMake** and **Qt 6**:

### Requirements:
* Windows 10/11 x64
* Visual Studio 2022 (MSVC v143) with C++20 support
* CMake 3.22+
* Qt 6.6+ (Core, Gui, Widgets, Network, Concurrent, Svg, Xml)

Automated builds for release installers are configured in [GitHub Actions](https://github.com/introllmortal/halo-launcher/actions) via the `build.yml` workflow.

---

## 📜 License

* The codebase is licensed under the **GNU General Public License v3.0 (GPL-3.0-only)**.
* Upstream projects: based on [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) and [PineconeMC](https://github.com/ElyPrismLauncher/Launcher).
* Halo Launcher is an independent project and is not affiliated with or endorsed by Mojang Studios, Microsoft, or Prism Launcher.
