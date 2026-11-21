// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2024 Leia uwu <leia@tutamail.com>
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

#include "PrintServers.h"

#include "net/DnsSrvLookup.h"

PrintServers::PrintServers(LaunchTask* parent, const QStringList& servers) : LaunchStep(parent), m_servers(servers) {}

void PrintServers::clearLookups()
{
    for (const auto& token : m_lookups) {
        if (token) {
            token->abort();
            token->deleteLater();
        }
    }
    m_lookups.clear();
}

void PrintServers::executeTask()
{
    clearLookups();
    m_server_to_address.clear();

    if (m_servers.isEmpty()) {
        emitSucceeded();
        return;
    }

    m_pendingCount = m_servers.size();
    for (const QString& server : m_servers) {
        m_lookups.append(Net::DnsSrvLookup::lookupHost(
            server, this, [this, server](const Net::HostLookupResult& hostInfo) { resolveServer(server, hostInfo); }));
    }
}

bool PrintServers::abort()
{
    clearLookups();
    m_pendingCount = 0;
    emitAborted();
    return true;
}

void PrintServers::resolveServer(const QString& server, const Net::HostLookupResult& hostInfo)
{
    if (isFinished()) {
        return;
    }

    QString addresses = server + " resolves to:\n  ";
    if (hostInfo.ok && !hostInfo.addresses.isEmpty()) {
        addresses += hostInfo.addresses.join(", ");
    } else {
        addresses += "N/A";
    }
    addresses += "\n";

    m_server_to_address.insert(server, addresses);

    if (--m_pendingCount <= 0) {
        for (const QString& serv : m_servers) {
            emit logLine(m_server_to_address.value(serv), MessageLevel::Launcher);
        }
        clearLookups();
        emitSucceeded();
    }
}

bool PrintServers::canAbort() const
{
    return true;
}
