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

## 🚀 О проекте / About

**Halo Launcher** — это легковесный, производительный и чистый лаунчер Minecraft, созданный на базе Prism Launcher. 

Лаунчер полностью очищен от рекламы, сторонних баннеров, донатов и лишних новостных лент. Он идеально подходит как для одиночной игры, так и для распространения среди друзей и игровых сообществ благодаря встроенной поддержке пользовательских сборок и дифференциальных обновлений.

---

## ✨ Ключевые возможности / Features

* **Универсальная поддержка аккаунтов:**
  * **Ely.by:** Официальная авторизация OAuth2, автоматическая загрузка скинов и плащей на серверах.
  * **Microsoft (MSA):** Официальная лицензия Minecraft через защищенный Microsoft OAuth2.
  * **Офлайн-режим:** Быстрый запуск игры без подключения к интернету.
* **Встроенный менеджер обновлений (Halo Pack):**
  * Нативная вкладка обновления в настройках каждого инстанса: выбор версий, чейнджлог и обновление в один клик.
  * Дифференциальная загрузка: скачиваются только измененные или добавленные моды/конфиги.
  * **100% сохранность пользовательских данных:** миры (`saves/`), скриншоты и настройки управления (`options.txt`) всегда остаются нетронутыми.
* **Чистый и быстрый интерфейс:**
  * Без рекламы, партнерских виджетов и стороннего визуального шума.
  * Быстрый отклик интерфейса на базе Qt 6.
* **Поддержка всех актуальных модлоадеров:**
  * NeoForge, Fabric, Forge, Quilt.
  * Изолированные профили Java, гибкое распределение оперативной памяти и аргументов JVM.

---

## 📦 Сборка из исходников / Building

Проект собирается с использованием **CMake** и **Qt 6**:

### Требования:
* Windows 10/11 x64
* Visual Studio 2022 (MSVC v143) с поддержкой C++20
* CMake 3.22+
* Qt 6.6+ (Core, Gui, Widgets, Network, Concurrent, Svg, Xml)

Автоматическая компиляция релизных установщиков настроена в [GitHub Actions](https://github.com/introllmortal/halo-launcher/actions) через запуск workflow `build.yml`.

---

## 📜 Лицензия / License

* Кодовая база распространяется на условиях свободной лицензии **GNU General Public License v3.0 (GPL-3.0-only)**.
* Исходный проект: форк [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) и [PineconeMC](https://github.com/ElyPrismLauncher/Launcher).
* Halo Launcher является независимым проектом и не связан с Mojang Studios, Microsoft или Prism Launcher.
