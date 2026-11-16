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

#include <QAbstractListModel>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include <utility>

#include "InstanceList.h"
#include "NullInstance.h"
#include "diagnostics/ProcessMemory.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/INISettingsObject.h"
#include "ui/instanceview/InstanceProxyModel.h"
#include "ui/instanceview/InstanceView.h"

namespace {
std::unique_ptr<INISettingsObject> makeGlobalSettings(const QString& path)
{
    auto settings = std::make_unique<INISettingsObject>(path);
    settings->registerSetting("ShowGameTime", true);
    settings->registerSetting("RecordGameTime", true);
    settings->registerSetting("PreLaunchCommand", "");
    settings->registerSetting("WrapperCommand", "");
    settings->registerSetting("PostExitCommand", "");
    settings->registerSetting("ShowConsole", false);
    settings->registerSetting("AutoCloseConsole", false);
    settings->registerSetting("ShowConsoleOnError", true);
    settings->registerSetting("EnableCrashAssistant", true);
    settings->registerSetting("LogPrePostOutput", true);
    settings->registerSetting("ConsoleMaxLines", 10000);
    settings->registerSetting("ConsoleOverflowStop", false);
    return settings;
}

class TestInstanceSourceModel final : public QAbstractListModel {
   public:
    struct Entry {
        std::shared_ptr<BaseInstance> inst;
        QString fallbackName;
        QString group;
    };

    explicit TestInstanceSourceModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    void add(std::shared_ptr<BaseInstance> inst, QString group = QStringLiteral("Default"))
    {
        const int row = m_entries.size();
        beginInsertRows({}, row, row);
        BaseInstance* raw = inst.get();
        m_entries.append({ std::move(inst), QString(), std::move(group) });
        endInsertRows();
        if (raw) {
            connect(raw, &BaseInstance::propertiesChanged, this, [this, raw]() {
                for (int i = 0; i < m_entries.size(); ++i) {
                    if (m_entries.at(i).inst.get() == raw) {
                        const QModelIndex idx = index(i, 0);
                        emit dataChanged(idx, idx);
                        break;
                    }
                }
            });
            connect(raw, &BaseInstance::runningStatusChanged, this, [this, raw](bool) {
                for (int i = 0; i < m_entries.size(); ++i) {
                    if (m_entries.at(i).inst.get() == raw) {
                        const QModelIndex idx = index(i, 0);
                        emit dataChanged(idx, idx);
                        break;
                    }
                }
            });
        }
    }

    void addNullRow(QString displayName, QString group = QStringLiteral("Default"))
    {
        const int row = m_entries.size();
        beginInsertRows({}, row, row);
        m_entries.append({ nullptr, std::move(displayName), std::move(group) });
        endInsertRows();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_entries.size();
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
            return {};
        }
        const Entry& entry = m_entries.at(index.row());
        switch (role) {
            case Qt::DisplayRole:
                return entry.inst ? entry.inst->name() : entry.fallbackName;
            case Qt::DecorationRole:
                return entry.inst ? entry.inst->iconKey() : QStringLiteral("default");
            case InstanceViewRoles::GroupRole:
                return entry.group;
            case InstanceList::InstancePointerRole:
                return entry.inst ? QVariant::fromValue<void*>(entry.inst.get()) : QVariant();
            default:
                return {};
        }
    }

   private:
    QList<Entry> m_entries;
};
}  // namespace

class InstanceProxyModelTest : public QObject {
    Q_OBJECT

   private slots:
    void parseLinuxVmRss()
    {
        const QByteArray sample =
            "Name:\tjava\n"
            "State:\tS (sleeping)\n"
            "VmPeak:\t 4194304 kB\n"
            "VmRSS:\t 1572864 kB\n"
            "Threads:\t42\n";
        const auto rss = ProcessMemory::parseLinuxProcStatusVmRssBytes(sample);
        QVERIFY(rss.has_value());
        QCOMPARE(*rss, 1572864LL * 1024LL);
        QCOMPARE(ProcessMemory::formatMemorySize(*rss), QStringLiteral("1.50 GB"));
        QCOMPARE(ProcessMemory::formatMemorySize(512LL * 1024LL * 1024LL), QStringLiteral("512 MB"));

        QVERIFY(!ProcessMemory::parseLinuxProcStatusVmRssBytes("Name:\tjava\n").has_value());
        QVERIFY(!ProcessMemory::parseLinuxProcStatusVmRssBytes("VmRSS:\tinvalid kB\n").has_value());
    }

