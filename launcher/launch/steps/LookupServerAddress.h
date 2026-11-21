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

#pragma once

#include <QObjectPtr.h>
#include <QPointer>
#include <launch/LaunchStep.h>

#include "minecraft/launch/MinecraftTarget.h"

namespace Net {
class DnsLookupToken;
struct SrvLookupResult;
}

class LookupServerAddress : public LaunchStep {
    Q_OBJECT
   public:
    explicit LookupServerAddress(LaunchTask* parent);
    virtual ~LookupServerAddress() = default;

    virtual void executeTask();
    virtual bool abort();
    virtual bool canAbort() const { return true; }

    void setLookupAddress(const QString& lookupAddress);
    void setOutputAddressPtr(MinecraftTarget::Ptr output);

   private:
    void on_dnsLookupFinished(const Net::SrvLookupResult& result);
    void resolve(const QString& address, quint16 port);

    QPointer<Net::DnsLookupToken> m_dnsLookup;
    QString m_lookupAddress;
    QString m_serviceName;
    MinecraftTarget::Ptr m_output;
};
