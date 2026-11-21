// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDir>
#include <QFileSystemWatcher>
#include <QList>
#include <QLocale>
#include <QString>
#include <QTranslator>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "net/NetJob.h"
#include "translations/TranslationsModel.h"

namespace TranslationsInternal {
enum class FileType : std::uint8_t { None, Qm, Po };

inline QString getSystemLocaleName()
{
    return QLocale::system().name();
}

inline QString getSystemLanguage()
{
    return getSystemLocaleName().split('_').front();
}
}  // namespace TranslationsInternal

struct Language {
    Language() : updated(true) {}

    explicit Language(QString _key) : key(std::move(_key)), updated(key == QLatin1String("en_US")) { locale = QLocale(key); }

    QString languageName() const
    {
        QString result;
        if (key == "ja_KANJI") {
            result = locale.nativeLanguageName() + QString::fromUtf8(" (漢字)");
        } else if (key == "es_UY") {
            result = QString::fromUtf8("Español de Latinoamérica");
        } else if (key == "en_NZ") {
            result = QStringLiteral("New Zealand English");
        } else if (key == "en@pirate") {
            result = QStringLiteral("Tongue of the High Seas");
        } else if (key == "en@uwu") {
            result = QStringLiteral("Cute Engwish");
        } else if (key == "tok") {
            result = QStringLiteral("toki pona");
        } else if (key == "nan") {
            result = QString::fromUtf8("閩南語");
        } else {
            result = locale.nativeLanguageName();
        }
        return result.isEmpty() ? key : result;
    }

    float percentTranslated() const { return total == 0 ? 100.0F : 100.0F * static_cast<float>(translated) / static_cast<float>(total); }

    void setTranslationStats(const unsigned _translated, const unsigned _untranslated, const unsigned _fuzzy)
    {
        translated = _translated;
        untranslated = _untranslated;
        fuzzy = _fuzzy;
        total = translated + untranslated + fuzzy;
    }

    bool isOfSameNameAs(const Language& other) const { return key == other.key; }

    bool isIdenticalTo(const Language& other) const
    {
        return (key == other.key && fileName == other.fileName && fileSize == other.fileSize && fileSha1 == other.fileSha1 &&
                translated == other.translated && fuzzy == other.fuzzy && total == other.total && localFileType == other.localFileType);
    }

    Language& apply(const Language& other)
    {
        if (!isOfSameNameAs(other)) {
            return *this;
        }
        fileName = other.fileName;
        fileSize = other.fileSize;
        fileSha1 = other.fileSha1;
        translated = other.translated;
        fuzzy = other.fuzzy;
        total = other.total;
        localFileType = other.localFileType;
        return *this;
    }

    QString key;
    QLocale locale;
    bool updated;
    QString fileName;
    std::size_t fileSize = 0;
    QString fileSha1;
    unsigned translated = 0;
    unsigned untranslated = 0;
    unsigned fuzzy = 0;
    unsigned total = 0;
    TranslationsInternal::FileType localFileType = TranslationsInternal::FileType::None;
};

struct TranslationsModel::Private {
    QDir m_dir;
    QList<Language> m_languages = {
        Language(QStringLiteral("en_US")),
        Language(QStringLiteral("ru")),
    };
    QString m_selectedLanguage = QStringLiteral("en_US");
    std::unique_ptr<QTranslator> m_qtTranslator;
    std::unique_ptr<QTranslator> m_appTranslator;
    std::vector<std::unique_ptr<QTranslator>> m_haloTranslators;

    Net::Download* m_indexTask = nullptr;
    QString m_downloadingTranslation;
    NetJob::Ptr m_downloadJob;
    NetJob::Ptr m_indexJob;
    QString m_nextDownload;
    QFileSystemWatcher* watcher = nullptr;
    bool m_noLanguageSet = false;
};
