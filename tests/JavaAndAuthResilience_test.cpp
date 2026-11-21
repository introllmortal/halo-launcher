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

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

#include "java/JavaInstall.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/launch/AutoInstallJava.h"

class JavaAndAuthResilienceTest : public QObject {
    Q_OBJECT

   private slots:
    void shouldRevalidateStaleAutoJavaOnMcUpgrade()
    {
        // Manual override (isAutomaticJava=false) stays skipped even if JavaPath isn't a raw file path (e.g. PATH binary)
        QVERIFY(AutoInstallJava::shouldSkipAutoJava(true, true, false, true, QStringLiteral("17.0.10"), { 21 }));
        QVERIFY(AutoInstallJava::shouldSkipAutoJava(true, true, false, false, QStringLiteral("17.0.10"), { 21 }));

        // Previously auto-selected Java 17 is now incompatible with MC 1.21.1 ({21}) -> must NOT skip
        QVERIFY(!AutoInstallJava::shouldSkipAutoJava(true, true, true, true, QStringLiteral("17.0.10"), { 21 }));

        // Auto-selected Java 21 is compatible with {21} -> safe to skip
        QVERIFY(AutoInstallJava::shouldSkipAutoJava(true, true, true, true, QStringLiteral("21.0.4"), { 21 }));

        // Missing binary triggers re-detection even if marked compatible
        QVERIFY(!AutoInstallJava::shouldSkipAutoJava(true, true, true, false, QStringLiteral("21.0.4"), { 21 }));

        // Global auto-switch off always skips
        QVERIFY(AutoInstallJava::shouldSkipAutoJava(false, false, true, false, QStringLiteral("17.0.10"), { 21 }));
    }

    void selectsBest64BitLocalJavaFirst()
    {
        auto makeJava = [](const QString& ver, bool bit64, const QString& path) {
            auto j = std::make_shared<JavaInstall>();
            j->id = JavaVersion(ver);
            j->is_64bit = bit64;
            j->path = path;
            return std::static_pointer_cast<BaseVersion>(j);
        };

        QList<BaseVersion::Ptr> javas = {
            makeJava(QStringLiteral("21.0.1"), false, QStringLiteral("/java21-x86")),
            makeJava(QStringLiteral("17.0.9"), true, QStringLiteral("/java17-x64")),
            makeJava(QStringLiteral("21.0.2"), true, QStringLiteral("/java21-x64-old")),
            makeJava(QStringLiteral("21.0.5"), true, QStringLiteral("/java21-x64-new")),
        };

        auto picked = AutoInstallJava::selectBestLocalJava(javas, { 21, 17 });
        QVERIFY(picked != nullptr);
        QCOMPARE(picked->path, QStringLiteral("/java21-x64-new"));
        QVERIFY(picked->is_64bit);

        // Falls back to 32-bit only when no 64-bit match exists
        QList<BaseVersion::Ptr> only32 = { makeJava(QStringLiteral("8.0.402"), false, QStringLiteral("/java8-x86")) };
        auto picked32 = AutoInstallJava::selectBestLocalJava(only32, { 8 });
        QVERIFY(picked32 != nullptr);
        QCOMPARE(picked32->path, QStringLiteral("/java8-x86"));
    }

    void accountListCreatesBackupAndRecoversWhenCorrupted()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString accountsPath = tempDir.filePath(QStringLiteral("accounts.json"));

        {
            AccountList list;
            list.setListFilePath(accountsPath, false);
            list.addAccount(MinecraftAccount::createOffline(QStringLiteral("TesterOne")));
            QVERIFY(list.saveList());
            // Second save triggers backup creation of previous valid accounts.json
            list.addAccount(MinecraftAccount::createOffline(QStringLiteral("TesterTwo")));
            QVERIFY(list.saveList());
            QVERIFY(QFile::exists(accountsPath + QStringLiteral(".bak")));
        }

        // Corrupt primary accounts.json and verify loadList recovers from .bak
        {
            QFile corrupt(accountsPath);
            QVERIFY(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
            corrupt.write("{not valid json");
        }

        AccountList recovered;
        recovered.setListFilePath(accountsPath, false);
        QVERIFY(recovered.loadList());
        QVERIFY(recovered.count() >= 1);

        // Missing primary accounts.json also recovers cleanly from .bak
        QVERIFY(QFile::remove(accountsPath));
        AccountList recoveredFromMissing;
        recoveredFromMissing.setListFilePath(accountsPath, false);
        QVERIFY(recoveredFromMissing.loadList());
        QVERIFY(recoveredFromMissing.count() >= 1);
    }
};

QTEST_GUILESS_MAIN(JavaAndAuthResilienceTest)

#include "JavaAndAuthResilience_test.moc"
