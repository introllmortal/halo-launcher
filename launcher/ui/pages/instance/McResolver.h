#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

namespace Net {
class DnsLookupToken;
}

// resolve the IP and port of a Minecraft server
class McResolver : public QObject {
    Q_OBJECT

    QString m_constrDomain;
    int m_constrPort;
    QPointer<Net::DnsLookupToken> m_srvLookup;
    QPointer<Net::DnsLookupToken> m_hostLookup;

   public:
    explicit McResolver(QObject* parent, QString domain, int port);
    ~McResolver() override;
    void ping();

   private:
    void cancelLookups();
    void pingWithDomainSRV(QString domain, int port);
    void pingWithDomainA(QString domain, int port);
    void emitFail(QString error);
    void emitSucceed(QString ip, int port);

   signals:
    void succeeded(QString ip, int port);
    void failed(QString error);
    void finished();
};
