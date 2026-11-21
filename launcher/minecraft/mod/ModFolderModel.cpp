// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "ModFolderModel.h"

#include <FileSystem.h>
#include <QAbstractButton>
#include <QDebug>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHeaderView>
#include <QIcon>
#include <QMetaObject>
#include <QMimeData>
#include <QPointer>
#include <QString>
#include <QStyle>
#include <QThread>
#include <QThreadPool>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include <set>

#include "minecraft/mod/Resource.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "minecraft/mod/tasks/LocalModParseTask.h"
#include "modplatform/ModIndex.h"
#include "ui/dialogs/CustomMessageBox.h"

ModFolderModel::ModFolderModel(const QDir& dir, BaseInstance* instance, bool isIndexed, bool createDir, QObject* parent)
    : ResourceFolderModel(QDir(dir), instance, isIndexed, createDir, parent)
{
    m_columnNames = QStringList({ "Enable", "Image", "Name", "Version", "Last Modified", "Provider", "Size", "Side", "Loaders",
                                  "Minecraft Versions", "Release Type", "Requires", "Required By", "File Name" });
    m_columnNamesTranslated =
        QStringList({ tr("Enable"), tr("Image"), tr("Name"), tr("Version"), tr("Last Modified"), tr("Provider"), tr("Size"), tr("Side"),
                      tr("Loaders"), tr("Minecraft Versions"), tr("Release Type"), tr("Requires"), tr("Required By"), tr("File Name") });
    m_columnSortKeys = { SortType::Enabled,     SortType::Name,     SortType::Name,       SortType::Version, SortType::Date,
                         SortType::Provider,    SortType::Size,     SortType::Side,       SortType::Loaders, SortType::McVersions,
                         SortType::ReleaseType, SortType::Requires, SortType::RequiredBy, SortType::Filename };
    m_columnResizeModes = { QHeaderView::Interactive, QHeaderView::Interactive, QHeaderView::Stretch,     QHeaderView::Interactive,
                            QHeaderView::Interactive, QHeaderView::Interactive, QHeaderView::Interactive, QHeaderView::Interactive,
                            QHeaderView::Interactive, QHeaderView::Interactive, QHeaderView::Interactive, QHeaderView::Interactive,
                            QHeaderView::Interactive, QHeaderView::Interactive };
    m_columnsHideable = { false, true, false, true, true, true, true, true, true, true, true, true, true, true };

    m_iconPool.setMaxThreadCount(qBound(2, QThread::idealThreadCount() / 2, 4));
    connect(this, &ModFolderModel::parseFinished, this, &ModFolderModel::onParseFinished);
}

ModFolderModel::~ModFolderModel()
{
    m_iconPool.clear();
    m_iconPool.waitForDone();
}

