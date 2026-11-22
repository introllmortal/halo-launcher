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

#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>
#include <zlib.h>

#include "FileSystem.h"
#include "tasks/DeltaSyncTask.h"
#include "tasks/ManagedFilesIndex.h"
#include "tasks/ZipDeltaDownloader.h"
#include "tasks/ZipDeltaPlanner.h"
#include "tasks/ZipRangeUtils.h"

namespace {

struct ArchiveInputEntry {
    QString rawPath;
    QByteArray payload;
    bool compressDeflate = true;
};

QByteArray deflateRaw(const QByteArray& input)
{
    if (input.isEmpty()) {
        return {};
    }
    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return {};
    }
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.constData()));
    stream.avail_in = static_cast<uInt>(input.size());
    QByteArray output(static_cast<qsizetype>(deflateBound(&stream, static_cast<uLong>(input.size()))), '\0');
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    const int rc = deflate(&stream, Z_FINISH);
    const qsizetype produced = static_cast<qsizetype>(stream.total_out);
    deflateEnd(&stream);
    if (rc != Z_STREAM_END) {
        return {};
    }
    output.truncate(produced);
    return output;
}

void appendU16(QByteArray& out, quint16 value)
{
    char buf[2];
    qToLittleEndian<quint16>(value, buf);
    out.append(buf, 2);
}

void appendU32(QByteArray& out, quint32 value)
{
    char buf[4];
    qToLittleEndian<quint32>(value, buf);
    out.append(buf, 4);
}

QByteArray buildTestZip(const QList<ArchiveInputEntry>& entries)
{
    QByteArray archiveBytes;
    QByteArray centralDir;

    for (const auto& entry : entries) {
        const QByteArray nameBytes = entry.rawPath.toUtf8();
        const quint32 crc = ZipRangeUtils::computeBytesCrc32(entry.payload);
        const quint16 method = (entry.compressDeflate && !entry.payload.isEmpty()) ? 8 : 0;
        const QByteArray compressed = (method == 8) ? deflateRaw(entry.payload) : entry.payload;
        const quint32 localOffset = static_cast<quint32>(archiveBytes.size());

        // Local file header (30 bytes)
        appendU32(archiveBytes, 0x04034B50u);
        appendU16(archiveBytes, 20);
        appendU16(archiveBytes, 0x0800);
        appendU16(archiveBytes, method);
        appendU16(archiveBytes, 0);
        appendU16(archiveBytes, 0);
        appendU32(archiveBytes, crc);
        appendU32(archiveBytes, static_cast<quint32>(compressed.size()));
        appendU32(archiveBytes, static_cast<quint32>(entry.payload.size()));
        appendU16(archiveBytes, static_cast<quint16>(nameBytes.size()));
        appendU16(archiveBytes, 0);
        archiveBytes.append(nameBytes);
        archiveBytes.append(compressed);

        // Central directory header (46 bytes)
        appendU32(centralDir, 0x02014B50u);
        appendU16(centralDir, 20);
        appendU16(centralDir, 20);
        appendU16(centralDir, 0x0800);
        appendU16(centralDir, method);
        appendU16(centralDir, 0);
        appendU16(centralDir, 0);
        appendU32(centralDir, crc);
        appendU32(centralDir, static_cast<quint32>(compressed.size()));
        appendU32(centralDir, static_cast<quint32>(entry.payload.size()));
        appendU16(centralDir, static_cast<quint16>(nameBytes.size()));
        appendU16(centralDir, 0);
        appendU16(centralDir, 0);
        appendU16(centralDir, 0);
        appendU16(centralDir, 0);
        appendU32(centralDir, 0);
        appendU32(centralDir, localOffset);
        centralDir.append(nameBytes);
    }

    const quint32 cdOffset = static_cast<quint32>(archiveBytes.size());
    archiveBytes.append(centralDir);

    // EOCD (22 bytes)
    appendU32(archiveBytes, 0x06054B50u);
    appendU16(archiveBytes, 0);
    appendU16(archiveBytes, 0);
    appendU16(archiveBytes, static_cast<quint16>(entries.size()));
    appendU16(archiveBytes, static_cast<quint16>(entries.size()));
    appendU32(archiveBytes, static_cast<quint32>(centralDir.size()));
    appendU32(archiveBytes, cdOffset);
    appendU16(archiveBytes, 0);
    return archiveBytes;
}

