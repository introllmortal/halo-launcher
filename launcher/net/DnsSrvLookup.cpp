#include "net/DnsSrvLookup.h"

#include <QCoreApplication>
#include <QHash>
#include <QHostAddress>
#include <QMetaObject>
#include <QPointer>
#include <QThread>

#include <atomic>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windns.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>
#else
#include <QDnsLookup>
#include <QHostInfo>
#endif

namespace Net {

struct DnsLookupImpl {
    static void clearTokenId(DnsLookupToken* token)
    {
        if (token) {
            token->m_requestId = 0;
        }
    }

    static void markAborted(DnsLookupToken* token)
    {
        if (token) {
            token->m_aborted.store(true, std::memory_order_release);
            token->m_requestId = 0;
        }
    }
};

namespace {

void assertMainThread()
{
    QCoreApplication* app = QCoreApplication::instance();
    Q_ASSERT(!app || QThread::currentThread() == app->thread());
}

quint64 nextRequestId()
{
    assertMainThread();
    static quint64 counter = 0;
    return ++counter;
}

}  // namespace

#ifdef Q_OS_WIN

static_assert(std::is_same_v<PDNS_RECORD, PDNS_RECORDW>, "UNICODE must be defined for WinDNS wide records");

namespace {

enum class QueryKind : quint8 {
    Srv,
    HostA,
    HostAAAA,
};

struct WinQuery {
    quint64 requestId = 0;
    QueryKind kind = QueryKind::Srv;
    std::wstring nameW;
    DNS_QUERY_REQUEST request{};
    DNS_QUERY_RESULT result{};
    DNS_QUERY_CANCEL cancelHandle{};
    std::atomic<bool> osPending{ false };
    std::atomic<bool> osCompleted{ false };
    std::atomic<bool> cancelled{ false };
};

struct ParsedWinDnsResult {
    QueryKind kind = QueryKind::Srv;
    DNS_STATUS status = 0;
    QList<SrvRecord> srvRecords;
    QStringList addresses;
};

struct PendingEntry {
    QString hostName;
    QPointer<DnsLookupToken> token;
    QPointer<QObject> context;
    std::function<void(SrvLookupResult)> srvCallback;
    std::function<void(HostLookupResult)> hostCallback;
    std::shared_ptr<WinQuery> activeQuery;
};

struct DispatchGuard {
    std::mutex mutex;
    bool shuttingDown = false;
    QObject* appObject = nullptr;
};

DispatchGuard& dispatchGuard()
{
    // Intentionally immortal so cancelled WinDNS threadpool callbacks arriving
    // during ExitProcess cannot lock a destroyed std::mutex.
    static auto* guard = new DispatchGuard();
    return *guard;
}

QHash<quint64, PendingEntry>& pendingEntries()
{
    assertMainThread();
    static QHash<quint64, PendingEntry> entries;
    return entries;
}

bool isShuttingDown()
{
    DispatchGuard& guard = dispatchGuard();
    std::lock_guard<std::mutex> lock(guard.mutex);
    return guard.shuttingDown;
}

QString formatDnsStatus(DNS_STATUS status)
{
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD len = FormatMessageW(flags, nullptr, static_cast<DWORD>(status), 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    QString message;
    if (len > 0 && buffer != nullptr) {
        message = QString::fromWCharArray(buffer, static_cast<int>(len)).trimmed();
        LocalFree(buffer);
    }
    if (message.isEmpty()) {
        message = QStringLiteral("DNS query failed (status %1)").arg(status);
    }
    return message;
}

ParsedWinDnsResult parseQueryResult(QueryKind kind, const DNS_QUERY_RESULT* queryResult)
{
    ParsedWinDnsResult parsed;
    parsed.kind = kind;
    if (!queryResult) {
        parsed.status = DNS_ERROR_RCODE_SERVER_FAILURE;
        return parsed;
    }

    parsed.status = queryResult->QueryStatus;
    if (parsed.status != ERROR_SUCCESS || !queryResult->pQueryRecords) {
        return parsed;
    }

    for (PDNS_RECORDW rec = queryResult->pQueryRecords; rec != nullptr; rec = rec->pNext) {
        if (rec->Flags.S.Section != DnsSectionAnswer) {
            continue;
        }
        if (kind == QueryKind::Srv && rec->wType == DNS_TYPE_SRV) {
            QString target = QString::fromWCharArray(rec->Data.SRV.pNameTarget);
            if (target.size() > 1 && target.endsWith(QLatin1Char('.'))) {
                target.chop(1);
            }
            SrvRecord srv;
            srv.target = target;
            srv.port = rec->Data.SRV.wPort;
            srv.priority = rec->Data.SRV.wPriority;
            srv.weight = rec->Data.SRV.wWeight;
            parsed.srvRecords.append(srv);
        } else if (kind == QueryKind::HostA && rec->wType == DNS_TYPE_A) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = rec->Data.A.IpAddress;
            const QHostAddress hostAddr(reinterpret_cast<const sockaddr*>(&addr));
            if (!hostAddr.isNull()) {
                parsed.addresses.append(hostAddr.toString());
            }
        } else if (kind == QueryKind::HostAAAA && rec->wType == DNS_TYPE_AAAA) {
            sockaddr_in6 addr6{};
            addr6.sin6_family = AF_INET6;
            std::memcpy(addr6.sin6_addr.s6_addr, rec->Data.AAAA.Ip6Address.IP6Byte, sizeof(addr6.sin6_addr.s6_addr));
            const QHostAddress hostAddr(reinterpret_cast<const sockaddr*>(&addr6));
            if (!hostAddr.isNull()) {
                parsed.addresses.append(hostAddr.toString());
            }
        }
    }

    if (kind == QueryKind::Srv && parsed.srvRecords.size() > 1) {
        std::sort(parsed.srvRecords.begin(), parsed.srvRecords.end(), [](const SrvRecord& a, const SrvRecord& b) {
            if (a.priority != b.priority) {
                return a.priority < b.priority;
            }
            return a.weight > b.weight;
        });
    }

    return parsed;
}

WORD queryTypeForKind(QueryKind kind)
{
    switch (kind) {
        case QueryKind::Srv:
            return DNS_TYPE_SRV;
        case QueryKind::HostA:
            return DNS_TYPE_A;
        case QueryKind::HostAAAA:
            return DNS_TYPE_AAAA;
    }
    return DNS_TYPE_A;
}

void deliverOnMainThread(quint64 requestId, const ParsedWinDnsResult& parsed);

VOID WINAPI winDnsCompletionRoutine(PVOID queryContext, PDNS_QUERY_RESULT queryResults)
{
    std::unique_ptr<std::shared_ptr<WinQuery>> holder(static_cast<std::shared_ptr<WinQuery>*>(queryContext));
    std::shared_ptr<WinQuery> query = holder ? *holder : nullptr;

    const bool keepResult = query && !query->cancelled.load(std::memory_order_acquire);
    ParsedWinDnsResult parsed;
    if (query) {
        query->osCompleted.store(true, std::memory_order_release);
        if (keepResult) {
            parsed = parseQueryResult(query->kind, queryResults);
        }
    }

    if (queryResults && queryResults->pQueryRecords) {
        DnsRecordListFree(queryResults->pQueryRecords, DnsFreeRecordList);
        queryResults->pQueryRecords = nullptr;
    }

    if (!keepResult) {
        return;
    }

    DispatchGuard& guard = dispatchGuard();
    std::lock_guard<std::mutex> lock(guard.mutex);
    if (guard.shuttingDown || !guard.appObject || query->cancelled.load(std::memory_order_acquire)) {
        return;
    }

    const quint64 id = query->requestId;
    QMetaObject::invokeMethod(
        guard.appObject,
        [id, parsed]() { deliverOnMainThread(id, parsed); },
        Qt::QueuedConnection);
}

void startWinQuery(quint64 requestId, const QString& name, QueryKind kind)
{
    assertMainThread();
    auto it = pendingEntries().find(requestId);
    if (it == pendingEntries().end()) {
        return;
    }

    QObject* app = QCoreApplication::instance();
    {
        DispatchGuard& guard = dispatchGuard();
        std::lock_guard<std::mutex> lock(guard.mutex);
        if (guard.shuttingDown || !app) {
            pendingEntries().erase(it);
            return;
        }
        guard.appObject = app;
    }

    auto query = std::make_shared<WinQuery>();
    query->requestId = requestId;
    query->kind = kind;
    query->nameW = name.toStdWString();
    query->result.Version = DNS_QUERY_RESULTS_VERSION1;

    query->request.Version = DNS_QUERY_REQUEST_VERSION1;
    query->request.QueryName = query->nameW.c_str();
    query->request.QueryType = queryTypeForKind(kind);
    query->request.QueryOptions = DNS_QUERY_STANDARD;
    query->request.pQueryCompletionCallback = &winDnsCompletionRoutine;

    auto* holder = new std::shared_ptr<WinQuery>(query);
    query->request.pQueryContext = holder;
    it->activeQuery = query;

    const DNS_STATUS status = DnsQueryEx(&query->request, &query->result, &query->cancelHandle);
    if (status == DNS_REQUEST_PENDING) {
        query->osPending.store(true, std::memory_order_release);
        return;
    }

    delete holder;
    ParsedWinDnsResult parsed = parseQueryResult(kind, &query->result);
    if (status != ERROR_SUCCESS && parsed.status == ERROR_SUCCESS) {
        parsed.status = status;
    }
    if (query->result.pQueryRecords) {
        DnsRecordListFree(query->result.pQueryRecords, DnsFreeRecordList);
        query->result.pQueryRecords = nullptr;
    }
    query->osCompleted.store(true, std::memory_order_release);

    QMetaObject::invokeMethod(
        app,
        [requestId, parsed]() { deliverOnMainThread(requestId, parsed); },
        Qt::QueuedConnection);
}

void deliverOnMainThread(quint64 requestId, const ParsedWinDnsResult& parsed)
{
    assertMainThread();
    auto it = pendingEntries().find(requestId);
    if (it == pendingEntries().end()) {
        return;
    }

    if ((it->token && it->token->isAborted()) || !it->context || isShuttingDown()) {
        pendingEntries().erase(it);
        return;
    }

    if (parsed.kind == QueryKind::HostA && parsed.addresses.isEmpty()) {
        // Retry with AAAA when no IPv4 A record was returned (e.g., IPv6-only servers).
        const QString host = it->hostName;
        startWinQuery(requestId, host, QueryKind::HostAAAA);
        return;
    }

    if (it->token) {
        DnsLookupImpl::clearTokenId(it->token);
    }
    PendingEntry entry = std::move(*it);
    pendingEntries().erase(it);

    if (entry.srvCallback) {
        SrvLookupResult out;
        out.ok = (parsed.status == ERROR_SUCCESS);
        if (!out.ok) {
            out.errorString = formatDnsStatus(parsed.status);
        }
        out.records = parsed.srvRecords;
        entry.srvCallback(std::move(out));
        return;
    }

    if (entry.hostCallback) {
        HostLookupResult out;
        out.ok = (parsed.status == ERROR_SUCCESS && !parsed.addresses.isEmpty());
        if (!out.ok) {
            out.errorString = (parsed.status != ERROR_SUCCESS)
                                  ? formatDnsStatus(parsed.status)
                                  : QStringLiteral("Host address lookup returned no records");
        }
        out.addresses = parsed.addresses;
        entry.hostCallback(std::move(out));
    }
}

void cancelActiveQuery(const std::shared_ptr<WinQuery>& query)
{
    if (!query) {
        return;
    }
    query->cancelled.store(true, std::memory_order_release);
    if (query->osPending.load(std::memory_order_acquire) && !query->osCompleted.load(std::memory_order_acquire)) {
        DnsCancelQuery(&query->cancelHandle);
    }
}

DnsLookupToken* createTokenAndEntry(const QString& targetName,
                                    QObject* context,
                                    std::function<void(SrvLookupResult)> srvCb,
                                    std::function<void(HostLookupResult)> hostCb,
                                    quint64& outRequestId)
{
    assertMainThread();
    auto* token = new DnsLookupToken(context);
    if (!context || !QCoreApplication::instance() || isShuttingDown()) {
        DnsLookupImpl::markAborted(token);
        outRequestId = 0;
        return token;
    }

    outRequestId = nextRequestId();
    DnsLookupImpl::clearTokenId(token);

    PendingEntry entry;
    entry.hostName = targetName;
    entry.token = token;
    entry.context = context;
    entry.srvCallback = std::move(srvCb);
    entry.hostCallback = std::move(hostCb);
    pendingEntries().insert(outRequestId, std::move(entry));
    return token;
}

}  // namespace

DnsLookupToken::~DnsLookupToken()
{
    abort();
}

void DnsLookupToken::abort()
{
    assertMainThread();
    if (m_aborted.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    if (m_requestId != 0) {
        const quint64 id = m_requestId;
        m_requestId = 0;
        DnsSrvLookup::cancelRequest(id);
    }
}

DnsLookupToken* DnsSrvLookup::lookupSrv(const QString& serviceName,
                                        QObject* context,
                                        std::function<void(SrvLookupResult)> callback)
{
    quint64 id = 0;
    DnsLookupToken* token = createTokenAndEntry(serviceName, context, std::move(callback), {}, id);
    if (id == 0) {
        return token;
    }
    token->m_requestId = id;
    startWinQuery(id, serviceName, QueryKind::Srv);
    return token;
}

DnsLookupToken* DnsSrvLookup::lookupHost(const QString& hostName,
                                         QObject* context,
                                         std::function<void(HostLookupResult)> callback)
{
    quint64 id = 0;
    DnsLookupToken* token = createTokenAndEntry(hostName, context, {}, std::move(callback), id);
    if (id == 0) {
        return token;
    }
    token->m_requestId = id;

    const QHostAddress literal(hostName);
    if (!literal.isNull()) {
        ParsedWinDnsResult parsed;
        parsed.kind = QueryKind::HostAAAA;
        parsed.status = ERROR_SUCCESS;
        parsed.addresses = { literal.toString() };

        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [id, parsed]() { deliverOnMainThread(id, parsed); },
            Qt::QueuedConnection);
        return token;
    }

    startWinQuery(id, hostName, QueryKind::HostA);
    return token;
}

void DnsSrvLookup::cancelRequest(quint64 requestId)
{
    assertMainThread();
    auto it = pendingEntries().find(requestId);
    if (it == pendingEntries().end()) {
        return;
    }
    const std::shared_ptr<WinQuery> query = it->activeQuery;
    if (query) {
        query->cancelled.store(true, std::memory_order_release);
    }
    pendingEntries().erase(it);
    cancelActiveQuery(query);
}

void DnsSrvLookup::cancelAllPending()
{
    assertMainThread();
    std::vector<std::shared_ptr<WinQuery>> activeQueries;
    {
        DispatchGuard& guard = dispatchGuard();
        std::lock_guard<std::mutex> lock(guard.mutex);
        guard.shuttingDown = true;
        guard.appObject = nullptr;

        activeQueries.reserve(static_cast<size_t>(pendingEntries().size()));
        for (auto it = pendingEntries().begin(); it != pendingEntries().end(); ++it) {
            if (it->token) {
                DnsLookupImpl::markAborted(it->token);
            }
            if (it->activeQuery) {
                it->activeQuery->cancelled.store(true, std::memory_order_release);
                activeQueries.push_back(it->activeQuery);
            }
        }
        pendingEntries().clear();
    }

    for (const auto& query : activeQueries) {
        cancelActiveQuery(query);
    }
}

#else

namespace {

bool& nonWinShuttingDown()
{
    assertMainThread();
    static bool shuttingDown = false;
    return shuttingDown;
}

QHash<quint64, QPointer<DnsLookupToken>>& nonWinTokens()
{
    assertMainThread();
    static QHash<quint64, QPointer<DnsLookupToken>> tokens;
    return tokens;
}

}  // namespace

DnsLookupToken::~DnsLookupToken()
{
    abort();
}

void DnsLookupToken::abort()
{
    assertMainThread();
    if (m_aborted.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    if (m_requestId != 0) {
        const quint64 id = m_requestId;
        m_requestId = 0;
        DnsSrvLookup::cancelRequest(id);
    }
    for (auto* lookup : findChildren<QDnsLookup*>(Qt::FindDirectChildrenOnly)) {
        lookup->abort();
    }
}

DnsLookupToken* DnsSrvLookup::lookupSrv(const QString& serviceName,
                                        QObject* context,
                                        std::function<void(SrvLookupResult)> callback)
{
    assertMainThread();
    auto* token = new DnsLookupToken(context);
    if (!context || !QCoreApplication::instance() || nonWinShuttingDown()) {
        DnsLookupImpl::markAborted(token);
        return token;
    }

    const quint64 id = nextRequestId();
    token->m_requestId = id;
    nonWinTokens().insert(id, token);

    auto* lookup = new QDnsLookup(token);
    lookup->setType(QDnsLookup::SRV);
    lookup->setName(serviceName);
    QPointer<DnsLookupToken> tokenGuard(token);
    QPointer<QObject> contextGuard(context);
    QObject::connect(lookup, &QDnsLookup::finished, token, [id, lookup, tokenGuard, contextGuard, cb = std::move(callback)]() {
        nonWinTokens().remove(id);
        lookup->deleteLater();
        if (!tokenGuard || tokenGuard->isAborted() || !contextGuard || nonWinShuttingDown()) {
            return;
        }
        DnsLookupImpl::clearTokenId(tokenGuard);
        SrvLookupResult result;
        result.ok = (lookup->error() == QDnsLookup::NoError);
        result.errorString = lookup->errorString();
        for (const auto& rec : lookup->serviceRecords()) {
            result.records.append(SrvRecord{ rec.target(), rec.port(), rec.priority(), rec.weight() });
        }
        cb(std::move(result));
    });
    lookup->lookup();
    return token;
}

DnsLookupToken* DnsSrvLookup::lookupHost(const QString& hostName,
                                         QObject* context,
                                         std::function<void(HostLookupResult)> callback)
{
    assertMainThread();
    auto* token = new DnsLookupToken(context);
    if (!context || !QCoreApplication::instance() || nonWinShuttingDown()) {
        DnsLookupImpl::markAborted(token);
        return token;
    }

    const quint64 id = nextRequestId();
    token->m_requestId = id;
    nonWinTokens().insert(id, token);

    QPointer<DnsLookupToken> tokenGuard(token);
    QPointer<QObject> contextGuard(context);
    QHostInfo::lookupHost(hostName, token, [id, tokenGuard, contextGuard, cb = std::move(callback)](const QHostInfo& info) {
        nonWinTokens().remove(id);
        if (!tokenGuard || tokenGuard->isAborted() || !contextGuard || nonWinShuttingDown()) {
            return;
        }
        DnsLookupImpl::clearTokenId(tokenGuard);
        HostLookupResult result;
        result.ok = (info.error() == QHostInfo::NoError && !info.addresses().isEmpty());
        result.errorString = info.errorString();
        for (const auto& addr : info.addresses()) {
            result.addresses.append(addr.toString());
        }
        cb(std::move(result));
    });
    return token;
}

void DnsSrvLookup::cancelRequest(quint64 requestId)
{
    assertMainThread();
    nonWinTokens().remove(requestId);
}

void DnsSrvLookup::cancelAllPending()
{
    assertMainThread();
    nonWinShuttingDown() = true;
    const QList<QPointer<DnsLookupToken>> tokens = nonWinTokens().values();
    nonWinTokens().clear();
    for (const auto& token : tokens) {
        if (token) {
            token->abort();
        }
    }
}

#endif

}  // namespace Net