QVariant ModFolderModel::data(const QModelIndex& index, int role) const
{
    if (!validateIndex(index)) {
        return {};
    }

    int row = index.row();
    int column = index.column();

    switch (role) {
        case Qt::BackgroundRole:
            return rowBackground(row);
        case Qt::DisplayRole:
            switch (column) {
                case VersionColumn: {
                    switch (at(row).type()) {
                        case ResourceType::FOLDER:
                            return tr("Folder");
                        case ResourceType::SINGLEFILE:
                            return tr("File");
                        default:
                            return at(row).version();
                    }
                }
                case SideColumn: {
                    return at(row).side();
                }
                case LoadersColumn: {
                    return at(row).loaders();
                }
                case McVersionsColumn: {
                    return at(row).mcVersionsString();
                }
                case ReleaseTypeColumn: {
                    return at(row).releaseType();
                }
                case RequiredByColumn: {
                    return at(row).requiredByCount();
                }
                case RequiresColumn: {
                    return at(row).requiresCount();
                }
                default:
                    break;
            }
            break;
        case Qt::DecorationRole: {
            if (column == ImageColumn) {
                const Mod& mod = at(row);
                QPixmap cached = mod.cachedIcon({ 32, 32 }, Qt::AspectRatioMode::KeepAspectRatioByExpanding);
                if (!cached.isNull()) {
                    return cached;
                }
                if (mod.needsIconLoad()) {
                    const_cast<ModFolderModel*>(this)->requestIconLoad(row);
                }
                return {};
            }
            break;
        }
        case Qt::SizeHintRole:
            if (column == ImageColumn) {
                return QSize(32, 32);
            }
            break;
        default:
            break;
    }

    // map the columns to the base equivilents
    QModelIndex mappedIndex;
    switch (column) {
        case ActiveColumn:
            mappedIndex = index.siblingAtColumn(ResourceFolderModel::ActiveColumn);
            break;
        case NameColumn:
            mappedIndex = index.siblingAtColumn(ResourceFolderModel::NameColumn);
            break;
        case DateColumn:
            mappedIndex = index.siblingAtColumn(ResourceFolderModel::DateColumn);
            break;
        case ProviderColumn:
            mappedIndex = index.siblingAtColumn(ResourceFolderModel::ProviderColumn);
            break;
        case SizeColumn:
            mappedIndex = index.siblingAtColumn(ResourceFolderModel::SizeColumn);
            break;
        case FileNameColumn:
            mappedIndex = index.siblingAtColumn(ResourceFolderModel::FileNameColumn);
            break;
        default:
            break;
    }

    if (mappedIndex.isValid()) {
        return ResourceFolderModel::data(mappedIndex, role);
    }

    return {};
}

QVariant ModFolderModel::headerData(int section, [[maybe_unused]] Qt::Orientation orientation, int role) const
{
    switch (role) {
        case Qt::DisplayRole:
            switch (section) {
                case ActiveColumn:
                case NameColumn:
                case VersionColumn:
                case DateColumn:
                case ProviderColumn:
                case ImageColumn:
                case SideColumn:
                case LoadersColumn:
                case McVersionsColumn:
                case ReleaseTypeColumn:
                case SizeColumn:
                case RequiredByColumn:
                case RequiresColumn:
                case FileNameColumn:
                    return columnNames().at(section);
                default:
                    return QVariant();
            }

        case Qt::ToolTipRole:
            switch (section) {
                case ActiveColumn:
                    return tr("Is the mod enabled?");
                case NameColumn:
                    return tr("The name of the mod.");
                case VersionColumn:
                    return tr("The version of the mod.");
                case DateColumn:
                    return tr("The date and time this mod was last changed (or added).");
                case ProviderColumn:
                    return tr("The source provider of the mod.");
                case SideColumn:
                    return tr("On what environment the mod is running.");
                case LoadersColumn:
                    return tr("The mod loader.");
                case McVersionsColumn:
                    return tr("The supported minecraft versions.");
                case ReleaseTypeColumn:
                    return tr("The release type.");
                case SizeColumn:
                    return tr("The size of the mod.");
                case RequiredByColumn:
                    return tr("For each mod, the number of other mods which depend on it.");
                case RequiresColumn:
                    return tr("For each mod, the number of other mods it depends on.");
                case FileNameColumn:
                    return tr("The file name of the mod.");
                default:
                    return QVariant();
            }
        default:
            return QVariant();
    }
}

int ModFolderModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : NumColumns;
}

Task* ModFolderModel::createParseTask(Resource& resource)
{
    return new LocalModParseTask(m_nextResolutionTicket, resource.type(), resource.fileinfo());
}

bool ModFolderModel::isValid()
{
    return m_dir.exists() && m_dir.isReadable();
}

void ModFolderModel::onParseSucceeded(int ticket, const QString& resourceId)
{
    auto iter = m_activeParseTasks.constFind(ticket);
    if (iter == m_activeParseTasks.constEnd()) {
        return;
    }

    int row = m_resourcesIndex[resourceId];

    const auto& parseTask = *iter;
    auto* castTask = static_cast<LocalModParseTask*>(parseTask.get());

    Q_ASSERT(castTask->token() == ticket);

    auto resource = find(resourceId);

    auto result = castTask->result();
    if (result && resource) {
        auto* mod = static_cast<Mod*>(resource.get());
        mod->finishResolvingWithDetails(std::move(result->details));
    }
    emit dataChanged(index(row, RequiresColumn), index(row, RequiredByColumn));
}

