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
 */

#include "ModFolderModel.h"

#include <QAbstractButton>
#include <QFileInfo>
#include <QMessageBox>
#include <algorithm>
#include <set>

#include "minecraft/mod/ResourceFolderModel.h"
#include "ui/dialogs/CustomMessageBox.h"

namespace {

QSet<Mod*> collectMods(const QSet<Mod*>& mods, QHash<QString, QSet<Mod*>> relation, std::set<QString>& seen, bool shouldBeEnabled)
{
    QSet<Mod*> affectedList = {};
    QSet<Mod*> needToCheck = {};
    for (auto* mod : mods) {
        auto id = mod->mod_id();
        if (!seen.contains(id)) {
            seen.insert(id);
            for (auto* affected : relation[id]) {
                auto affectedId = affected->mod_id();

                const bool inMods = std::ranges::any_of(mods, [&affectedId](Mod* m) { return m->mod_id() == affectedId; });
                if (!inMods && !seen.contains(affectedId)) {
                    if (shouldBeEnabled != affected->enabled()) {
                        affectedList << affected;
                    }
                    needToCheck << affected;
                }
            }
        }
    }
    if (!needToCheck.isEmpty()) {
        affectedList += collectMods(needToCheck, relation, seen, shouldBeEnabled);
    }
    return affectedList;
}

QStringList reqToList(const QSet<Mod*>& modsSet)
{
    QStringList req;
    for (auto* m : modsSet) {
        req << m->name();
    }
    return req;
}

}  // namespace

QModelIndexList ModFolderModel::getAffectedMods(const QModelIndexList& indexes, EnableAction action)
{
    if (indexes.isEmpty()) {
        return {};
    }

    QModelIndexList affectedList = {};
    auto affectedModsList = selectedMods(indexes);
    auto affectedMods = QSet(affectedModsList.begin(), affectedModsList.end());
    std::set<QString> seen;

    switch (action) {
        case EnableAction::ENABLE: {
            affectedMods = collectMods(affectedMods, m_requires, seen, true);
            break;
        }
        case EnableAction::DISABLE: {
            affectedMods = collectMods(affectedMods, m_requiredBy, seen, false);
            break;
        }
        case EnableAction::TOGGLE: {
            return {};
        }
    }
    for (auto* affected : affectedMods) {
        auto row = m_resourcesIndex[affected->internalId()];
        affectedList << index(row, 0);
    }
    return affectedList;
}

bool ModFolderModel::setResourceEnabled(const QModelIndexList& indexes, EnableAction action)
{
    if (indexes.isEmpty()) {
        return {};
    }

    auto indexedModsList = selectedMods(indexes);
    auto indexedMods = QSet(indexedModsList.begin(), indexedModsList.end());

    QSet<Mod*> toEnable = {};
    QSet<Mod*> toDisable = {};
    std::set<QString> seen;

    switch (action) {
        case EnableAction::ENABLE: {
            toEnable = indexedMods;
            break;
        }
        case EnableAction::DISABLE: {
            toDisable = indexedMods;
            break;
        }
        case EnableAction::TOGGLE: {
            for (auto* mod : indexedMods) {
                if (mod->enabled()) {
                    toDisable << mod;
                } else {
                    toEnable << mod;
                }
            }
            break;
        }
    }

    auto requiredToEnable = collectMods(toEnable, m_requires, seen, true);
    auto requiredToDisable = collectMods(toDisable, m_requiredBy, seen, false);

    toDisable.removeIf([toEnable](Mod* m) { return toEnable.contains(m); });
    auto toList = [this](const QSet<Mod*>& mods) {
        QModelIndexList list;
        for (auto* mod : mods) {
            auto row = m_resourcesIndex[mod->internalId()];
            list << index(row, 0);
        }
        return list;
    };

    if (requiredToEnable.size() > 0 || requiredToDisable.size() > 0) {
        QString title;
        QString message;
        QString noButton;
        QString yesButton;
        if (requiredToEnable.size() > 0 && requiredToDisable.size() > 0) {
            title = tr("Confirm toggle");
            message = tr("Toggling these mod(s) will cause changes to other mods.\n") +
                      tr("%n mod(s) will be enabled\n", "", requiredToEnable.size()) +
                      tr("%n mod(s) will be disabled\n", "", requiredToDisable.size()) +
                      tr("Do you want to automatically apply these related changes?\nIgnoring them may break the game.");
            noButton = tr("Only Toggle Selected");
            yesButton = tr("Toggle Required Mods");
        } else if (requiredToEnable.size() > 0) {
            title = tr("Confirm enable");
            message = tr("The enabled mod(s) require %n mod(s).\n", "", requiredToEnable.size()) +
                      tr("Would you like to enable them as well?\nIgnoring them may break the game.");
            noButton = tr("Only Enable Selected");
            yesButton = tr("Enable Required");
        } else {
            title = tr("Confirm disable");
            message = tr("The disabled mod(s) are required by %n mod(s).\n", "", requiredToDisable.size()) +
                      tr("Would you like to disable them as well?\nIgnoring them may break the game.");
            noButton = tr("Only Disable Selected");
            yesButton = tr("Disable Required");
        }

        auto* box = CustomMessageBox::selectable(nullptr, title, message, QMessageBox::Warning,
                                                 QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);
        box->button(QMessageBox::No)->setText(noButton);
        box->button(QMessageBox::Yes)->setText(yesButton);
        auto response = box->exec();

        if (response == QMessageBox::Yes) {
            toEnable |= requiredToEnable;
            toDisable |= requiredToDisable;
        } else if (response == QMessageBox::Cancel) {
            return false;
        }
    }

    auto disableStatus = ResourceFolderModel::setResourceEnabled(toList(toDisable), EnableAction::DISABLE);
    auto enableStatus = ResourceFolderModel::setResourceEnabled(toList(toEnable), EnableAction::ENABLE);
    return disableStatus && enableStatus;
}

QStringList ModFolderModel::requiresList(const QString& id)
{
    return reqToList(m_requires[id]);
}

QStringList ModFolderModel::requiredByList(const QString& id)
{
    return reqToList(m_requiredBy[id]);
}

bool ModFolderModel::deleteResources(const QModelIndexList& indexes)
{
    auto deleteInvalid = [](QSet<Mod*>& mods) {
        for (auto it = mods.begin(); it != mods.end();) {
            auto* mod = *it;
            // the QFileInfo::exists is used instead of mod->fileinfo().exists
            // because the later somehow caches that the file exists
            if (!mod || !QFileInfo::exists(mod->fileinfo().absoluteFilePath())) {
                it = mods.erase(it);
            } else {
                ++it;
            }
        }
    };
    auto rsp = ResourceFolderModel::deleteResources(indexes);
    for (auto* mod : allMods()) {
        auto id = mod->mod_id();
        deleteInvalid(m_requiredBy[id]);
        deleteInvalid(m_requires[id]);
        if (mod->requiredByCount() != m_requiredBy[id].count() || mod->requiresCount() != m_requires[id].count()) {
            mod->setRequiredByCount(m_requiredBy[id].count());
            mod->setRequiresCount(m_requires[id].count());
            int row = m_resourcesIndex[mod->internalId()];
            emit dataChanged(index(row, RequiresColumn), index(row, RequiredByColumn));
        }
    }
    return rsp;
}