class LocalRangeServer : public QTcpServer {
   public:
    explicit LocalRangeServer(QByteArray archive, bool supportRange)
        : m_archive(std::move(archive)), m_supportRange(supportRange)
    {}

    int rangeRequestCount() const { return m_rangeRequests; }

   protected:
    void incomingConnection(qintptr socketDescriptor) override
    {
        auto* socket = new QTcpSocket(this);
        if (!socket->setSocketDescriptor(socketDescriptor)) {
            socket->deleteLater();
            return;
        }
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            const QByteArray requestBytes = socket->readAll();
            if (!requestBytes.contains("\r\n\r\n")) {
                return;
            }
            const QString reqText = QString::fromLatin1(requestBytes);
            QRegularExpression rangeRe(QStringLiteral(R"(Range:\s*bytes=(\d+)-(\d+))"), QRegularExpression::CaseInsensitiveOption);
            const auto match = rangeRe.match(reqText);

            if (!m_supportRange || !match.hasMatch()) {
                QByteArray headers;
                headers += "HTTP/1.1 200 OK\r\n";
                headers += "Content-Type: application/zip\r\n";
                headers += "Content-Length: " + QByteArray::number(m_archive.size()) + "\r\n";
                headers += "Connection: close\r\n\r\n";
                socket->write(headers);
                socket->write(m_archive);
                socket->disconnectFromHost();
                return;
            }

            ++m_rangeRequests;
            const qint64 start = match.captured(1).toLongLong();
            const qint64 requestedEnd = match.captured(2).toLongLong();
            const qint64 total = m_archive.size();
            const qint64 end = qMin(requestedEnd, total - 1);
            const QByteArray slice = (start >= 0 && start <= end) ? m_archive.mid(start, end - start + 1) : QByteArray();

            QByteArray headers;
            headers += "HTTP/1.1 206 Partial Content\r\n";
            headers += "Content-Type: application/zip\r\n";
            headers += "Accept-Ranges: bytes\r\n";
            headers += "Content-Range: bytes " + QByteArray::number(start) + "-" + QByteArray::number(end) + "/" +
                       QByteArray::number(total) + "\r\n";
            headers += "Content-Length: " + QByteArray::number(slice.size()) + "\r\n";
            headers += "Connection: close\r\n\r\n";
            socket->write(headers);
            socket->write(slice);
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }

   private:
    QByteArray m_archive;
    bool m_supportRange = true;
    int m_rangeRequests = 0;
};

void writeLocalFile(const QString& root, const QString& relPath, const QByteArray& bytes)
{
    const QString fullPath = FS::PathCombine(root, relPath);
    FS::ensureFilePathExists(fullPath);
    QFile file(fullPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), bytes.size());
    file.close();
}

QByteArray readLocalFile(const QString& root, const QString& relPath)
{
    QFile file(FS::PathCombine(root, relPath));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

}  // namespace

class ZipRangeDeltaTest : public QObject {
    Q_OBJECT

   private slots:
    void testNormalizeAndCoalesce()
    {
        QCOMPARE(ZipRangeUtils::normalizePackRelativePath(".minecraft/mods/create.jar"), QString("mods/create.jar"));
        QCOMPARE(ZipRangeUtils::normalizePackRelativePath("Pack-1.0/.minecraft/config/a.toml"), QString("config/a.toml"));
        QCOMPARE(ZipRangeUtils::normalizePackRelativePath("overrides/kubejs/server.js"), QString("kubejs/server.js"));
        QCOMPARE(ZipRangeUtils::normalizePackRelativePath(".minecraft/mods/"), QString());
        QVERIFY(!DeltaSyncTask::isPathSafe(ZipRangeUtils::normalizePackRelativePath(".minecraft/options.txt")));

        QList<ZipRemoteEntry> entries;
        ZipRemoteEntry first;
        first.rawName = "mods/a.jar";
        first.normalizedPath = "mods/a.jar";
        first.localHeaderOffset = 0;
        first.compressedSize = 100;
        entries.append(first);

        ZipRemoteEntry second;
        second.rawName = "mods/b.jar";
        second.normalizedPath = "mods/b.jar";
        second.localHeaderOffset = 1000;
        second.compressedSize = 200;
        entries.append(second);

        ZipRemoteEntry farEntry;
        farEntry.rawName = "mods/c.jar";
        farEntry.normalizedPath = "mods/c.jar";
        farEntry.localHeaderOffset = 500000;
        farEntry.compressedSize = 300;
        entries.append(farEntry);

        const auto ranges = ZipRangeUtils::coalesceByteRanges(entries, 1000000, 65536, 4 * 1024 * 1024);
        QCOMPARE(ranges.size(), 2);
        QCOMPARE(ranges[0].entryIndices.size(), 2);
        QCOMPARE(ranges[1].entryIndices.size(), 1);
    }

