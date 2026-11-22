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

#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>

#include "tasks/Task.h"

class QNetworkReply;

/**
 * Fetches an explicit HTTP byte range [startOffset, endOffset] while following redirects.
 * Immediately aborts if the server ignores Range (HTTP 200 instead of 206), preventing
 * full-archive memory exhaustion.
 */
class HttpRangeRequest : public Task {
    Q_OBJECT
   public:
    HttpRangeRequest(QNetworkAccessManager* network,
                     const QUrl& url,
                     quint64 startOffset,
                     quint64 endOffset,
                     QObject* parent = nullptr);
    ~HttpRangeRequest() override;

    bool abort() override;
    bool canAbort() const override { return true; }

    [[nodiscard]] QByteArray payload() const { return m_buffer; }
    [[nodiscard]] quint64 totalRemoteSize() const { return m_totalRemoteSize; }
    [[nodiscard]] bool rangeNotSupported() const { return m_rangeNotSupported; }
    [[nodiscard]] int httpStatus() const { return m_httpStatus; }

    static bool parseContentRange(const QByteArray& headerValue, quint64& outStart, quint64& outEnd, quint64& outTotal);

   protected:
    void executeTask() override;

   private slots:
    void onMetaDataChanged();
    void onReadyRead();
    void onFinished();

   private:
    void dispatchUrl(const QUrl& targetUrl);
    void releaseReply();

    QPointer<QNetworkAccessManager> m_network;
    QUrl m_url;
    quint64 m_startOffset = 0;
    quint64 m_endOffset = 0;
    int m_redirectCount = 0;

    QPointer<QNetworkReply> m_reply;
    QByteArray m_buffer;
    quint64 m_totalRemoteSize = 0;
    int m_httpStatus = 0;
    bool m_headersValidated = false;
    bool m_rangeNotSupported = false;
    bool m_abortedInternally = false;
};
