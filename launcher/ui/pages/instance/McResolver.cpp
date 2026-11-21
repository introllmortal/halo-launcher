#include "McResolver.h"

#include <QDebug>
#include <QHostAddress>

#include "net/DnsSrvLookup.h"

McResolver::McResolver(QObject* parent, QString domain, int port) : QObject(parent), m_constrDomain(domain), m_constrPort(port) {}

McResolver::~McResolver()
{
    cancelLookups();
}

void McResolver::cancelLookups()
{
    if (m_srvLookup) {
        m_srvLookup->abort();
        m_srvLookup->deleteLater();
        m_srvLookup.clear();
    }
    if (m_hostLookup) {
        m_hostLookup->abort();
        m_hostLookup->deleteLater();
        m_hostLookup.clear();
    }
}

void McResolver::ping()
{
    pingWithDomainSRV(m_constrDomain, m_constrPort);
}

void McResolver::pingWithDomainSRV(QString domain, int port)
{
    cancelLookups();

    const QHostAddress literalAddr(domain);
    if (!literalAddr.isNull()) {
        pingWithDomainA(domain, port);
        return;
    }

    m_srvLookup = Net::DnsSrvLookup::lookupSrv(QString("_minecraft._tcp.%1").arg(domain), this,
                                             [this, domain, port](const Net::SrvLookupResult& result) {
                                                 if (m_srvLookup) {
                                                     m_srvLookup->deleteLater();
                                                     m_srvLookup.clear();
                                                 }

                                                 if (!result.ok) {
                                                     qDebug() << QString("Warning: SRV record lookup failed (%1), trying A record lookup")
                                                                     .arg(result.errorString);
                                                     pingWithDomainA(domain, port);
                                                     return;
                                                 }

                                                 if (result.records.isEmpty()) {
                                                     qDebug() << "Warning: no SRV entries found for domain, trying A record lookup";
                                                     pingWithDomainA(domain, port);
                                                     return;
                                                 }

                                                 const auto& firstRecord = result.records.at(0);
                                                 pingWithDomainA(firstRecord.target, firstRecord.port);
                                             });
}

void McResolver::pingWithDomainA(QString domain, int port)
{
    if (m_hostLookup) {
        m_hostLookup->abort();
        m_hostLookup->deleteLater();
        m_hostLookup.clear();
    }

    m_hostLookup = Net::DnsSrvLookup::lookupHost(domain, this, [this, port](const Net::HostLookupResult& result) {
        if (m_hostLookup) {
            m_hostLookup->deleteLater();
            m_hostLookup.clear();
        }

        if (!result.ok) {
            emitFail("A record lookup failed");
            return;
        }

        if (result.addresses.isEmpty()) {
            emitFail("No A entries found for domain");
            return;
        }

        emitSucceed(result.addresses.at(0), port);
    });
}

void McResolver::emitFail(QString error)
{
    qDebug() << "DNS resolver error:" << error;
    emit failed(error);
    emit finished();
}

void McResolver::emitSucceed(QString ip, int port)
{
    emit succeeded(ip, port);
    emit finished();
}