    void testPatchedModsRangeUpdateAndCleanup()
    {
        QTemporaryDir instDir;
        QVERIFY(instDir.isValid());
        const QString instanceRoot = instDir.path();
        const QString gameRoot = FS::PathCombine(instanceRoot, ".minecraft");

        const QByteArray unchangedBytes("unchanged-mod-binary-content-123456");
        const QByteArray oldLargerBytes(100, 'L');
        const QByteArray newLargerBytes(145, 'L');
        const QByteArray oldSmallerBytes(100, 'S');
        const QByteArray newSmallerBytes(64, 'S');
        const QByteArray oldSameSizeBytes(100, 'A');
        const QByteArray newSameSizeBytes(100, 'B');
        const QByteArray renamedOldBytes("old-renamed-mod");
        const QByteArray renamedNewBytes("new-renamed-mod-v2");
        const QByteArray playerModBytes("player-personal-mod");
        const QByteArray optionsBytes("fov:90\n");

        writeLocalFile(gameRoot, "mods/unchanged.jar", unchangedBytes);
        writeLocalFile(gameRoot, "mods/patched_larger.jar", oldLargerBytes);
        writeLocalFile(gameRoot, "mods/patched_smaller.jar", oldSmallerBytes);
        writeLocalFile(gameRoot, "mods/patched_same_size.jar", oldSameSizeBytes);
        writeLocalFile(gameRoot, "mods/renamed_old-1.0.jar", renamedOldBytes);
        writeLocalFile(gameRoot, "mods/player_personal.jar", playerModBytes);
        writeLocalFile(gameRoot, "options.txt", optionsBytes);

        // Save managed index representing version 1.0 (excludes player_personal.jar and options.txt)
        ManagedFilesManifest oldIndex;
        oldIndex.version = "1.0";
        for (const QString& rel : { "mods/unchanged.jar", "mods/patched_larger.jar", "mods/patched_smaller.jar",
                                    "mods/patched_same_size.jar", "mods/renamed_old-1.0.jar" }) {
            ManagedFileRecord rec;
            rec.path = rel;
            const QByteArray fileBytes = readLocalFile(gameRoot, rel);
            rec.size = static_cast<qint64>(fileBytes.size());
            rec.crc32 = ZipRangeUtils::computeBytesCrc32(fileBytes);
            oldIndex.files.append(rec);
        }
        QVERIFY(ManagedFilesIndex::save(instanceRoot, oldIndex));

        // Build remote 2.0 ZIP with .minecraft/ prefix and mixed Store + Deflate
        const QByteArray zipBytes = buildTestZip({
            { ".minecraft/mods/unchanged.jar", unchangedBytes, true },
            { ".minecraft/mods/patched_larger.jar", newLargerBytes, true },
            { ".minecraft/mods/patched_smaller.jar", newSmallerBytes, false },
            { ".minecraft/mods/patched_same_size.jar", newSameSizeBytes, true },
            { ".minecraft/mods/renamed_new-2.0.jar", renamedNewBytes, true },
            { ".minecraft/config/empty.cfg", QByteArray(), false },
            { ".minecraft/options.txt", QByteArray("fov:70\n"), true },
        });

        LocalRangeServer server(zipBytes, true);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QUrl archiveUrl(QString("http://127.0.0.1:%1/Create_Clockwork_Empire-2.0.zip").arg(server.serverPort()));

        QNetworkAccessManager nam;
        ZipDeltaPlanTask planTask(&nam, archiveUrl, instanceRoot, gameRoot, "2.0");
        QEventLoop loop;
        connect(&planTask, &Task::finished, &loop, &QEventLoop::quit);
        planTask.start();
        loop.exec();

        QVERIFY2(planTask.wasSuccessful(), qPrintable(planTask.failReason()));
        const ZipDeltaPlan& plan = planTask.plan();
        QVERIFY(plan.valid);
        QCOMPARE(plan.skippedCount, 1);
        QCOMPARE(plan.entriesToDownload.size(), 5);
        QCOMPARE(plan.pathsToRemove, QStringList{ "mods/renamed_old-1.0.jar" });
        QVERIFY(server.rangeRequestCount() >= 3);

        // Download & extract staged chunks
        QTemporaryDir stagingDir;
        QVERIFY(stagingDir.isValid());
        ZipDeltaDownloadTask downloadTask(&nam, plan, stagingDir.path());
        QEventLoop dlLoop;
        connect(&downloadTask, &Task::finished, &dlLoop, &QEventLoop::quit);
        downloadTask.start();
        dlLoop.exec();

        QVERIFY2(downloadTask.wasSuccessful(), qPrintable(downloadTask.failReason()));
        QCOMPARE(readLocalFile(stagingDir.path(), "mods/patched_larger.jar"), newLargerBytes);
        QCOMPARE(readLocalFile(stagingDir.path(), "mods/patched_smaller.jar"), newSmallerBytes);
        QCOMPARE(readLocalFile(stagingDir.path(), "mods/patched_same_size.jar"), newSameSizeBytes);
        QCOMPARE(readLocalFile(stagingDir.path(), "mods/renamed_new-2.0.jar"), renamedNewBytes);
        QVERIFY(QFileInfo::exists(FS::PathCombine(stagingDir.path(), "config/empty.cfg")));

        // Verify player personal mod & options.txt were never scheduled for removal or overwrite
        QCOMPARE(readLocalFile(gameRoot, "mods/player_personal.jar"), playerModBytes);
        QCOMPARE(readLocalFile(gameRoot, "options.txt"), optionsBytes);
    }

