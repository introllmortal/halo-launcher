// SPDX-License-Identifier: GPL-3.0-only

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "FileSystem.h"
#include "tasks/DeltaSyncTask.h"

class DeltaSyncTest : public QObject {
    Q_OBJECT

   private slots:
    void testPathSecurity()
    {
        QVERIFY(DeltaSyncTask::isPathSafe("mods/create.jar"));
        QVERIFY(DeltaSyncTask::isPathSafe("config/create-client.toml"));
        QVERIFY(DeltaSyncTask::isPathSafe("kubejs/server_scripts/recipes.js"));

        QVERIFY(!DeltaSyncTask::isPathSafe(""));
        QVERIFY(!DeltaSyncTask::isPathSafe("../evil.jar"));
        QVERIFY(!DeltaSyncTask::isPathSafe("mods/../../evil.jar"));
        QVERIFY(!DeltaSyncTask::isPathSafe("/mods/create.jar"));
        QVERIFY(!DeltaSyncTask::isPathSafe("C:/mods/create.jar"));
        QVERIFY(!DeltaSyncTask::isPathSafe("options.txt"));
        QVERIFY(!DeltaSyncTask::isPathSafe("instance.cfg"));
        QVERIFY(!DeltaSyncTask::isPathSafe("saves/world/level.dat"));
        QVERIFY(!DeltaSyncTask::isPathSafe("patches/custom.json"));
        QVERIFY(!DeltaSyncTask::isPathSafe("mods/"));
    }

    void testParseManifestFormatsAndDeduplication()
    {
        QJsonObject manifest{
            { "files", QJsonArray{
                           QJsonObject{ { "path", "mods/a.jar" }, { "sha1", "aaa" }, { "size", 10 }, { "url", "https://example/a" } },
                           QJsonObject{ { "file", "mods/a.jar" }, { "hash", "bbb" }, { "fileSize", 20 }, { "downloadUrl", "https://example/b" } },
                       } },
            { "removals", QJsonArray{
                              QString("mods/old.jar"),
                              QJsonObject{ { "path", "config/obsolete.toml" }, { "action", "delete" } },
                          } },
        };

        const auto items = DeltaSyncTask::parseManifestItems(manifest);
        QCOMPARE(items.size(), 3);
        QCOMPARE(items[0].path, QString("mods/a.jar"));
        QCOMPARE(items[0].sha1, QString("bbb"));
        QCOMPARE(items[0].size, 20);
        QCOMPARE(items[0].action, DeltaFileItem::Action::Update);

        QCOMPARE(items[1].path, QString("mods/old.jar"));
        QCOMPARE(items[1].action, DeltaFileItem::Action::Remove);
        QCOMPARE(items[2].path, QString("config/obsolete.toml"));
        QCOMPARE(items[2].action, DeltaFileItem::Action::Remove);
    }

    void testFilterUnchangedUpdatesSkipsMatchingSha1()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QByteArray payloadA("unchanged-mod-bytes");
        const QString sha1A = QString::fromLatin1(QCryptographicHash::hash(payloadA, QCryptographicHash::Sha1).toHex());
        const QString pathA = FS::PathCombine(tempDir.path(), "mods/unchanged.jar");
        FS::ensureFilePathExists(pathA);
        QFile fileA(pathA);
        QVERIFY(fileA.open(QIODevice::WriteOnly));
        QCOMPARE(fileA.write(payloadA), payloadA.size());
        fileA.close();

        const QByteArray payloadB("old-mod-bytes");
        const QString sha1NewB = QString::fromLatin1(QCryptographicHash::hash("new-mod-bytes", QCryptographicHash::Sha1).toHex());
        const QString pathB = FS::PathCombine(tempDir.path(), "mods/changed.jar");
        QFile fileB(pathB);
        QVERIFY(fileB.open(QIODevice::WriteOnly));
        QCOMPARE(fileB.write(payloadB), payloadB.size());
        fileB.close();

        QList<DeltaFileItem> items = {
            { "mods/unchanged.jar", sha1A.toUpper(), payloadA.size(), "https://example/unchanged.jar", DeltaFileItem::Action::Update },
            { "mods/changed.jar", sha1NewB, 13, "https://example/changed.jar", DeltaFileItem::Action::Update },
            { "mods/missing.jar", sha1A, payloadA.size(), "https://example/missing.jar", DeltaFileItem::Action::Update },
            { "mods/old.jar", "", 0, "", DeltaFileItem::Action::Remove },
        };

        const int skipped = DeltaSyncTask::filterUnchangedUpdates(tempDir.path(), items);
        QCOMPARE(skipped, 1);
        QCOMPARE(items.size(), 3);
        QCOMPARE(items[0].path, QString("mods/changed.jar"));
        QCOMPARE(items[1].path, QString("mods/missing.jar"));
        QCOMPARE(items[2].path, QString("mods/old.jar"));
    }
};

QTEST_GUILESS_MAIN(DeltaSyncTest)
#include "DeltaSync_test.moc"
