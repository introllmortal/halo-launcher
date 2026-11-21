#include "LocalModParseTask.h"

#include <QRegularExpression>
#include <QString>

#include "FileSystem.h"
#include "archive/ArchiveReader.h"
#include "minecraft/mod/ModDetails.h"

static const QRegularExpression s_newlineRegex("\r\n|\n|\r");

namespace ModUtils {

bool process(Mod& mod, ProcessingLevel level)
{
    switch (mod.type()) {
        case ResourceType::FOLDER:
            return processFolder(mod, level);
        case ResourceType::ZIPFILE:
            return processZIP(mod, level);
        case ResourceType::LITEMOD:
            return processLitemod(mod);
        default:
            qWarning() << "Invalid type" << mod.type() << "for mod parse task!";
            return false;
    }
}

bool processZIP(Mod& mod, [[maybe_unused]] ProcessingLevel level)
{
    ModDetails details;

    MMCZip::ArchiveReader zip(mod.fileinfo().filePath());

    bool baseForgePopulated = false;
    bool isNilMod = false;
    bool isValid = false;
    QString manifestVersion = {};
    QByteArray nilData = {};
    QString nilFilePath = {};

    if (!zip.parse([&details, &baseForgePopulated, &manifestVersion, &isValid, &nilData, &isNilMod, &nilFilePath](
                       MMCZip::ArchiveReader::File* file, bool& stop) {
            auto filePath = file->filename();

            if (filePath == "META-INF/mods.toml" || filePath == "META-INF/neoforge.mods.toml") {
                details = ReadMCModTOML(file->readAll());
                isValid = true;
                if (details.version == "${file.jarVersion}" && !manifestVersion.isEmpty()) {
                    details.version = manifestVersion;
                }
                stop = details.version != "${file.jarVersion}";
                baseForgePopulated = true;
                return true;
            }
            if (filePath == "META-INF/MANIFEST.MF") {
                // quick and dirty line-by-line parser
                auto manifestLines = QString(file->readAll()).split(s_newlineRegex);
                manifestVersion = "";
                for (auto& line : manifestLines) {
                    if (line.startsWith("Implementation-Version: ", Qt::CaseInsensitive)) {
                        manifestVersion = line.remove("Implementation-Version: ", Qt::CaseInsensitive);
                        break;
                    }
                }

                // some mods use ${projectversion} in their build.gradle, causing this mess to show up in MANIFEST.MF
                // also keep with forge's behavior of setting the version to "NONE" if none is found
                if (manifestVersion.contains("task ':jar' property 'archiveVersion'") || manifestVersion == "") {
                    manifestVersion = "NONE";
                }
                if (baseForgePopulated) {
                    details.version = manifestVersion;
                    stop = true;
                }
                return true;
            }
            if (filePath == "mcmod.info") {
                details = ReadMCModInfo(file->readAll());
                isValid = true;
                stop = true;
                return true;
            }
            if (filePath == "quilt.mod.json") {
                details = ReadQuiltModInfo(file->readAll());
                isValid = true;
                stop = true;
                return true;
            }
            if (filePath == "fabric.mod.json") {
                details = ReadFabricModInfo(file->readAll());
                isValid = true;
                stop = true;
                return true;
            }
            if (filePath == "forgeversion.properties") {
                details = ReadForgeInfo(file->readAll());
                isValid = true;
                stop = true;
                return true;
            }
            if (filePath == "META-INF/nil/mappings.json") {
                // nilloader uses the filename of the metadata file for the modid, so we can't know the exact filename
                // thankfully, there is a good file to use as a canary so we don't look for nil meta all the time
                isNilMod = true;
                stop = !nilFilePath.isEmpty();
                file->skip();
                return true;
            }
            // nilmods can shade nilloader to be able to run as a standalone agent - which includes nilloader's own meta file
            if (filePath.endsWith(".nilmod.css") && filePath != "nilloader.nilmod.css") {
                nilData = file->readAll();
                nilFilePath = filePath;
                stop = isNilMod;
                return true;
            }
            file->skip();
            return true;
        })) {
        return false;
    }
    if (isNilMod) {
        details = ReadNilModInfo(nilData, nilFilePath);
        isValid = true;
    }
    if (isValid) {
        mod.setDetails(details);
        return true;
    }
    return false;  // no valid mod found in archive
}

bool processFolder(Mod& mod, [[maybe_unused]] ProcessingLevel level)
{
    ModDetails details;

    QFileInfo mcmod_info(FS::PathCombine(mod.fileinfo().filePath(), "mcmod.info"));
    if (mcmod_info.exists() && mcmod_info.isFile()) {
        QFile mcmod(mcmod_info.filePath());
        if (!mcmod.open(QIODevice::ReadOnly))
            return false;
        auto data = mcmod.readAll();
        if (data.isEmpty() || data.isNull())
            return false;
        details = ReadMCModInfo(data);

        mod.setDetails(details);
        return true;
    }

    return false;  // no valid mcmod.info file found
}

bool processLitemod(Mod& mod, [[maybe_unused]] ProcessingLevel level)
{
    ModDetails details;

    MMCZip::ArchiveReader zip(mod.fileinfo().filePath());

    if (auto file = zip.goToFile("litemod.json"); file) {
        details = ReadLiteModInfo(file->readAll());

        mod.setDetails(details);
        return true;
    }

    return false;  // no valid litemod.json found in archive
}

/** Checks whether a file is valid as a mod or not. */
bool validate(QFileInfo file)
{
    Mod mod{ file };
    return ModUtils::process(mod, ProcessingLevel::BasicInfoOnly) && mod.valid();
}

bool processIconPNG(const Mod& mod, QByteArray&& raw_data, QPixmap* pixmap)
{
    auto img = QImage::fromData(raw_data);
    if (!img.isNull()) {
        *pixmap = mod.setIcon(img);
    } else {
        qWarning() << "Failed to parse mod logo:" << mod.iconPath() << "from" << mod.name();
        return false;
    }
    return true;
}

bool loadIconImage(const QString& filePath, ResourceType type, const QString& iconPath, QImage* outImage)
{
    if (!outImage || iconPath.isEmpty()) {
        return false;
    }

    QByteArray iconBytes;
    switch (type) {
        case ResourceType::FOLDER: {
            QFileInfo iconInfo(FS::PathCombine(filePath, iconPath));
            if (!iconInfo.exists() || !iconInfo.isFile()) {
                return false;
            }
            QFile iconFile(iconInfo.filePath());
            if (!iconFile.open(QIODevice::ReadOnly)) {
                return false;
            }
            iconBytes = iconFile.readAll();
            break;
        }
        case ResourceType::ZIPFILE: {
            MMCZip::ArchiveReader zip(filePath);
            auto file = zip.goToFile(iconPath);
            if (!file) {
                return false;
            }
            iconBytes = file->readAll();
            break;
        }
        default:
            return false;
    }

    QImage img = QImage::fromData(iconBytes);
    if (img.isNull()) {
        return false;
    }
    if (img.width() > 64 || img.height() > 64) {
        img = img.scaled({ 64, 64 }, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }
    *outImage = std::move(img);
    return true;
}

bool loadIconFile(const Mod& mod, QPixmap* pixmap)
{
    if (mod.iconPath().isEmpty()) {
        qWarning() << "No Iconfile set, be sure to parse the mod first";
        return false;
    }

    QImage img;
    if (!loadIconImage(mod.fileinfo().filePath(), mod.type(), mod.iconPath(), &img)) {
        qWarning() << "Mod at" << mod.fileinfo().filePath() << "does not have a valid icon:" << mod.iconPath();
        return false;
    }
    if (pixmap) {
        *pixmap = mod.setIcon(img);
    }
    return true;
}

}  // namespace ModUtils

LocalModParseTask::LocalModParseTask(int token, ResourceType type, const QFileInfo& modFile)
    : Task(false), m_token(token), m_type(type), m_modFile(modFile), m_result(new Result())
{}

bool LocalModParseTask::abort()
{
    m_aborted.store(true);
    return true;
}

void LocalModParseTask::executeTask()
{
    Mod mod{ m_modFile };
    ModUtils::process(mod, ModUtils::ProcessingLevel::Full);

    m_result->details = mod.details();

    if (m_aborted)
        emitAborted();
    else
        emitSucceeded();
}
