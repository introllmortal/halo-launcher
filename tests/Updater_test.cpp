// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Halo Launcher - Minecraft Launcher
 *  Copyright (C) 2026 introllmortal <introllmortal@users.noreply.github.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <QTest>
#include <QTranslator>

#include "ui/dialogs/ReleaseNotesDialog.h"
#include "updater/prismupdater/UpdaterArtifacts.h"

static void initHaloTranslationsForTest()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        Q_INIT_RESOURCE(halo_translations);
    }
}

namespace {
GitHubRelease makeCanonicalHaloRelease(const QString& version)
{
    GitHubRelease release;
    release.id = 42;
    release.tag_name = QStringLiteral("v") + version;
    release.name = QStringLiteral("Halo Launcher v") + version;

    auto addAsset = [&release](int id, const QString& name) {
        GitHubReleaseAsset a;
        a.id = id;
        a.name = name;
        a.browser_download_url = QStringLiteral("https://example.invalid/") + name;
        release.assets.append(a);
    };

    // 4 canonical release assets matching .github/workflows/release.yml
    addAsset(1, QStringLiteral("HaloLauncher-Windows-MSVC-Setup-%1.exe").arg(version));
    addAsset(2, QStringLiteral("HaloLauncher-Windows-MSVC-Portable-%1.zip").arg(version));
    addAsset(3, QStringLiteral("HaloLauncher-Linux-%1-x86_64.AppImage").arg(version));
    addAsset(4, QStringLiteral("HaloLauncher-Linux-Portable-%1.tar.gz").arg(version));

    // Noise / extra artifacts that must always be rejected
    addAsset(5, QStringLiteral("HaloLauncher-Linux-%1-x86_64.AppImage.zsync").arg(version));
    addAsset(6, QStringLiteral("HaloLauncher-macOS-arm64-%1.zip").arg(version));
    addAsset(7, QStringLiteral("HaloLauncher-Windows-MSVC-arm64-%1.zip").arg(version));
    addAsset(8, QStringLiteral("HaloLauncher-Linux-arm64-%1-Qt5.tar.gz").arg(version));
    return release;
}
}  // namespace

class UpdaterTest : public QObject {
    Q_OBJECT

   private slots:
    void matchesAllFourCanonicalReleaseArtifacts()
    {
        const GitHubRelease release = makeCanonicalHaloRelease(QStringLiteral("2.1.0"));

        // 1. Windows Installer (MSVC Qt6 x86_64)
        {
            HaloUpdater::ArtifactTarget target;
            target.buildArtifact = QStringLiteral("Windows-MSVC-Qt6");
            target.cpuArchitecture = QStringLiteral("x86_64");
            target.isPortable = false;
            target.isAppimage = false;

            const auto matches = HaloUpdater::validReleaseArtifacts(release, target);
            QCOMPARE(matches.size(), 1);
            QCOMPARE(matches.first().name, QStringLiteral("HaloLauncher-Windows-MSVC-Setup-2.1.0.exe"));
        }

        // 2. Windows Portable (MSVC Qt6 x86_64)
        {
            HaloUpdater::ArtifactTarget target;
            target.buildArtifact = QStringLiteral("Windows-MSVC-Qt6");
            target.cpuArchitecture = QStringLiteral("x86_64");
            target.isPortable = true;
            target.isAppimage = false;

            const auto matches = HaloUpdater::validReleaseArtifacts(release, target);
            QCOMPARE(matches.size(), 1);
            QCOMPARE(matches.first().name, QStringLiteral("HaloLauncher-Windows-MSVC-Portable-2.1.0.zip"));
        }

        // 3. Linux AppImage (Qt6 x86_64)
        {
            HaloUpdater::ArtifactTarget target;
            target.buildArtifact = QStringLiteral("Linux-Qt6");
            target.cpuArchitecture = QStringLiteral("x86_64");
            target.isPortable = false;
            target.isAppimage = true;

            const auto matches = HaloUpdater::validReleaseArtifacts(release, target);
            QCOMPARE(matches.size(), 1);
            QCOMPARE(matches.first().name, QStringLiteral("HaloLauncher-Linux-2.1.0-x86_64.AppImage"));
        }

        // 4. Linux Portable tarball (Qt6 x86_64)
        {
            HaloUpdater::ArtifactTarget target;
            target.buildArtifact = QStringLiteral("Linux-Qt6");
            target.cpuArchitecture = QStringLiteral("x86_64");
            target.isPortable = true;
            target.isAppimage = false;

            const auto matches = HaloUpdater::validReleaseArtifacts(release, target);
            QCOMPARE(matches.size(), 1);
            QCOMPARE(matches.first().name, QStringLiteral("HaloLauncher-Linux-Portable-2.1.0.tar.gz"));
        }
    }

