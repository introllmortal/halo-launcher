// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "TranslationsModel_p.h"

#include <QDebug>

#include "Application.h"
#include "BuildConfig.h"
#include "net/ChecksumValidator.h"

void TranslationsModel::downloadIndex()
{
    if (d->m_indexJob || d->m_downloadJob) {
        return;
    }
    qDebug() << "Downloading Translations Index...";
    d->m_indexJob.reset(new NetJob("Translations Index", APPLICATION->network()));
    const MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry("translations", "index_v2.json");
    entry->setStale(true);
    auto task = Net::Download::makeCached(QUrl(BuildConfig.TRANSLATION_FILES_URL + "index_v2.json"), entry);
    d->m_indexTask = task.get();
    d->m_indexJob->addNetAction(task);
    d->m_indexJob->setAskRetry(false);
    connect(d->m_indexJob.get(), &NetJob::failed, this, &TranslationsModel::indexFailed);
    connect(d->m_indexJob.get(), &NetJob::succeeded, this, &TranslationsModel::indexReceived);
    d->m_indexJob->start();
}

void TranslationsModel::updateLanguage(const QString& key)
{
    if (key == QLatin1String("en_US")) {
        qWarning() << "Cannot update builtin language" << key;
        return;
    }
    auto found = findLanguageAsOptional(key);
    if (!found.has_value()) {
        qWarning() << "Cannot update invalid language" << key;
        return;
    }
    if (!found->updated) {
        downloadTranslation(key);
    }
}

void TranslationsModel::downloadTranslation(const QString& key)
{
    if (d->m_downloadJob) {
        d->m_nextDownload = key;
        return;
    }
    auto lang = findLanguageAsOptional(key);
    if (!lang.has_value()) {
        qWarning() << "Will not download an unknown translation" << key;
        return;
    }
    if (lang->fileName.isEmpty()) {
        qDebug() << "Skipping download for built-in translation without remote index entry:" << key;
        return;
    }

    d->m_downloadingTranslation = key;
    const MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry("translations", "mmc_" + key + ".qm");
    entry->setStale(true);

    auto dl = Net::Download::makeCached(QUrl(BuildConfig.TRANSLATION_FILES_URL + lang->fileName), entry);
    dl->addValidator(new Net::ChecksumValidator(QCryptographicHash::Sha1, lang->fileSha1));
    dl->setProgress(dl->getProgress(), lang->fileSize);

    d->m_downloadJob.reset(new NetJob("Translation for " + key, APPLICATION->network()));
    d->m_downloadJob->addNetAction(dl);
    d->m_downloadJob->setAskRetry(false);

    connect(d->m_downloadJob.get(), &NetJob::succeeded, this, &TranslationsModel::dlGood);
    connect(d->m_downloadJob.get(), &NetJob::failed, this, &TranslationsModel::dlFailed);

    d->m_downloadJob->start();
}

void TranslationsModel::downloadNext()
{
    if (!d->m_nextDownload.isEmpty()) {
        downloadTranslation(d->m_nextDownload);
        d->m_nextDownload.clear();
    }
}

void TranslationsModel::dlFailed(const QString& reason)
{
    qCritical() << "Translations Download Failed:" << reason;
    d->m_downloadJob.reset();
    downloadNext();
}

void TranslationsModel::dlGood()
{
    qDebug() << "Got translation:" << d->m_downloadingTranslation;

    if (d->m_downloadingTranslation == d->m_selectedLanguage) {
        selectLanguage(d->m_selectedLanguage);
    }
    d->m_downloadJob.reset();
    downloadNext();
}

void TranslationsModel::indexFailed(const QString& reason) const
{
    qCritical() << "Translations Index Download Failed:" << reason;
    d->m_indexJob.reset();
}