    void testRange200Fallback()
    {
        const QByteArray zipBytes = buildTestZip({
            { ".minecraft/mods/example.jar", QByteArray("hello-mod"), true },
        });
        LocalRangeServer server(zipBytes, false);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QUrl archiveUrl(QString("http://127.0.0.1:%1/pack.zip").arg(server.serverPort()));

        QTemporaryDir instDir;
        QVERIFY(instDir.isValid());
        QNetworkAccessManager nam;
        ZipDeltaPlanTask planTask(&nam, archiveUrl, instDir.path(), FS::PathCombine(instDir.path(), ".minecraft"), "2.0");
        QEventLoop loop;
        connect(&planTask, &Task::finished, &loop, &QEventLoop::quit);
        planTask.start();
        loop.exec();

        QVERIFY(!planTask.wasSuccessful());
        QVERIFY(planTask.plan().rangeNotSupported);
    }

    void testUnixSymlinkEntrySkipped()
    {
        QByteArray centralDir;
        const QByteArray nameBytes(".minecraft/mods/evil_link.jar");
        appendU32(centralDir, 0x02014B50u);
        appendU16(centralDir, (3u << 8) | 20u);  // Unix made-by
        appendU16(centralDir, 20u);
        appendU16(centralDir, 0x0800u);
        appendU16(centralDir, 0u);
        appendU16(centralDir, 0u);
        appendU16(centralDir, 0u);
        appendU32(centralDir, 0u);
        appendU32(centralDir, 10u);
        appendU32(centralDir, 10u);
        appendU16(centralDir, static_cast<quint16>(nameBytes.size()));
        appendU16(centralDir, 0u);
        appendU16(centralDir, 0u);
        appendU16(centralDir, 0u);
        appendU16(centralDir, 0u);
        appendU32(centralDir, 0xA1FFu << 16);  // S_IFLNK
        appendU32(centralDir, 0u);
        centralDir.append(nameBytes);

        bool parsedOk = false;
        const auto entries = ZipRangeUtils::parseCentralDirectory(centralDir, &parsedOk);
        QVERIFY(parsedOk);
        QVERIFY(entries.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ZipRangeDeltaTest)
#include "ZipRangeDelta_test.moc"
