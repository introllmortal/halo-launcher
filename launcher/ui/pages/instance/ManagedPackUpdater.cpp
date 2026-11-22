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
#include "ManagedPackUpdater.h"

#include <QDir>
#include <QFileDialog>
#include <QJsonDocument>
#include <QPointer>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "InstanceTask.h"
#include "PackSnapshot.h"
#include "StringUtils.h"
#include "tasks/ManagedFilesIndex.h"
#include "tasks/ZipDeltaPlanner.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"
#include "tasks/DeltaSyncTask.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"

namespace {

bool warnIfInstanceRunning(BaseInstance* inst, QWidget* parent)
{
    if (!inst || !inst->isRunning()) {
        return false;
    }
    CustomMessageBox::selectable(parent, QObject::tr("Game Running"),
                                 QObject::tr("Minecraft is currently running. Please close the game before updating."),
                                 QMessageBox::Warning)
        ->exec();
    return true;
}

void showUpdateOutcomeDialog(BaseInstance* inst, bool didSucceed, QWidget* parent)
{
    const QString versionName = inst ? inst->getManagedPackVersionName() : QString();
    if (didSucceed) {
        CustomMessageBox::selectable(parent,
                                     QObject::tr("Update Successful"),
                                     QObject::tr("The instance updated to pack version %1 successfully.").arg(versionName),
                                     QMessageBox::Information)
            ->show();
    } else {
        CustomMessageBox::selectable(
            parent,
            QObject::tr("Update Failed"),
            QObject::tr("The instance failed to update to pack version %1. Please check launcher logs for more information.")
                .arg(versionName),
            QMessageBox::Critical)
            ->show();
    }
}

}  // namespace

