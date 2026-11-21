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

#include "LookupServerAddress.h"

#include <launch/LaunchTask.h>

#include "net/DnsSrvLookup.h"

LookupServerAddress::LookupServerAddress(LaunchTask* parent) : LaunchStep(parent) {}

void LookupServerAddress::setLookupAddress(const QString& lookupAddress)
{
    m_lookupAddress = lookupAddress;
    m_serviceName = QString("_minecraft._tcp.%1").arg(lookupAddress);
}

void LookupServerAddress::setOutputAddressPtr(MinecraftTarget::Ptr output)
{
    m_output = std::move(output);
}

bool LookupServerAddress::abort()
{
    if (m_dnsLookup) {
        m_dnsLookup->abort();
        m_dnsLookup->deleteLater();
        m_dnsLookup.clear();
    }
    emitAborted();
    return true;
}

void LookupServerAddress::executeTask()
{
    if (m_dnsLookup) {
        m_dnsLookup->abort();
        m_dnsLookup->deleteLater();
        m_dnsLookup.clear();
    }
    m_dnsLookup = Net::DnsSrvLookup::lookupSrv(m_serviceName, this, [this](const Net::SrvLookupResult& result) { on_dnsLookupFinished(result); });
}

void LookupServerAddress::on_dnsLookupFinished(const Net::SrvLookupResult& result)
{
    if (m_dnsLookup) {
        m_dnsLookup->deleteLater();
        m_dnsLookup.clear();
    }

    if (isFinished()) {
        // Aborted
        return;
    }

    if (!result.ok) {
        emit logLine(QString("Failed to resolve server address (this is NOT an error!) %1: %2\n").arg(m_serviceName, result.errorString),
                     MessageLevel::Launcher);
        resolve(m_lookupAddress, 25565);  // Technically the task failed, however, we don't abort the launch
                                          // and leave it up to minecraft to fail (or maybe not) when connecting
        return;
    }

    if (result.records.empty()) {
        emit logLine(QString("Failed to resolve server address %1: the DNS lookup succeeded, but no records were returned.\n")
                         .arg(m_serviceName),
                     MessageLevel::Warning);
        resolve(m_lookupAddress, 25565);  // Technically the task failed, however, we don't abort the launch
                                          // and leave it up to minecraft to fail (or maybe not) when connecting
        return;
    }

    const auto& firstRecord = result.records.at(0);
    quint16 port = firstRecord.port;

    emit logLine(QString("Resolved server address %1 to %2 with port %3\n").arg(m_serviceName, firstRecord.target, QString::number(port)),
                 MessageLevel::Launcher);
    resolve(firstRecord.target, port);
}

void LookupServerAddress::resolve(const QString& address, quint16 port)
{
    m_output->address = address;
    m_output->port = port;

    emitSucceeded();
}
