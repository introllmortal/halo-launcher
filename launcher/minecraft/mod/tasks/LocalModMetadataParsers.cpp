#include "LocalModParseTask.h"

#include <qdcss.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "Json.h"
#include "minecraft/mod/ModDetails.h"
#include "settings/INIFile.h"

namespace ModUtils {

// https://fabricmc.net/wiki/documentation:fabric_mod_json
ModDetails ReadFabricModInfo(QByteArray contents)
{
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
    auto object = jsonDoc.object();
    auto schemaVersion = object.contains("schemaVersion") ? object.value("schemaVersion").toInt(0) : 0;

    ModDetails details;

    details.mod_id = object.value("id").toString();
    details.version = object.value("version").toString();

    details.name = object.contains("name") ? object.value("name").toString() : details.mod_id;
    details.description = object.value("description").toString();

    if (schemaVersion >= 1) {
        QJsonArray authors = object.value("authors").toArray();
        for (auto author : authors) {
            if (author.isObject()) {
                details.authors.append(author.toObject().value("name").toString());
            } else {
                details.authors.append(author.toString());
            }
        }

        if (object.contains("contact")) {
            QJsonObject contact = object.value("contact").toObject();

            if (contact.contains("homepage")) {
                details.homeurl = contact.value("homepage").toString();
            }
            if (contact.contains("issues")) {
                details.issue_tracker = contact.value("issues").toString();
            }
        }

        if (object.contains("license")) {
            auto license = object.value("license");
            if (license.isArray()) {
                for (auto l : license.toArray()) {
                    if (l.isString()) {
                        details.licenses.append(ModLicense(l.toString()));
                    } else if (l.isObject()) {
                        auto obj = l.toObject();
                        details.licenses.append(ModLicense(obj.value("name").toString(), obj.value("id").toString(),
                                                           obj.value("url").toString(), obj.value("description").toString()));
                    }
                }
            } else if (license.isString()) {
                details.licenses.append(ModLicense(license.toString()));
            } else if (license.isObject()) {
                auto obj = license.toObject();
                details.licenses.append(ModLicense(obj.value("name").toString(), obj.value("id").toString(), obj.value("url").toString(),
                                                   obj.value("description").toString()));
            }
        }

        if (object.contains("icon")) {
            auto icon = object.value("icon");
            if (icon.isObject()) {
                auto obj = icon.toObject();
                // take the largest icon
                int largest = 0;
                for (auto key : obj.keys()) {
                    auto size = key.split('x').first().toInt();
                    if (size > largest) {
                        largest = size;
                    }
                }
                if (largest > 0) {
                    auto key = QString::number(largest) + "x" + QString::number(largest);
                    details.icon_file = obj.value(key).toString();
                } else {
                    if (auto it = obj.begin(); it != obj.end()) {
                        details.icon_file = it->toString();
                    }
                }
            } else if (icon.isString()) {
                details.icon_file = icon.toString();
            }
        }

        if (object.contains("depends")) {
            auto depends = object.value("depends");
            if (depends.isObject()) {
                auto obj = depends.toObject();
                for (auto key : obj.keys()) {
                    if (key != "fabricloader" && key != "minecraft" && !key.startsWith("fabric-")) {
                        details.dependencies.append(key);
                    }
                }
            }
        }
    }
    return details;
}

// https://github.com/QuiltMC/rfcs/blob/master/specification/0002-quilt.mod.json.md
ModDetails ReadQuiltModInfo(QByteArray contents)
{
    ModDetails details;
    try {
        QJsonParseError jsonError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
        auto object = Json::requireObject(jsonDoc, "quilt.mod.json");
        auto schemaVersion = object.value("schema_version").toInt();

        if (schemaVersion == 1) {
            auto modInfo = Json::requireObject(object.value("quilt_loader"), "Quilt mod info");

            details.mod_id = Json::requireString(modInfo.value("id"), "Mod ID");
            details.version = Json::requireString(modInfo.value("version"), "Mod version");

            auto modMetadata = modInfo.value("metadata").toObject();

            details.name = modMetadata.value("name").toString(details.mod_id);
            details.description = modMetadata.value("description").toString();

            auto modContributors = modMetadata.value("contributors").toObject();

            details.authors += modContributors.keys();

            auto modContact = modMetadata.value("contact").toObject();

            if (modContact.contains("homepage")) {
                details.homeurl = Json::requireString(modContact.value("homepage"));
            }
            if (modContact.contains("issues")) {
                details.issue_tracker = Json::requireString(modContact.value("issues"));
            }

            if (modMetadata.contains("license")) {
                auto license = modMetadata.value("license");
                if (license.isArray()) {
                    for (auto l : license.toArray()) {
                        if (l.isString()) {
                            details.licenses.append(ModLicense(l.toString()));
                        } else if (l.isObject()) {
                            auto obj = l.toObject();
                            details.licenses.append(ModLicense(obj.value("name").toString(), obj.value("id").toString(),
                                                               obj.value("url").toString(), obj.value("description").toString()));
                        }
                    }
                } else if (license.isString()) {
                    details.licenses.append(ModLicense(license.toString()));
                } else if (license.isObject()) {
                    auto obj = license.toObject();
                    details.licenses.append(ModLicense(obj.value("name").toString(), obj.value("id").toString(),
                                                       obj.value("url").toString(), obj.value("description").toString()));
                }
            }

            if (modMetadata.contains("icon")) {
                auto icon = modMetadata.value("icon");
                if (icon.isObject()) {
                    auto obj = icon.toObject();
                    int largest = 0;
                    for (auto key : obj.keys()) {
                        auto size = key.split('x').first().toInt();
                        if (size > largest) {
                            largest = size;
                        }
                    }
                    if (largest > 0) {
                        auto key = QString::number(largest) + "x" + QString::number(largest);
                        details.icon_file = obj.value(key).toString();
                    } else {
                        if (auto it = obj.begin(); it != obj.end()) {
                            details.icon_file = it->toString();
                        }
                    }
                } else if (icon.isString()) {
                    details.icon_file = icon.toString();
                }
            }
            if (object.contains("depends")) {
                auto depends = object.value("depends");
                if (depends.isArray()) {
                    auto array = depends.toArray();
                    for (auto obj : array) {
                        QString modId;
                        if (obj.isString()) {
                            modId = obj.toString();
                        } else if (obj.isObject()) {
                            auto objValue = obj.toObject();
                            modId = objValue.value("id").toString();
                            if (objValue.contains("optional") && objValue.value("optional").toBool()) {
                                continue;
                            }
                        } else {
                            continue;
                        }
                        if (modId != "minecraft" && !modId.startsWith("quilt_")) {
                            details.dependencies.append(modId);
                        }
                    }
                }
            }
        }

    } catch (const Exception& e) {
        qWarning() << "Unable to parse mod info:" << e.cause();
    }
    return details;
}

ModDetails ReadForgeInfo(QByteArray contents)
{
    ModDetails details;
    details.name = "Minecraft Forge";
    details.mod_id = "Forge";
    details.homeurl = "http://www.minecraftforge.net/forum/";
    INIFile ini;
    if (!ini.loadFile(contents))
        return details;

    QString major = ini.get("forge.major.number", "0").toString();
    QString minor = ini.get("forge.minor.number", "0").toString();
    QString revision = ini.get("forge.revision.number", "0").toString();
    QString build = ini.get("forge.build.number", "0").toString();

    details.version = major + "." + minor + "." + revision + "." + build;
    return details;
}

ModDetails ReadLiteModInfo(QByteArray contents)
{
    ModDetails details;
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
    auto object = jsonDoc.object();
    if (object.contains("name")) {
        details.mod_id = details.name = object.value("name").toString();
    }
    if (object.contains("version")) {
        details.version = object.value("version").toString("");
    } else {
        details.version = object.value("revision").toString("");
    }
    details.mcversion = object.value("mcversion").toString();
    auto author = object.value("author").toString();
    if (!author.isEmpty()) {
        details.authors.append(author);
    }
    details.description = object.value("description").toString();
    details.homeurl = object.value("url").toString();
    return details;
}

// https://git.sleeping.town/unascribed/NilLoader/src/commit/d7fc87b255fc31019ff90f80d45894927fac6efc/src/main/java/nilloader/api/NilMetadata.java#L64
ModDetails ReadNilModInfo(QByteArray contents, QString fname)
{
    ModDetails details;

    QDCSS cssData = QDCSS(contents);
    auto name = cssData.get("@nilmod.name");
    auto desc = cssData.get("@nilmod.description");
    auto authors = cssData.get("@nilmod.authors");

    if (name->has_value()) {
        details.name = name->value();
    }
    if (desc->has_value()) {
        details.description = desc->value();
    }
    if (authors->has_value()) {
        details.authors.append(authors->value());
    }
    details.version = cssData.get("@nilmod.version")->value_or("?");

    details.mod_id = fname.remove(".nilmod.css");

    return details;
}

}  // namespace ModUtils
