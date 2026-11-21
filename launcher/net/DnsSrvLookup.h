#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>

namespace Net {

struct DnsLookupImpl;

struct SrvRecord {
    QString target;
    quint16 port = 0;
    quint16 priority = 0;
    quint16 weight = 0;
};

struct SrvLookupResult {
    bool ok = false;
    QString errorString;
    QList<SrvRecord> records;
};

struct HostLookupResult {
    bool ok = false;
    QString errorString;
    QStringList addresses;
};

/**
 * Cancellation handle for an in-flight asynchronous DNS lookup.
 * Destroyed automatically with its parent context or aborted explicitly.
 */
class DnsLookupToken : public QObject {
    Q_OBJECT
   public:
    explicit DnsLookupToken(QObject* parent = nullptr) : QObject(parent) {}
    ~DnsLookupToken() override;

    void abort();
    [[nodiscard]] bool isAborted() const noexcept { return m_aborted.load(std::memory_order_acquire); }

   private:
    friend class DnsSrvLookup;
    friend struct DnsLookupImpl;
    quint64 m_requestId = 0;
    std::atomic<bool> m_aborted{ false };
};

/**
 * Asynchronous DNS SRV and host address resolver.
 * Uses native WinDNS (DnsQueryEx / DnsCancelQuery) on Windows so Qt6's
 * internal QDnsLookupThreadPool is never initialized or awaited on exit.
 */
class DnsSrvLookup {
   public:
    static DnsLookupToken* lookupSrv(const QString& serviceName,
                                     QObject* context,
                                     std::function<void(SrvLookupResult)> callback);

    static DnsLookupToken* lookupHost(const QString& hostName,
                                      QObject* context,
                                      std::function<void(HostLookupResult)> callback);

    static void cancelAllPending();

   private:
    friend class DnsLookupToken;
    static void cancelRequest(quint64 requestId);
};

}  // namespace Net