    void rejectsMismatchedQtLegacyArchAndEmptyArtifact()
    {
        GitHubRelease release;
        release.id = 99;
        release.tag_name = QStringLiteral("v2.1.0");

        GitHubReleaseAsset legacyWin;
        legacyWin.id = 1;
        legacyWin.name = QStringLiteral("HaloLauncher-Windows-Legacy-Portable-2.1.0.zip");
        release.assets.append(legacyWin);

        GitHubReleaseAsset qt5Asset;
        qt5Asset.id = 2;
        qt5Asset.name = QStringLiteral("HaloLauncher-Linux-Portable-Qt5-2.1.0.tar.gz");
        release.assets.append(qt5Asset);

        GitHubReleaseAsset zsyncOnly;
        zsyncOnly.id = 3;
        zsyncOnly.name = QStringLiteral("HaloLauncher-Linux-2.1.0-x86_64.AppImage.zsync");
        release.assets.append(zsyncOnly);

        HaloUpdater::ArtifactTarget target;
        target.buildArtifact = QStringLiteral("Windows-MSVC-Qt6");
        target.cpuArchitecture = QStringLiteral("x86_64");
        target.isPortable = true;
        QVERIFY(HaloUpdater::validReleaseArtifacts(release, target).isEmpty());

        target.buildArtifact = QStringLiteral("Linux-Qt6");
        target.isPortable = true;
        QVERIFY(HaloUpdater::validReleaseArtifacts(release, target).isEmpty());

        target.buildArtifact.clear();
        QVERIFY(HaloUpdater::validReleaseArtifacts(release, target).isEmpty());
    }

    void parsesReleaseNotesAndChecksPromptCondition()
    {
        const QByteArray payload = QByteArrayLiteral(
            "{"
            "\"tag_name\": \"v2.1.0\","
            "\"name\": \"Halo Launcher 2.1.0\","
            "\"body\": \"### Highlights\\n- Pack snapshots\\n- Pre-flight check\","
            "\"html_url\": \"https://github.com/introllmortal/halo-launcher/releases/tag/v2.1.0\","
            "\"published_at\": \"2026-11-16T20:00:00Z\","
            "\"draft\": false"
            "}");

        ReleaseNotesDialog::ReleaseInfo info;
        QVERIFY(ReleaseNotesDialog::parseLatestRelease(payload, info));
        QCOMPARE(info.tagName, QStringLiteral("v2.1.0"));
        QCOMPARE(info.title, QStringLiteral("Halo Launcher 2.1.0"));
        QVERIFY(info.publishedAt.isValid());

        QVERIFY(ReleaseNotesDialog::shouldAutoPrompt(info.tagName, QStringLiteral("2.1.0"), QString()));
        QVERIFY(ReleaseNotesDialog::shouldAutoPrompt(info.tagName, QStringLiteral("2.1.0"), QStringLiteral("2.0.0")));
        QVERIFY(!ReleaseNotesDialog::shouldAutoPrompt(info.tagName, QStringLiteral("2.1.0"), QStringLiteral("2.1.0")));
        QVERIFY(!ReleaseNotesDialog::shouldAutoPrompt(QStringLiteral("v2.0.0"), QStringLiteral("2.1.0"), QString()));
    }

    void loadsRussianHaloTranslationResource()
    {
        initHaloTranslationsForTest();

        QTranslator uiTranslator;
        QVERIFY(uiTranslator.load(QStringLiteral(":/i18n/halo_ui_ru.qm")));
        QVERIFY(!uiTranslator.isEmpty());

        QCOMPARE(uiTranslator.translate("InstanceInspector", "Rollback Update"), QStringLiteral("Откатить обновление"));
        QCOMPARE(uiTranslator.translate("LauncherSidebar", "Quick Play"), QStringLiteral("Быстрый запуск"));
        QCOMPARE(uiTranslator.translate("MainWindow", "What's New..."), QStringLiteral("Что нового..."));

        QTranslator dialogsTranslator;
        QVERIFY(dialogsTranslator.load(QStringLiteral(":/i18n/halo_dialogs_ru.qm")));
        QVERIFY(!dialogsTranslator.isEmpty());

        QCOMPARE(dialogsTranslator.translate("LaunchPreflightCheck", "Configured Java executable was not found"),
                 QStringLiteral("Указанный исполняемый файл Java не найден"));
        QCOMPARE(dialogsTranslator.translate("ReleaseNotesDialog", "Open on GitHub"), QStringLiteral("Открыть на GitHub"));
        QCOMPARE(dialogsTranslator.translate("QObject", "Halo Dark"), QStringLiteral("Halo Тёмная"));
        QCOMPARE(dialogsTranslator.translate("AboutDialog", "Credits"), QStringLiteral("Авторы"));
    }
};

QTEST_GUILESS_MAIN(UpdaterTest)

#include "Updater_test.moc"
