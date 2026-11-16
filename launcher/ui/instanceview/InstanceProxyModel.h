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

#pragma once

#include <QCollator>
#include <QSet>
#include <QSortFilterProxyModel>

class BaseInstance;

class InstanceProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

   public:
    enum class LoaderFilter {
        All,
        Vanilla,
        NeoForge,
        Forge,
        Fabric,
        Quilt,
    };
    Q_ENUM(LoaderFilter)

    InstanceProxyModel(QObject* parent = 0);

    void setSearchQuery(const QString& query);
    QString searchQuery() const { return m_searchQuery; }

    void setLoaderFilter(LoaderFilter filter);
    LoaderFilter loaderFilter() const { return m_loaderFilter; }

    void setRunningOnly(bool runningOnly);
    bool runningOnly() const { return m_runningOnly; }

    bool hasActiveFilter() const { return !m_searchQuery.isEmpty() || m_loaderFilter != LoaderFilter::All || m_runningOnly; }

    static LoaderFilter detectLoader(const BaseInstance* instance);
    static LoaderFilter detectLoaderFromComponentUids(const QSet<QString>& uids);

   protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    bool subSortLessThan(const QModelIndex& left, const QModelIndex& right) const;

   private:
    QCollator m_naturalSort;
    QString m_searchQuery;
    LoaderFilter m_loaderFilter = LoaderFilter::All;
    bool m_runningOnly = false;
};