namespace ManagedPackUpdater {

bool runImportUpdate(BaseInstance* inst,
                     const QUrl& url,
                     const QString& versionID,
                     const QString& versionName,
                     QWidget* parent,
                     bool showResultDialog)
{
    if (!inst || warnIfInstanceRunning(inst, parent)) {
        return false;
    }

    const QPointer<BaseInstance> safeInst(inst);
    PackSnapshot::createSnapshot(safeInst, versionName);

    QMap<QString, QString> extraInfo;
    extraInfo.insert(QStringLiteral("pack_id"), safeInst->getManagedPackID());
    extraInfo.insert(QStringLiteral("pack_version_id"), versionID);
    extraInfo.insert(QStringLiteral("original_instance_id"), safeInst->id());

    auto* extracted = new InstanceImportTask(url, parent, std::move(extraInfo));
    if (versionName.isEmpty()) {
        extracted->setName(safeInst->name());
    } else {
        InstanceName instName(safeInst->getManagedPackName(), versionName);
        QString nextName = safeInst->name();
        const QString currentVersionName = safeInst->getManagedPackVersionName();
        if (!currentVersionName.isEmpty()) {
            nextName.replace(currentVersionName, versionName);
        }
        instName.setName(nextName);
        extracted->setName(instName);
    }
    extracted->setGroup(APPLICATION->instances()->getInstanceGroup(safeInst->id()));
    extracted->setIcon(safeInst->iconKey());
    extracted->setConfirmUpdate(false);

    unique_qobject_ptr<Task> wrappedTask(APPLICATION->instances()->wrapInstanceTask(extracted));
    QObject::connect(wrappedTask.get(), &Task::failed, parent, [parent](const QString& reason) {
        CustomMessageBox::selectable(parent, QObject::tr("Error"), reason, QMessageBox::Critical)->show();
    });
    QObject::connect(wrappedTask.get(), &Task::succeeded, parent, [parent, extracted]() {
        const QStringList warnings = extracted->warnings();
        if (!warnings.isEmpty()) {
            CustomMessageBox::selectable(parent, QObject::tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
        }
    });
    QObject::connect(wrappedTask.get(), &Task::aborted, parent, [parent]() {
        CustomMessageBox::selectable(parent, QObject::tr("Task aborted"), QObject::tr("The task has been aborted by the user."),
                                     QMessageBox::Information)
            ->show();
    });

    ProgressDialog loadDialog(parent);
    loadDialog.setSkipButton(true, QObject::tr("Abort"));
    loadDialog.execWithTask(wrappedTask.get());

    const bool didSucceed = wrappedTask->wasSuccessful();
    if (!safeInst) {
        return false;
    }
    if (didSucceed) {
        safeInst->reloadSettings();
        safeInst->setUpdateAvailable(false);
        const QString effectiveVersion =
            safeInst->getManagedPackVersionName().isEmpty() ? versionName : safeInst->getManagedPackVersionName();
        ManagedFilesIndex::save(safeInst->instanceRoot(),
                                ManagedFilesIndex::scanGameRoot(safeInst->gameRoot(), effectiveVersion));
    }
    if (showResultDialog) {
        showUpdateOutcomeDialog(safeInst, didSucceed, parent);
    }
    return didSucceed;
}

bool executeHaloDeltaSync(BaseInstance* inst, const HaloPackVersion& version, QWidget* parent)
{
    if (!inst || warnIfInstanceRunning(inst, parent)) {
        return false;
    }

    const QPointer<BaseInstance> safeInst(inst);
    auto offerFullFallback = [&](const QString& reason) -> bool {
        const auto choice = CustomMessageBox::selectable(
                                parent, QObject::tr("Delta Update Failed"),
                                QObject::tr("Delta update could not be completed (%1).\n\nWould you like to perform a full modpack "
                                            "archive update instead?")
                                    .arg(reason),
                                QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                                ->exec();
        if (choice == QMessageBox::Yes && safeInst) {
            return runImportUpdate(safeInst, version.downloadUrl, version.version, version.version, parent);
        }
        return false;
    };

    QJsonObject manifest = version.deltaManifest;
    if (manifest.isEmpty() && version.deltaManifestUrl.isValid()) {
        auto netJob = makeShared<NetJob>(QObject::tr("Fetch Delta Manifest"), APPLICATION->network());
        auto [action, response] = Net::ApiDownload::makeByteArray(version.deltaManifestUrl);
        netJob->addNetAction(action);

        ProgressDialog fetchDlg(parent);
        fetchDlg.setSkipButton(true, QObject::tr("Abort"));
        fetchDlg.execWithTask(netJob.get());

        if (!safeInst) {
            return false;
        }
        if (!netJob->wasSuccessful()) {
            const QString reason = netJob->failReason().isEmpty() ? QObject::tr("Failed to download delta manifest") : netJob->failReason();
            return offerFullFallback(reason);
        }

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(*response, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return offerFullFallback(QObject::tr("Invalid delta manifest format"));
        }
        manifest = doc.object();
    }

    PackSnapshot::createSnapshot(safeInst, version.version);

    auto* deltaTask = new DeltaSyncTask(safeInst, manifest, version.version, parent);
    ProgressDialog progressDialog(parent);
    progressDialog.setSkipButton(true, QObject::tr("Abort"));
    progressDialog.execWithTask(deltaTask);

    const bool deltaSucceeded = deltaTask->wasSuccessful();
    if (!deltaSucceeded) {
        QString reason = deltaTask->failReason();
        if (reason.isEmpty()) {
            reason = deltaTask->wasAborted() ? QObject::tr("Task aborted by user") : QObject::tr("Unknown error");
        }
        delete deltaTask;
        return safeInst ? offerFullFallback(reason) : false;
    }

    delete deltaTask;
    if (!safeInst) {
        return false;
    }
    safeInst->reloadSettings();
    safeInst->setUpdateAvailable(false);

    CustomMessageBox::selectable(parent, QObject::tr("Update Successful"),
                                 QObject::tr("The instance updated to pack version %1 successfully.").arg(version.version),
                                 QMessageBox::Information)
        ->show();
    return true;
}

bool executeZipDeltaSync(BaseInstance* inst,
                         const ZipDeltaPlan& plan,
                         const QUrl& fallbackUrl,
                         const QString& versionName,
                         QWidget* parent,
                         bool showResultDialog)
{
    if (!inst) {
        return false;
    }

    QPointer<BaseInstance> safeInst(inst);
    PackSnapshot::createSnapshot(safeInst, versionName);

    auto* deltaTask = new DeltaSyncTask(safeInst, plan, APPLICATION->network(), parent);
    ProgressDialog progressDialog(parent);
    progressDialog.setSkipButton(true, QObject::tr("Abort"));
    progressDialog.execWithTask(deltaTask);

    const bool deltaSucceeded = deltaTask->wasSuccessful();
    if (!deltaSucceeded) {
        QString reason = deltaTask->failReason();
        if (reason.isEmpty()) {
            reason = deltaTask->wasAborted() ? QObject::tr("Task aborted by user") : QObject::tr("Unknown error");
        }
        delete deltaTask;
        if (!safeInst) {
            return false;
        }
        auto choice = CustomMessageBox::selectable(
                          parent, QObject::tr("Delta Update Failed"),
                          QObject::tr("Delta update could not be completed (%1).\n\nWould you like to perform a full modpack archive update instead?")
                              .arg(reason),
                          QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                          ->exec();
        if (choice == QMessageBox::Yes) {
            return runImportUpdate(safeInst, fallbackUrl, versionName, versionName, parent, showResultDialog);
        }
        return false;
    }

    delete deltaTask;
    if (!safeInst) {
        return false;
    }
    safeInst->reloadSettings();
    safeInst->setUpdateAvailable(false);

    if (showResultDialog) {
        CustomMessageBox::selectable(parent, QObject::tr("Update Successful"),
                                     QObject::tr("The instance updated to pack version %1 successfully.").arg(versionName),
                                     QMessageBox::Information)
            ->show();
    }
    return true;
}

bool runSmartZipUpdate(BaseInstance* inst,
                       const QUrl& url,
                       const QString& versionID,
                       const QString& versionName,
                       QWidget* parent,
                       bool showResultDialog)
{
    if (!inst) {
        return false;
    }

    QPointer<BaseInstance> safeInst(inst);
    if (url.fileName().endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive) && url.scheme().startsWith(QStringLiteral("http"))) {
        auto* planTask = new ZipDeltaPlanTask(APPLICATION->network(), url, safeInst->instanceRoot(), safeInst->gameRoot(), versionName, parent);
        ProgressDialog planDlg(parent);
        planDlg.setSkipButton(true, QObject::tr("Abort"));
        planDlg.execWithTask(planTask);
        const bool userAborted = planTask->wasAborted();
        const bool planReady = planTask->wasSuccessful() && planTask->plan().valid;
        const ZipDeltaPlan plan = planTask->plan();
        delete planTask;
        if (!safeInst || userAborted) {
            return false;
        }
        if (planReady) {
            return executeZipDeltaSync(safeInst, plan, url, versionName, parent, showResultDialog);
        }
    }
    return runImportUpdate(safeInst, url, versionID, versionName, parent, showResultDialog);
}

bool runOneStepUpdate(BaseInstance* inst, QWidget* parent)
{
    if (!supportsOneStepUpdate(inst)) {
        return false;
    }
    if (warnIfInstanceRunning(inst, parent)) {
        return false;
    }

    QPointer<BaseInstance> guard(inst);
    const QString customUrl = inst->settings()->get(QStringLiteral("ManagedPackURL")).toString().trimmed();

    if (inst->getManagedPackID().isEmpty()) {
        if (!customUrl.isEmpty()) {
            const auto choice = CustomMessageBox::selectable(parent, QObject::tr("Modpack Update"),
                                                             QObject::tr("Update \"%1\" from configured URL?\n%2").arg(inst->name(), customUrl),
                                                             QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                                    ->exec();
            if (choice == QMessageBox::Yes && guard) {
                return runImportUpdate(guard, QUrl(customUrl), {}, {}, parent);
            }
            return false;
        }

        const QUrl fileUrl = QFileDialog::getOpenFileUrl(parent, QObject::tr("Choose update file"), QDir::homePath(),
                                                         QObject::tr("Modpack") + QStringLiteral(" (*.mrpack *.zip)"));
        if (!fileUrl.isEmpty() && guard) {
            return runImportUpdate(guard, fileUrl, {}, {}, parent);
        }
        return false;
    }

    ManagedPackUpdateCandidate candidate;
    bool isUpToDate = false;
    if (!fetchLatestCandidate(inst, parent, candidate, isUpToDate) || !guard) {
        return false;
    }

    if (isUpToDate) {
        guard->setUpdateAvailable(false);
        const QString ver = candidate.versionName.isEmpty() ? guard->getManagedPackVersionName() : candidate.versionName;
        CustomMessageBox::selectable(parent, QObject::tr("Up to Date"),
                                     QObject::tr("\"%1\" is already on the latest version (%2).").arg(guard->name(), ver),
                                     QMessageBox::Information)
            ->exec();
        return false;
    }

    guard->setUpdateAvailable(true);
    if (candidate.isHaloPack && !candidate.haloVersion.hasDeltaManifest &&
        candidate.downloadUrl.fileName().endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive) &&
        candidate.downloadUrl.scheme().startsWith(QStringLiteral("http"))) {
        auto* planTask = new ZipDeltaPlanTask(APPLICATION->network(), candidate.downloadUrl, guard->instanceRoot(), guard->gameRoot(),
                                              candidate.versionName, parent);
        ProgressDialog planDlg(parent);
        planDlg.setSkipButton(true, QObject::tr("Abort"));
        planDlg.execWithTask(planTask);
        const bool userAborted = planTask->wasAborted();
        const bool planReady = planTask->wasSuccessful() && planTask->plan().valid;
        if (planReady) {
            const ZipDeltaPlan plan = planTask->plan();
            candidate.isDelta = true;
            candidate.hasZipDeltaPlan = true;
            candidate.zipDeltaPlan = plan;
            candidate.deltaSummary = QObject::tr("Smart delta update: %1 to download (%2 files changed, %3 unchanged, %4 removed) instead of %5")
                                         .arg(StringUtils::humanReadableFileSize(static_cast<double>(plan.downloadCompressedBytes)))
                                         .arg(plan.entriesToDownload.size())
                                         .arg(plan.skippedCount)
                                         .arg(plan.pathsToRemove.size())
                                         .arg(StringUtils::humanReadableFileSize(static_cast<double>(plan.totalRemoteZipBytes)));
        }
        delete planTask;
        if (!guard || userAborted) {
            return false;
        }
    }

    if (!askConfirmUpdateDialog(guard, candidate, parent) || !guard) {
        return false;
    }
    if (warnIfInstanceRunning(guard, parent)) {
        return false;
    }

    if (candidate.isHaloPack && candidate.haloVersion.hasDeltaManifest) {
        return executeHaloDeltaSync(guard, candidate.haloVersion, parent);
    }
    if (candidate.hasZipDeltaPlan) {
        return executeZipDeltaSync(guard, candidate.zipDeltaPlan, candidate.downloadUrl, candidate.versionName, parent);
    }
    return runImportUpdate(guard, candidate.downloadUrl, candidate.versionId, candidate.versionName, parent);
}

}  // namespace ManagedPackUpdater
