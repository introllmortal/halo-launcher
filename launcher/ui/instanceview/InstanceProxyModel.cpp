/* Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "InstanceProxyModel.h"

#include <BaseInstance.h>
#include <InstanceList.h>
#include <icons/IconList.h>
#include "Application.h"
#include "InstanceView.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"

#include <QDebug>

InstanceProxyModel::InstanceProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
{
    m_naturalSort.setNumericMode(true);
    m_naturalSort.setCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
    // FIXME: use loaded translation as source of locale instead, hook this up to translation changes
    m_naturalSort.setLocale(QLocale::system());
}

void InstanceProxyModel::setSearchQuery(const QString& query)
{
    const QString trimmed = query.trimmed();
    if (m_searchQuery == trimmed) {
        return;
    }
    m_searchQuery = trimmed;
    invalidateFilter();
}

void InstanceProxyModel::setLoaderFilter(LoaderFilter filter)
{
    if (m_loaderFilter == filter) {
        return;
    }
    m_loaderFilter = filter;
    invalidateFilter();
}

void InstanceProxyModel::setRunningOnly(bool runningOnly)
{
    if (m_runningOnly == runningOnly) {
        return;
    }
    m_runningOnly = runningOnly;
    invalidateFilter();
}

InstanceProxyModel::LoaderFilter InstanceProxyModel::detectLoaderFromComponentUids(const QSet<QString>& uids)
{
    if (uids.contains(QStringLiteral("net.neoforged")) || uids.contains(QStringLiteral("net.neoforge"))) {
        return LoaderFilter::NeoForge;
    }
    if (uids.contains(QStringLiteral("net.minecraftforge"))) {
        return LoaderFilter::Forge;
    }
    if (uids.contains(QStringLiteral("net.fabricmc.fabric-loader"))) {
        return LoaderFilter::Fabric;
    }
    if (uids.contains(QStringLiteral("org.quiltmc.quilt-loader"))) {
        return LoaderFilter::Quilt;
    }
    if (uids.contains(QStringLiteral("net.minecraft"))) {
        return LoaderFilter::Vanilla;
    }
    return LoaderFilter::All;
}

InstanceProxyModel::LoaderFilter InstanceProxyModel::detectLoader(const BaseInstance* instance)
{
    const auto* mcInst = dynamic_cast<const MinecraftInstance*>(instance);
    if (!mcInst || !mcInst->getPackProfile()) {
        return LoaderFilter::All;
    }
    const auto* profile = mcInst->getPackProfile();
    QSet<QString> uids;
    static const QString kKnownUids[] = {
        QStringLiteral("net.neoforged"),
        QStringLiteral("net.neoforge"),
        QStringLiteral("net.minecraftforge"),
        QStringLiteral("net.fabricmc.fabric-loader"),
        QStringLiteral("org.quiltmc.quilt-loader"),
        QStringLiteral("net.minecraft"),
    };
    for (const QString& uid : kKnownUids) {
        if (!profile->getComponentVersion(uid).isEmpty()) {
            uids.insert(uid);
        }
    }
    return detectLoaderFromComponentUids(uids);
}

bool InstanceProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!idx.isValid()) {
        return false;
    }

    auto* inst = static_cast<BaseInstance*>(idx.data(InstanceList::InstancePointerRole).value<void*>());
    if (m_runningOnly && (!inst || !inst->isRunning())) {
        return false;
    }
    if (m_loaderFilter != LoaderFilter::All && detectLoader(inst) != m_loaderFilter) {
        return false;
    }

    if (m_searchQuery.isEmpty()) {
        return true;
    }

    const QString name = idx.data(Qt::DisplayRole).toString();
    if (name.contains(m_searchQuery, Qt::CaseInsensitive)) {
        return true;
    }
    const QString group = idx.data(InstanceViewRoles::GroupRole).toString();
    if (group.contains(m_searchQuery, Qt::CaseInsensitive)) {
        return true;
    }
    if (inst && inst->isManagedPack() && inst->getManagedPackName().contains(m_searchQuery, Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

QVariant InstanceProxyModel::data(const QModelIndex& index, int role) const
{
    QVariant data = QSortFilterProxyModel::data(index, role);
    if (role == Qt::DecorationRole && APPLICATION_DYN != nullptr && APPLICATION_DYN->icons()) {
        return QVariant(APPLICATION->icons()->getIcon(data.toString()));
    }
    return data;
}

bool InstanceProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString leftCategory = left.data(InstanceViewRoles::GroupRole).toString();
    const QString rightCategory = right.data(InstanceViewRoles::GroupRole).toString();
    if (leftCategory == rightCategory) {
        return subSortLessThan(left, right);
    } else {
        // FIXME: real group sorting happens in InstanceView::updateGeometries(), see LocaleString
        auto result = leftCategory.localeAwareCompare(rightCategory);
        if (result == 0) {
            return subSortLessThan(left, right);
        }
        return result < 0;
    }
}

bool InstanceProxyModel::subSortLessThan(const QModelIndex& left, const QModelIndex& right) const
{
    auto* pdataLeft = static_cast<BaseInstance*>(left.data(InstanceList::InstancePointerRole).value<void*>());
    auto* pdataRight = static_cast<BaseInstance*>(right.data(InstanceList::InstancePointerRole).value<void*>());
    if (!pdataLeft || !pdataRight) {
        if (pdataLeft != pdataRight) {
            return pdataLeft != nullptr;
        }
        return m_naturalSort.compare(left.data(Qt::DisplayRole).toString(), right.data(Qt::DisplayRole).toString()) < 0;
    }

    const bool leftPinned = pdataLeft->isPinned();
    const bool rightPinned = pdataRight->isPinned();
    if (leftPinned != rightPinned) {
        return leftPinned;
    }

    const QString sortMode = (APPLICATION_DYN != nullptr && APPLICATION_DYN->settings())
                                 ? APPLICATION_DYN->settings()->get("InstSortMode").toString()
                                 : QStringLiteral("Name");
    if (sortMode == "LastLaunch") {
        return pdataLeft->lastLaunch() > pdataRight->lastLaunch();
    } else if (sortMode == "Playtime") {
        if (pdataLeft->totalTimePlayed() == pdataRight->totalTimePlayed()) {
            return m_naturalSort.compare(pdataLeft->name(), pdataRight->name()) < 0;
        }
        return pdataLeft->totalTimePlayed() > pdataRight->totalTimePlayed();
    } else {
        return m_naturalSort.compare(pdataLeft->name(), pdataRight->name()) < 0;
    }
}
