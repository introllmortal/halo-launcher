// SPDX-License-Identifier: GPL-3.0-only
#include "CrashAssistantTranslator.h"

#include <QCoreApplication>
#include <QHash>
#include <QLocale>

#include "Application.h"
#include "settings/SettingsObject.h"
#include "translations/TranslationsModel.h"

namespace {
static const QHash<QByteArray, const char*> s_ruTranslations = {
    // ==========================================
    // === CrashAnalyzer Context              ===
    // ==========================================

    // --- Out of Memory ---
    { "CrashAnalyzer::Insufficient Memory (Out of Memory)",
      "Нехватка оперативной памяти (OutOfMemoryError)" },
    { "CrashAnalyzer::Minecraft ran out of allocated RAM and terminated (%1).\nCurrently allocated: %2 MB. This modpack requires more memory to run smoothly.",
      "В Minecraft закончилась выделенная оперативная память (Java heap) во время игры (%1).\nТекущий объем: %2 МБ. Данной сборке требуется больше памяти для стабильной работы." },
    { "CrashAnalyzer::Increase allocated memory to %1 MB in the launcher instance settings.",
      "Увеличьте объем выделенной памяти до %1 МБ в настройках сборки." },
    { "CrashAnalyzer::Allocate %1 MB RAM",
      "Выделить %1 МБ RAM" },

    // --- Java Version ---
    { "CrashAnalyzer::Incompatible Java Version",
      "Несовместимая версия Java" },
    { "CrashAnalyzer::A mod was compiled for Java %1 (class version %2), but Minecraft was launched with an older or incompatible Java runtime.",
      "Мод был скомпилирован для Java %1 (версия классов %2), но Minecraft запущен на несовместимой версии Java." },
    { "CrashAnalyzer::Switch the instance Java installation to Java %1 in instance settings.",
      "Переключите используемую версию Java на Java %1 в настройках инстанса: Настройки -> Java." },
    { "CrashAnalyzer::Switch to Java %1",
      "Переключить на Java %1" },

    // --- Duplicate Mods ---
    { "CrashAnalyzer::Duplicate Mod Detected",
      "Обнаружены дубликаты модов" },
    { "CrashAnalyzer::Minecraft found multiple copies or conflicting versions of the same mod in your mods folder:\n%1",
      "В папке mods найдено несколько версий или копий одного и того же мода:\n%1" },
    { "CrashAnalyzer::Multiple duplicate mod files",
      "Несколько дублирующихся файлов модов" },
    { "CrashAnalyzer::Remove or disable one of the duplicate mod files.",
      "Удалите или отключите одну из дублирующихся версий мода в папке mods." },
    { "CrashAnalyzer::Disable %1",
      "Отключить %1" },

    // --- Missing Dependency ---
    { "CrashAnalyzer::Missing Required Mod (Dependency)",
      "Отсутствует необходимый мод (зависимость)" },
    { "CrashAnalyzer::Mod '%1' requires '%2' to be installed, but it is missing or incompatible.",
      "Мод '%1' требует установки мода '%2', но он отсутствует или несовместим." },
    { "CrashAnalyzer::Install the required mod ('%1') into your mods folder or update the modpack.",
      "Установите требуемый мод ('%1') в папку mods или обновите сборку." },
    { "CrashAnalyzer::The mod '%1' is required but is missing or incompatible.",
      "Требуется мод '%1', однако он не найден или несовместим." },
    { "CrashAnalyzer::Install '%1' into your mods folder or update the modpack.",
      "Установите '%1' в папку mods или обновите сборку." },
    { "CrashAnalyzer::One or more mods are missing their mandatory library dependencies:\n%1",
      "У одного или нескольких модов отсутствуют обязательные библиотеки-зависимости:\n%1" },
    { "CrashAnalyzer::Install the missing dependency mod into the mods folder or update the modpack.",
      "Установите недостающие моды-зависимости в папку mods или обновите сборку." },
    { "CrashAnalyzer::Upload Log to mclo.gs",
      "Загрузить лог на mclo.gs" },

    // --- Corrupted Config ---
    { "CrashAnalyzer::Corrupted Configuration File",
      "Поврежден файл конфигурации" },
    { "CrashAnalyzer::The configuration file '%1' contains invalid syntax or has been corrupted.",
      "Файл конфигурации '%1' поврежден или содержит синтаксические ошибки." },
    { "CrashAnalyzer::Reset or delete the corrupted configuration file so Minecraft can recreate a clean default one.",
      "Сбросьте или удалите поврежденный файл конфигурации, чтобы игра создала чистый дефолтный конфиг при запуске." },
    { "CrashAnalyzer::Reset Config File",
      "Сбросить файл конфига" },

    // --- Mod Conflict ---
    { "CrashAnalyzer::Mod Crash / Conflict Detected",
      "Обнаружен конфликт или сбой мода" },
    { "CrashAnalyzer::A game crash occurred caused by mod:\n%1\nThe mod encountered an internal error or conflicted with another installed mod.",
      "Произошел сбой игры, вызванный модом:\n%1\nМод вызвал внутреннюю ошибку или конфликтует с другим установленным модом." },
    { "CrashAnalyzer::Temporarily disable or update this mod to resolve the crash.",
      "Временно отключите или обновите этот мод для устранения сбоя." },

    // --- GPU & Shaders ---
    { "CrashAnalyzer::Hybrid GPU Detected (Integrated Graphics in Use)",
      "Обнаружена гибридная графика (используется встроенная видеокарта)" },
    { "CrashAnalyzer::Minecraft failed to communicate with your graphics processor (%1).\nYour system has hybrid graphics: dedicated (%2) and integrated (%3).\nThe game appears to be running on the integrated graphics card instead of the dedicated GPU.",
      "Minecraft не смог корректно инициализировать видеокарту (%1).\nВ вашей системе гибридная графика: дискретная (%2) и встроенная (%3).\nИгра была запущена на встроенной видеокарте вместо производительной дискретной." },
    { "CrashAnalyzer::Click 'Enforce Dedicated GPU' below to register high-performance GPU preference for Java in Windows, then relaunch Minecraft.",
      "Нажмите кнопку «Назначить дискретную видеокарту» ниже, чтобы зарегистрировать режим высокой производительности для Java в Windows, затем перезапустите Minecraft." },
    { "CrashAnalyzer::Enforce Dedicated GPU",
      "Назначить дискретную видеокарту" },

    { "CrashAnalyzer::Shader Compilation Failure",
      "Сбой компиляции шейдеров" },
    { "CrashAnalyzer::Minecraft crashed while compiling or linking visual shaders (%1).\nThis usually happens due to syntax errors or hardware incompatibility in active shader packs.",
      "Minecraft аварийно завершился при компиляции шейдеров (%1).\nОбычно это происходит из-за синтаксических ошибок или несовместимости активного шейдерпака с вашей видеокартой." },
    { "CrashAnalyzer::Disable active shader packs by clicking 'Disable Shaders' below, or update Iris/Oculus.",
      "Отключите активные шейдеры, нажав кнопку «Отключить шейдеры» ниже, или обновите Iris/Oculus." },
    { "CrashAnalyzer::Disable Shaders",
      "Отключить шейдеры" },

    { "CrashAnalyzer::Graphics Driver Error",
      "Сбой графического драйвера" },
    { "CrashAnalyzer::Minecraft crashed inside the NVIDIA graphics driver (%1).\nThis indicates a driver crash or incompatibility with the installed NVIDIA driver version.",
      "Minecraft аварийно завершился внутри видеодрайвера NVIDIA (%1).\nЭто указывает на сбой драйвера или несовместимость установленной версии драйверов NVIDIA." },
    { "CrashAnalyzer::Perform a clean reinstall or update your NVIDIA GeForce drivers from nvidia.com.",
      "Выполните чистую переустановку или обновите драйверы NVIDIA GeForce с сайта nvidia.com." },
    { "CrashAnalyzer::Minecraft crashed due to a graphics driver or OpenGL error (%1).",
      "Minecraft аварийно завершился из-за ошибки графического драйвера или OpenGL (%1)." },
    { "CrashAnalyzer::Update your graphics card drivers and make sure your GPU supports OpenGL 4.5+.",
      "Обновите драйверы видеокарты и убедитесь, что ваш графический процессор поддерживает OpenGL 4.5+." },

    // --- Fallback / Unknown Crash ---
    { "CrashAnalyzer::Unexpected Game Crash",
      "Непредвиденное завершение игры" },
    { "CrashAnalyzer::Minecraft closed unexpectedly (Exit code %1).\nRoot cause:\n%2",
      "Minecraft неожиданно закрылся (код выхода %1).\nПричина сбоя:\n%2" },
    { "CrashAnalyzer::Minecraft terminated unexpectedly with exit code %1.\nThe specific cause could not be determined automatically.",
      "Minecraft неожиданно завершился с кодом выхода %1.\nТочную причину сбоя не удалось определить автоматически." },
    { "CrashAnalyzer::Upload the crash log to generate a clean shareable link for server administrators or developers.",
      "Загрузите лог краша на mclo.gs, чтобы получить удобную ссылку для отправки разработчикам или администраторам." },

    // ==========================================
    // === CrashAssistantDialog Context       ===
    // ==========================================

    // Headers & Labels
    { "CrashAssistantDialog::Halo Crash Assistant",
      "Краш-ассистент Halo" },
    { "CrashAssistantDialog::Halo Launcher analyzed the game crash and determined the cause:",
      "Halo Launcher проанализировал сбой игры и определил причину:" },
    { "CrashAssistantDialog::💡 Recommended Solution:",
      "💡 Рекомендуемое решение:" },
    { "CrashAssistantDialog::Technical Details:",
      "Технические подробности:" },

    // Buttons
    { "CrashAssistantDialog::Upload Log to mclo.gs",
      "Загрузить лог на mclo.gs" },
    { "CrashAssistantDialog::Copy Details",
      "Скопировать отчет" },
    { "CrashAssistantDialog::Cancel",
      "Закрыть" },
    { "CrashAssistantDialog::Safe Mode",
      "Безопасный режим" },
    { "CrashAssistantDialog::Temporarily reset active shaders, resource packs, and JVM overrides to recover from crashes.",
      "Временно сбросить активные шейдеры, ресурспаки и пользовательские аргументы JVM для выхода из цикла сбоев." },
    { "CrashAssistantDialog::Uploading...",
      "Загрузка..." },
    { "CrashAssistantDialog::Link Copied!",
      "Ссылка скопирована!" },
    { "CrashAssistantDialog::Minecraft Crash Log",
      "Лог краша Minecraft" },
    { "CrashAssistantDialog::Copied",
      "Скопировано" },
    { "CrashAssistantDialog::Crash diagnostic report has been copied to your clipboard.",
      "Диагностический отчет скопирован в буфер обмена." },

    // Dialog messages
    { "CrashAssistantDialog::RAM Updated",
      "Выделение памяти обновлено" },
    { "CrashAssistantDialog::Allocated RAM has been increased to %1 MB!\nYou can now relaunch Minecraft.",
      "Выделенная память увеличена до %1 МБ!\nТеперь вы можете перезапустить Minecraft." },
    { "CrashAssistantDialog::Mod Disabled",
      "Мод отключен" },
    { "CrashAssistantDialog::Conflicting mod has been disabled:\n%1\nYou can now relaunch Minecraft.",
      "Конфликтующий мод отключен:\n%1\nТеперь вы можете перезапустить Minecraft." },
    { "CrashAssistantDialog::Error",
      "Ошибка" },
    { "CrashAssistantDialog::Could not disable mod file. Please check file permissions.",
      "Не удалось отключить файл мода. Проверьте права доступа к файлу." },
    { "CrashAssistantDialog::Config Reset",
      "Конфигурация сброшена" },
    { "CrashAssistantDialog::The corrupted config file has been backed up and reset.\nMinecraft will recreate a clean default config upon next launch.",
      "Поврежденный файл конфигурации скопирован в бэкап и сброшен.\nMinecraft создаст чистый конфиг по умолчанию при следующем запуске." },
    { "CrashAssistantDialog::Java Setting",
      "Настройка Java" },
    { "CrashAssistantDialog::Please configure Java %1 in instance settings under Settings -> Java.",
      "Пожалуйста, настройте Java %1 в параметрах сборки: Настройки -> Java." },
    { "CrashAssistantDialog::Shaders Disabled",
      "Шейдеры отключены" },
    { "CrashAssistantDialog::Active shaders have been reset to prevent graphics driver crashes.",
      "Активные шейдеры сброшены для предотвращения сбоев графического драйвера." },
    { "CrashAssistantDialog::GPU Preference Set",
      "Назначена дискретная видеокарта" },
    { "CrashAssistantDialog::High-Performance GPU preference (GpuPreference=2) has been successfully registered for Java in Windows!\nYou can now relaunch Minecraft.",
      "Режим высокой производительности (GpuPreference=2) успешно зарегистрирован для Java в Windows!\nТеперь вы можете перезапустить Minecraft на дискретной видеокарте." },
    { "CrashAssistantDialog::Failed to Set GPU Preference",
      "Не удалось назначить видеокарту" },
    { "CrashAssistantDialog::Could not apply GPU preference to Windows registry.\nPlease check that a valid Java executable is configured.",
      "Не удалось записать настройки видеокарты в реестр Windows.\nУбедитесь, что указан корректный путь к Java." },
    { "CrashAssistantDialog::Warning",
      "Предупреждение" },
    { "CrashAssistantDialog::Instance is no longer available.",
      "Сборка больше недоступна." },
    { "CrashAssistantDialog::Safe Mode Prepared",
      "Безопасный режим применен" },
    { "CrashAssistantDialog::Shaders, resource packs, and custom Java parameters have been safely reset.\nYou can now relaunch Minecraft.",
      "Шейдеры, ресурспаки и пользовательские аргументы JVM сброшены.\nТеперь вы можете перезапустить Minecraft." },
    { "CrashAssistantDialog::Empty Log",
      "Пустой лог" },
    { "CrashAssistantDialog::No game log output is available to upload.",
      "Лог игры пуст, нет данных для выгрузки." },

    // ==========================================
    // === MainWindow Context                 ===
    // ==========================================
    { "MainWindow::Share Latest Log (mclo.gs)",
      "Поделиться последним логом (mclo.gs)" },
    { "MainWindow::Share the latest game log via mclo.gs",
      "Опубликовать последний лог игры через mclo.gs" },
    { "MainWindow::Log Not Found",
      "Лог не найден" },
    { "MainWindow::No latest.log was found for \"%1\".",
      "Файл latest.log не найден для сборки «%1»." },
    { "MainWindow::Read Error",
      "Ошибка чтения" },
    { "MainWindow::Could not open latest.log for reading.",
      "Не удалось открыть latest.log для чтения." },
    { "MainWindow::Empty Log",
      "Пустой лог" },
    { "MainWindow::The latest.log file is empty.",
      "Файл latest.log пуст." }
};
}  // namespace

