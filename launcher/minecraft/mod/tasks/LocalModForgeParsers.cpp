#include "LocalModParseTask.h"

#include <toml++/toml.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "minecraft/mod/ModDetails.h"

namespace ModUtils {

// NEW format
// https://github.com/MinecraftForge/FML/wiki/FML-mod-information-file/c8d8f1929aff9979e322af79a59ce81f3e02db6a

// OLD format:
// https://github.com/MinecraftForge/FML/wiki/FML-mod-information-file/5bf6a2d05145ec79387acc0d45c958642fb049fc
ModDetails ReadMCModInfo(QByteArray contents)
{
    auto getInfoFromArray = [](QJsonArray arr) -> ModDetails {
        if (!arr.at(0).isObject()) {
            return {};
        }
        ModDetails details;
        auto firstObj = arr.at(0).toObject();
        details.mod_id = firstObj.value("modid").toString();
        auto name = firstObj.value("name").toString();
        // NOTE: ignore stupid example mods copies where the author didn't even bother to change the name
        if (name != "Example Mod") {
            details.name = name;
        }
        details.version = firstObj.value("version").toString();
        auto homeurl = firstObj.value("url").toString().trimmed();
        if (!homeurl.isEmpty()) {
            if (!homeurl.startsWith("http://") && !homeurl.startsWith("https://") && !homeurl.startsWith("ftp://")) {
                homeurl.prepend("http://");
            }
        }
        details.homeurl = homeurl;
        details.description = firstObj.value("description").toString();
        QJsonArray authors = firstObj.value("authorList").toArray();
        if (authors.size() == 0) {
            authors = firstObj.value("authors").toArray();
        }

        if (firstObj.contains("logoFile")) {
            details.icon_file = firstObj.value("logoFile").toString();
        }

        for (auto author : authors) {
            details.authors.append(author.toString());
        }

        if (details.mod_id.startsWith("mod_")) {
            details.mod_id = details.mod_id.mid(4);
        }

        auto addDep = [&details](QString dep) {
            if (dep == "mod_MinecraftForge" || dep == "Forge")
                return;
            if (dep.contains(":")) {
                dep = dep.section(":", 1);
            }
            if (dep.contains("@")) {
                dep = dep.section("@", 0, 0);
            }
            if (dep.startsWith("mod_")) {
                dep = dep.mid(4);
            }
            details.dependencies.append(dep);
        };

        if (firstObj.contains("requiredMods")) {
            for (auto dep : firstObj.value("requiredMods").toArray()) {
                addDep(dep.toString());
            }
        } else if (firstObj.contains("dependencies")) {
            for (auto dep : firstObj.value("dependencies").toArray()) {
                addDep(dep.toString());
            }
        }

        return details;
    };
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
    if (jsonDoc.isArray()) {
        return getInfoFromArray(jsonDoc.array());
    } else if (jsonDoc.isObject()) {
        auto val = jsonDoc.object().value("modinfoversion");
        if (val.isUndefined()) {
            val = jsonDoc.object().value("modListVersion");
        }

        int version = val.toInt(-1);

        if (version < 0)
            version = val.toString("").toInt();

        if (version != 2) {
            qWarning() << QString(R"(The value of 'modListVersion' is "%1" (expected "2")! The file may be corrupted.)").arg(version);
            qWarning() << "The contents of 'mcmod.info' are as follows:";
            qWarning() << contents;
        }

        auto arrVal = jsonDoc.object().value("modlist");
        if (arrVal.isUndefined()) {
            arrVal = jsonDoc.object().value("modList");
        }
        if (arrVal.isArray()) {
            return getInfoFromArray(arrVal.toArray());
        }
    }
    return {};
}

// https://github.com/MinecraftForge/Documentation/blob/5ab4ba6cf9abc0ac4c0abd96ad187461aefd72af/docs/gettingstarted/structuring.md
ModDetails ReadMCModTOML(QByteArray contents)
{
    ModDetails details;

    toml::table tomlData;
#if TOML_EXCEPTIONS
    try {
        tomlData = toml::parse(contents.toStdString());
    } catch ([[maybe_unused]] const toml::parse_error& err) {
        return {};
    }
#else
    toml::parse_result result = toml::parse(contents.toStdString());
    if (!result) {
        return {};
    }
    tomlData = result.table();
#endif

    auto tomlModsArr = tomlData["mods"].as_array();
    if (!tomlModsArr) {
        qWarning() << "Corrupted mods.toml? Couldn't find [[mods]] array!";
        return {};
    }

    auto tomlModsTable0 = tomlModsArr->get(0);
    if (!tomlModsTable0) {
        qWarning() << "Corrupted mods.toml? [[mods]] didn't have an element at index 0!";
        return {};
    }
    auto modsTable = tomlModsTable0->as_table();
    if (!modsTable) {
        qWarning() << "Corrupted mods.toml? [[mods]] was not a table!";
        return {};
    }

    if (auto modIdDatum = (*modsTable)["modId"].as_string()) {
        details.mod_id = QString::fromStdString(modIdDatum->get());
    }
    if (auto versionDatum = (*modsTable)["version"].as_string()) {
        details.version = QString::fromStdString(versionDatum->get());
    }
    if (auto displayNameDatum = (*modsTable)["displayName"].as_string()) {
        details.name = QString::fromStdString(displayNameDatum->get());
    }
    if (auto descriptionDatum = (*modsTable)["description"].as_string()) {
        details.description = QString::fromStdString(descriptionDatum->get());
    }

    QString authors = "";
    if (auto authorsDatum = tomlData["authors"].as_string()) {
        authors = QString::fromStdString(authorsDatum->get());
    } else if (auto authorsDatumMods = (*modsTable)["authors"].as_string()) {
        authors = QString::fromStdString(authorsDatumMods->get());
    }
    if (!authors.isEmpty()) {
        details.authors.append(authors);
    }

    QString homeurl = "";
    if (auto homeurlDatum = tomlData["displayURL"].as_string()) {
        homeurl = QString::fromStdString(homeurlDatum->get());
    } else if (auto homeurlDatumMods = (*modsTable)["displayURL"].as_string()) {
        homeurl = QString::fromStdString(homeurlDatumMods->get());
    }
    if (!homeurl.isEmpty() && !homeurl.startsWith("http://") && !homeurl.startsWith("https://") && !homeurl.startsWith("ftp://")) {
        homeurl.prepend("http://");
    }
    details.homeurl = homeurl;

    QString issueTrackerURL = "";
    if (auto issueTrackerURLDatum = tomlData["issueTrackerURL"].as_string()) {
        issueTrackerURL = QString::fromStdString(issueTrackerURLDatum->get());
    } else if (auto issueTrackerURLDatumMods = (*modsTable)["issueTrackerURL"].as_string()) {
        issueTrackerURL = QString::fromStdString(issueTrackerURLDatumMods->get());
    }
    details.issue_tracker = issueTrackerURL;

    QString license = "";
    if (auto licenseDatum = tomlData["license"].as_string()) {
        license = QString::fromStdString(licenseDatum->get());
    } else if (auto licenseDatumMods = (*modsTable)["license"].as_string()) {
        license = QString::fromStdString(licenseDatumMods->get());
    }
    if (!license.isEmpty())
        details.licenses.append(ModLicense(license));

    QString logoFile = "";
    if (auto logoFileDatum = tomlData["logoFile"].as_string()) {
        logoFile = QString::fromStdString(logoFileDatum->get());
    } else if (auto logoFileDatumMods = (*modsTable)["logoFile"].as_string()) {
        logoFile = QString::fromStdString(logoFileDatumMods->get());
    }
    details.icon_file = logoFile;

    auto parseDep = [&details](toml::array* dependencies) {
        static const QStringList ignoreModIds = { "", "forge", "neoforge", "minecraft" };
        if (!dependencies) {
            return;
        }
        auto isNeoForgeDep = [](toml::table* t) {
            auto type = (*t)["type"].as_string();
            return type && type->get() == "required";
        };
        auto isForgeDep = [](toml::table* t) {
            auto mandatory = (*t)["mandatory"].as_boolean();
            return mandatory && mandatory->get();
        };
        for (auto& dep : *dependencies) {
            auto dep_table = dep.as_table();
            if (!dep_table) {
                continue;
            }
            auto modId = (*dep_table)["modId"].as_string();
            if (!modId || ignoreModIds.contains(QString::fromStdString(modId->get()))) {
                continue;
            }
            if (isNeoForgeDep(dep_table) || isForgeDep(dep_table)) {
                details.dependencies.append(QString::fromStdString(modId->get()));
            }
        }
    };

    if (tomlData.contains("dependencies")) {
        auto depValue = tomlData["dependencies"];
        if (auto array = depValue.as_array()) {
            parseDep(array);
        } else if (auto depTable = depValue.as_table()) {
            auto expectedKey = details.mod_id.toStdString();
            if (!depTable->contains(expectedKey)) {
                if (auto it = depTable->begin(); it != depTable->end()) {
                    expectedKey = it->first;
                }
            }
            if ((array = (*depTable)[expectedKey].as_array())) {
                parseDep(array);
            }
        }
    }

    return details;
}

}  // namespace ModUtils