void ModFolderModel::requestIconLoad(int row)
{
    if (row < 0 || row >= rowCount()) {
        return;
    }
    const Mod& mod = at(row);
    const QString resourceId = mod.internalId();
    const QString iconPath = mod.iconPath();
    if (resourceId.isEmpty() || iconPath.isEmpty() || m_pendingIconIds.contains(resourceId)) {
        return;
    }
    m_pendingIconIds.insert(resourceId);

    const QString filePath = mod.fileinfo().filePath();
    const ResourceType type = mod.type();
    QPointer<ModFolderModel> self(this);

    m_iconPool.start([self, resourceId, filePath, type, iconPath]() {
        QImage loadedImage;
        ModUtils::loadIconImage(filePath, type, iconPath, &loadedImage);

        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(
            self,
            [self, resourceId, loadedImage]() {
                if (!self) {
                    return;
                }
                self->m_pendingIconIds.remove(resourceId);
                auto rowIt = self->m_resourcesIndex.constFind(resourceId);
                if (rowIt == self->m_resourcesIndex.constEnd()) {
                    return;
                }
                const int targetRow = rowIt.value();
                auto resource = self->find(resourceId);
                if (!resource) {
                    return;
                }
                auto* modPtr = static_cast<Mod*>(resource.get());
                if (loadedImage.isNull()) {
                    modPtr->markIconLoadAttempted();
                    return;
                }
                modPtr->setIcon(loadedImage);
                const QModelIndex iconIdx = self->index(targetRow, ImageColumn);
                emit self->dataChanged(iconIdx, iconIdx, { Qt::DecorationRole });
            },
            Qt::QueuedConnection);
    });
}

void ModFolderModel::onParseFinished()
{
    if (hasPendingParseTasks()) {
        return;
    }
    auto modsList = allMods();
    auto mods = QSet(modsList.begin(), modsList.end());

    m_requires.clear();
    m_requiredBy.clear();

    QHash<QString, Mod*> modsById;
    QHash<QString, Mod*> modsByProviderProject;
    modsById.reserve(mods.size());
    modsByProviderProject.reserve(mods.size());

    auto makeProjectKey = [](ModPlatform::ResourceProvider provider, const QVariant& projectId) -> QString {
        return QString::number(static_cast<int>(provider)) + QLatin1Char(':') + projectId.toString();
    };

    for (auto* mod : mods) {
        if (!mod->mod_id().isEmpty()) {
            modsById.insert(mod->mod_id(), mod);
        }
        if (mod->metadata()) {
            modsByProviderProject.insert(makeProjectKey(mod->metadata()->provider, mod->metadata()->project_id), mod);
        }
    }

    for (auto* mod : mods) {
        auto id = mod->mod_id();
        for (const auto& dep : mod->dependencies()) {
            if (auto* d = modsById.value(dep, nullptr)) {
                m_requires[id] << d;
                m_requiredBy[d->mod_id()] << mod;
            }
        }
        if (mod->metadata()) {
            for (const auto& dep : mod->metadata()->dependencies) {
                if (dep.type == ModPlatform::DependencyType::REQUIRED) {
                    if (auto* d = modsByProviderProject.value(makeProjectKey(mod->metadata()->provider, dep.addonId), nullptr)) {
                        m_requires[id] << d;
                        m_requiredBy[d->mod_id()] << mod;
                    }
                }
            }
        }
    }
    for (auto* mod : mods) {
        auto id = mod->mod_id();
        if (mod->requiredByCount() != m_requiredBy[id].count() || mod->requiresCount() != m_requires[id].count()) {
            mod->setRequiredByCount(m_requiredBy[id].count());
            mod->setRequiresCount(m_requires[id].count());
            int row = m_resourcesIndex[mod->internalId()];
            emit dataChanged(index(row, RequiresColumn), index(row, RequiredByColumn), { Qt::DisplayRole });
        }
    }
}