    void detectLoaderFromComponents()
    {
        QCOMPARE(InstanceProxyModel::detectLoaderFromComponentUids({ QStringLiteral("net.minecraft") }),
                 InstanceProxyModel::LoaderFilter::Vanilla);
        QCOMPARE(InstanceProxyModel::detectLoaderFromComponentUids(
                     { QStringLiteral("net.minecraft"), QStringLiteral("net.neoforge") }),
                 InstanceProxyModel::LoaderFilter::NeoForge);
        QCOMPARE(InstanceProxyModel::detectLoaderFromComponentUids(
                     { QStringLiteral("net.minecraft"), QStringLiteral("net.minecraftforge") }),
                 InstanceProxyModel::LoaderFilter::Forge);
        QCOMPARE(InstanceProxyModel::detectLoaderFromComponentUids(
                     { QStringLiteral("net.minecraft"), QStringLiteral("net.fabricmc.fabric-loader") }),
                 InstanceProxyModel::LoaderFilter::Fabric);
        QCOMPARE(InstanceProxyModel::detectLoaderFromComponentUids(
                     { QStringLiteral("net.minecraft"), QStringLiteral("org.quiltmc.quilt-loader") }),
                 InstanceProxyModel::LoaderFilter::Quilt);
        QCOMPARE(InstanceProxyModel::detectLoaderFromComponentUids({}), InstanceProxyModel::LoaderFilter::All);
    }

    void dynamicPinResortAndRunningFilter()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(tempDir.filePath(QStringLiteral("global.cfg")));
        auto makeNullInst = [&](const QString& id, const QString& name) {
            const QString root = tempDir.filePath(id);
            QDir().mkpath(root);
            auto instSettings = std::make_unique<INISettingsObject>(QDir(root).filePath(QStringLiteral("instance.cfg")));
            auto inst = std::make_shared<NullInstance>(global.get(), std::move(instSettings), root);
            inst->setName(name);
            return inst;
        };

        auto alpha = makeNullInst(QStringLiteral("alpha"), QStringLiteral("Alpha Pack"));
        auto zeta = makeNullInst(QStringLiteral("zeta"), QStringLiteral("Zeta Pack"));
        auto beta = makeNullInst(QStringLiteral("beta"), QStringLiteral("Beta Pack"));

        TestInstanceSourceModel source;
        source.add(alpha);
        source.add(zeta);
        source.add(beta);

        InstanceProxyModel proxy;
        proxy.setSourceModel(&source);
        proxy.setDynamicSortFilter(true);
        proxy.sort(0);

