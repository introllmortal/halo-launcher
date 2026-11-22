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

#include "HttpRangeClient.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

#include "BuildConfig.h"

namespace {
constexpr int kMaxRedirects = 8;
}  // namespace

HttpRangeRequest::HttpRangeRequest(QNetworkAccessManager* network,
                                   const QUrl& url,
                                   quint64 startOffset,
                                   quint64 endOffset,
                                   QObject* parent)
    : Task(parent), m_network(network), m_url(url), m_startOffset(startOffset), m_endOffset(endOffset)
{
    setAbortable(true);
}

HttpRangeRequest::~HttpRangeRequest()
{
    releaseReply();
}

void HttpRangeRequest::releaseReply()
{
    if (!m_reply) {
        return;
    }
    QNetworkReply* old = m_reply.data();
    m_reply.clear();
    disconnect(old, nullptr, this, nullptr);
    if (old->isRunning()) {
        old->abort();
    }
    old->deleteLater();
}

bool HttpRangeRequest::abort()
{
    m_abortedInternally = true;
    releaseReply();
    emitAborted();
    return true;
}

bool HttpRangeRequest::parseContentRange(const QByteArray& headerValue, quint64& outStart, quint64& outEnd, quint64& outTotal)
{
    static const QRegularExpression s_pattern(QStringLiteral("^bytes\\s+(\\d+)-(\\d+)/(\\d+)$"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = s_pattern.match(QString::fromLatin1(headerValue).trimmed());
    if (!match.hasMatch()) {
        return false;
    }
    bool okStart = false;
    bool okEnd = false;
    bool okTotal = false;
    outStart = match.captured(1).toULongLong(&okStart);
    outEnd = match.captured(2).toULongLong(&okEnd);
    outTotal = match.captured(3).toULongLong(&okTotal);
    return okStart && okEnd && okTotal && outEnd >= outStart && outTotal > outEnd;
}

void HttpRangeRequest::executeTask()
{
    if (!m_network) {
        emitFailed(tr("Network subsystem unavailable."));
        return;
    }
    if (!m_url.isValid() || m_endOffset < m_startOffset) {
        emitFailed(tr("Invalid HTTP range request."));
        return;
    }
    m_redirectCount = 0;
    dispatchUrl(m_url);
}

void HttpRangeRequest::dispatchUrl(const QUrl& targetUrl)
{
    releaseReply();
    m_buffer.clear();
    m_headersValidated = false;
    m_rangeNotSupported = false;
    m_httpStatus = 0;
    m_abortedInternally = false;

    const quint64 expectedBytes = m_endOffset - m_startOffset + 1;
    m_buffer.reserve(static_cast<int>(qMin<quint64>(expectedBytes, 8ULL * 1024 * 1024)));

    QNetworkRequest req(targetUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("Range", QStringLiteral("bytes=%1-%2").arg(m_startOffset).arg(m_endOffset).toLatin1());
    req.setRawHeader("Accept-Encoding", "identity");
    req.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);

    m_reply = m_network->get(req);
    connect(m_reply, &QNetworkReply::metaDataChanged, this, &HttpRangeRequest::onMetaDataChanged);
    connect(m_reply, &QNetworkReply::readyRead, this, &HttpRangeRequest::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &HttpRangeRequest::onFinished);
}

void HttpRangeRequest::onMetaDataChanged()
{
    if (!m_reply || m_headersValidated) {
        return;
    }
    m_httpStatus = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (m_httpStatus == 0) {
        return;
    }

    if (m_httpStatus == 301 || m_httpStatus == 302 || m_httpStatus == 303 || m_httpStatus == 307 || m_httpStatus == 308) {
        return;
    }

    m_headersValidated = true;
    if (m_httpStatus != 206) {
        m_rangeNotSupported = (m_httpStatus == 200);
        m_abortedInternally = true;
        m_reply->abort();
        return;
    }

    if (m_reply->hasRawHeader("Content-Range")) {
        quint64 rangeStart = 0;
        quint64 rangeEnd = 0;
        quint64 totalSize = 0;
        if (parseContentRange(m_reply->rawHeader("Content-Range"), rangeStart, rangeEnd, totalSize)) {
            m_totalRemoteSize = totalSize;
        }
    }
}

void HttpRangeRequest::onReadyRead()
{
    if (!m_reply || m_abortedInternally) {
        return;
    }
    if (!m_headersValidated) {
        onMetaDataChanged();
        if (m_abortedInternally) {
            return;
        }
    }
    if (m_httpStatus == 206) {
        m_buffer.append(m_reply->readAll());
        const quint64 expectedBytes = m_endOffset - m_startOffset + 1;
        setProgress(static_cast<qint64>(m_buffer.size()), static_cast<qint64>(expectedBytes));
    }
}

void HttpRangeRequest::onFinished()
{
    if (!m_reply) {
        return;
    }

    if (wasAborted() && !m_abortedInternally) {
        releaseReply();
        return;
    }

    m_httpStatus = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (m_httpStatus == 301 || m_httpStatus == 302 || m_httpStatus == 303 || m_httpStatus == 307 || m_httpStatus == 308) {
        QUrl nextUrl = m_reply->header(QNetworkRequest::LocationHeader).toUrl();
        if (!nextUrl.isValid() && m_reply->hasRawHeader("Location")) {
            nextUrl = QUrl::fromEncoded(m_reply->rawHeader("Location"));
        }
        if (nextUrl.isRelative()) {
            nextUrl = m_reply->url().resolved(nextUrl);
        }
        releaseReply();

        if (!nextUrl.isValid() || m_redirectCount >= kMaxRedirects) {
            emitFailed(tr("Too many or invalid HTTP redirects."));
            return;
        }
        ++m_redirectCount;
        dispatchUrl(nextUrl);
        return;
    }

    if (m_httpStatus == 206) {
        m_buffer.append(m_reply->readAll());
        if (m_reply->hasRawHeader("Content-Range")) {
            quint64 rangeStart = 0;
            quint64 rangeEnd = 0;
            quint64 totalSize = 0;
            if (parseContentRange(m_reply->rawHeader("Content-Range"), rangeStart, rangeEnd, totalSize)) {
                if (rangeStart != m_startOffset || rangeEnd != m_endOffset) {
                    releaseReply();
                    emitFailed(tr("Server returned mismatched HTTP Content-Range."));
                    return;
                }
                m_totalRemoteSize = totalSize;
            }
        }
        const quint64 expectedBytes = m_endOffset - m_startOffset + 1;
        releaseReply();
        if (static_cast<quint64>(m_buffer.size()) != expectedBytes) {
            emitFailed(tr("Incomplete HTTP range response (%1 of %2 bytes).").arg(m_buffer.size()).arg(expectedBytes));
            return;
        }
        emitSucceeded();
        return;
    }

    const QString netErr = m_reply->errorString();
    releaseReply();
    if (m_rangeNotSupported || m_httpStatus == 200) {
        emitFailed(tr("Server does not support HTTP Range requests (status 200)."));
        return;
    }
    emitFailed(tr("HTTP range request failed (%1): %2").arg(m_httpStatus).arg(netErr));
}
