# Halo Launcher

<p align="center">
  <img alt="Halo Launcher Logo" src="/program_info/io.github.introllmortal.HaloLauncher_256.png" width="128">
</p>

<p align="center">
  <b>Кастомный, производительный и чистый лаунчер Minecraft с нативной поддержкой Ely.by, лицензий Microsoft и системы обновления серверных сборок.</b>
</p>

<p align="center">
  <img alt="License" src="https://img.shields.io/badge/License-GPL--3.0-blue.svg">
  <img alt="Platform" src="https://img.shields.io/badge/Platform-Windows%20x64-green.svg">
  <img alt="Framework" src="https://img.shields.io/badge/Framework-Qt6%20%7C%20C%2B%2B20-orange.svg">
</p>

---

## 🚀 О проекте

**Halo Launcher** — это форк Prism Launcher, созданный специально для комфортной приватной игры с друзьями на сервере с тяжелыми модпаками (включая флагманскую сборку **Create: Clockwork Empire** на NeoForge 1.21.1).

Лаунчер избавлен от визуального мусора, партнерских рекламных модулей и лишних внешних ссылок, сохраняя при этом всю мощь и гибкость Prism Launcher.

---

## ✨ Ключевые возможности

* **Полная свобода аккаунтов:**
  * **Ely.by:** Официальная авторизация OAuth2, автоматическая загрузка скинов и плащей на серверах.
  * **Microsoft (MSA):** Официальная лицензия Minecraft через Microsoft OAuth2.
  * **Офлайн-режим:** Возможность быстрого входа без интернета.
* **Нативная система обновления сборок (Halo Pack):**
  * Встроенная в настройки инстанса вкладка обновлений (выпадающий список версий, подробный чейнджлог, кнопка обновления).
  * Дифференциальное скачивание: обновляются только новые/измененные моды и конфиги.
  * **100% сохранность данных:** ваши одиночные миры (`saves/`), скриншоты и настройки клавиш (`options.txt`) никогда не затираются при обновлениях.
* **Чистый интерфейс:**
  * Никакой рекламы серверов, донатов, сторонних Discord-ботов и новостных лент.
  * Быстрый и плавный запуск на базе Qt6.
* **Поддержка всех современных загрузчиков:**
  * NeoForge, Fabric, Forge, Quilt.
  * Изолированное управление Java, тонкая настройка аргументов JVM и оптимизация потребления памяти.

---

## 📦 Сборка и разработка

Проект собирается с использованием **CMake** и **Qt 6**:

### Требования для сборки:
* Windows 10/11 x64
* Visual Studio 2022 (MSVC v143) с поддержкой C++20
* CMake 3.22+
* Qt 6.6+ (Core, Gui, Widgets, Network, Concurrent, Svg, Xml)

Сборка релизных установщиков автоматически настроена в [GitHub Actions](https://github.com/introllmortal/halo-launcher/actions) через запуск workflow `build.yml`.

---

## 📜 Лицензия и условия

* Кодовая база распространяется на условиях лицензии **GNU General Public License v3.0 (GPL-3.0-only)**.
* Исходный проект: основан на коде [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) и [PineconeMC](https://github.com/ElyPrismLauncher/Launcher).
* Данный проект является независимым и не связан с Mojang Studios, Microsoft или Prism Launcher.