        QCOMPARE(proxy.rowCount(), 3);
        QCOMPARE(proxy.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Alpha Pack"));

        // Toggling pin dynamically must re-sort Zeta ahead of Alpha via dataChanged
        zeta->setPinned(true);
        QCOMPARE(proxy.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Zeta Pack"));
        QCOMPARE(proxy.index(1, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Alpha Pack"));

        zeta->setPinned(false);
        QCOMPARE(proxy.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Alpha Pack"));

        // Running-only filter reacts dynamically to runningStatusChanged -> dataChanged
        proxy.setRunningOnly(true);
        QVERIFY(proxy.hasActiveFilter());
        QCOMPARE(proxy.rowCount(), 0);

        beta->setRunning(true);
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Beta Pack"));

        beta->setRunning(false);
        QCOMPARE(proxy.rowCount(), 0);
    }

    void loaderFilteringAndSearchMetadata()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(tempDir.filePath(QStringLiteral("global.cfg")));
        auto makeMcInst = [&](const QString& id, const QString& name, const QString& loaderUid, SettingsObject** rawOut = nullptr) {
            const QString root = tempDir.filePath(id);
            QDir().mkpath(root);
            auto instSettings = std::make_unique<INISettingsObject>(QDir(root).filePath(QStringLiteral("instance.cfg")));
            if (rawOut) {
                *rawOut = instSettings.get();
            }
            auto inst = std::make_shared<MinecraftInstance>(global.get(), std::move(instSettings), root);
            inst->setName(name);
            auto* profile = inst->getPackProfile();
            profile->buildingFromScratch();
            if (!profile->installEmpty(QStringLiteral("net.minecraft"), QStringLiteral("Minecraft"))) {
                return std::shared_ptr<MinecraftInstance>{};
            }
            if (!loaderUid.isEmpty() && !profile->installEmpty(loaderUid, loaderUid)) {
                return std::shared_ptr<MinecraftInstance>{};
            }
            return inst;
        };

        auto vanilla = makeMcInst(QStringLiteral("mc-vanilla"), QStringLiteral("Pure World"), {});
        SettingsObject* neoRawSettings = nullptr;
        auto neoforge = makeMcInst(QStringLiteral("mc-neo"), QStringLiteral("Tech Server"), QStringLiteral("net.neoforge"), &neoRawSettings);
        neoRawSettings->set("ManagedPack", true);
        neoRawSettings->set("ManagedPackName", QStringLiteral("Clockwork Empire"));
        auto fabric = makeMcInst(QStringLiteral("mc-fab"), QStringLiteral("Sodium Light"), QStringLiteral("net.fabricmc.fabric-loader"));

        QCOMPARE(InstanceProxyModel::detectLoader(vanilla.get()), InstanceProxyModel::LoaderFilter::Vanilla);
        QCOMPARE(InstanceProxyModel::detectLoader(neoforge.get()), InstanceProxyModel::LoaderFilter::NeoForge);
        QCOMPARE(InstanceProxyModel::detectLoader(fabric.get()), InstanceProxyModel::LoaderFilter::Fabric);
        QCOMPARE(InstanceProxyModel::detectLoader(nullptr), InstanceProxyModel::LoaderFilter::All);

        TestInstanceSourceModel source;
        source.add(vanilla, QStringLiteral("Singleplayer"));
        source.add(neoforge, QStringLiteral("Servers"));
        source.add(fabric, QStringLiteral("OptiFineGroup"));

        InstanceProxyModel proxy;
        proxy.setSourceModel(&source);
        proxy.sort(0);

        QCOMPARE(proxy.rowCount(), 3);
        QVERIFY(!proxy.hasActiveFilter());

        proxy.setLoaderFilter(InstanceProxyModel::LoaderFilter::NeoForge);
        QVERIFY(proxy.hasActiveFilter());
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Tech Server"));

        proxy.setLoaderFilter(InstanceProxyModel::LoaderFilter::Vanilla);
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Pure World"));

        // Search by group name and managed pack name
        proxy.setLoaderFilter(InstanceProxyModel::LoaderFilter::All);
        proxy.setSearchQuery(QStringLiteral("OptiFineGroup"));
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Sodium Light"));

        proxy.setSearchQuery(QStringLiteral("Clockwork Empire"));
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Tech Server"));
    }

    void nullInstanceAndHeadlessDecorationSafety()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(tempDir.filePath(QStringLiteral("global.cfg")));
        const QString root = tempDir.filePath(QStringLiteral("valid"));
        QDir().mkpath(root);
        auto instSettings = std::make_unique<INISettingsObject>(QDir(root).filePath(QStringLiteral("instance.cfg")));
        auto validInst = std::make_shared<NullInstance>(global.get(), std::move(instSettings), root);
        validInst->setName(QStringLiteral("Valid Alpha"));

        TestInstanceSourceModel source;
        source.addNullRow(QStringLiteral("Null Row A"));
        source.add(validInst);
        source.addNullRow(QStringLiteral("Null Row B"));

        InstanceProxyModel proxy;
        proxy.setSourceModel(&source);
        proxy.sort(0);

        QCOMPARE(proxy.rowCount(), 3);
        // Valid instance sorts before null rows deterministically
        QCOMPARE(proxy.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Valid Alpha"));
        // Headless Qt::DecorationRole must not crash when APPLICATION_DYN == nullptr
        QCOMPARE(proxy.index(0, 0).data(Qt::DecorationRole).toString(), QStringLiteral("default"));

        proxy.setRunningOnly(true);
        QCOMPARE(proxy.rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(InstanceProxyModelTest)

#include "InstanceProxyModel_test.moc"