CrashAssistantTranslator::CrashAssistantTranslator(QObject* parent)
    : QTranslator(parent)
{
}

bool CrashAssistantTranslator::isRussianLanguage()
{
    if (APPLICATION) {
        if (APPLICATION->translations()) {
            QString lang = APPLICATION->translations()->selectedLanguage();
            if (!lang.isEmpty()) {
                return lang.startsWith("ru", Qt::CaseInsensitive);
            }
        }
        if (APPLICATION->settings()) {
            QString lang = APPLICATION->settings()->get("Language").toString();
            if (!lang.isEmpty()) {
                return lang.startsWith("ru", Qt::CaseInsensitive);
            }
        }
    }

    const QString sysLocale = QLocale::system().name();
    return sysLocale.startsWith("ru", Qt::CaseInsensitive) || QLocale().language() == QLocale::Russian;
}

QString CrashAssistantTranslator::translate(const char* context, const char* sourceText,
                                            [[maybe_unused]] const char* disambiguation,
                                            [[maybe_unused]] int n) const
{
    if (!context || !sourceText) {
        return QString();
    }

    // Fast filter: only process contexts managed by CrashAssistantTranslator
    if (context[0] != 'C' && context[0] != 'M') {
        return QString();
    }

    QByteArray key = QByteArray(context) + "::" + sourceText;
    auto it = s_ruTranslations.find(key);
    if (it == s_ruTranslations.end()) {
        return QString();
    }

    // Only query locale when the key actually matches one of our strings
    if (!isRussianLanguage()) {
        return QString();
    }

    return QString::fromUtf8(it.value());
}

void CrashAssistantTranslator::install()
{
    if (!QCoreApplication::instance()) {
        return;
    }
    static CrashAssistantTranslator* s_instance = []() {
        auto* translator = new CrashAssistantTranslator(QCoreApplication::instance());
        QCoreApplication::installTranslator(translator);
        return translator;
    }();
    Q_UNUSED(s_instance);
}
