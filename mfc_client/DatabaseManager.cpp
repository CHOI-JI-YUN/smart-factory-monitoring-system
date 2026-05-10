// DatabaseManager.cpp
// SQLite DB manager for alert history (with async worker thread)

#include "pch.h"
#include "DatabaseManager.h"
#include <ctime>
#include <sstream>
#include <iomanip>

CDatabaseManager::CDatabaseManager()
    : m_pDB(nullptr)
    , m_hWorkerThread(nullptr)
    , m_hQueueEvent(nullptr)
    , m_bWorkerRunning(false)
{
    InitializeCriticalSection(&m_csQueue);
}

CDatabaseManager::~CDatabaseManager()
{
    StopWorker();
    Close();
    DeleteCriticalSection(&m_csQueue);
}

bool CDatabaseManager::Open(const std::string& dbPath)
{
    if (m_pDB) return true;

    int rc = sqlite3_open(dbPath.c_str(), &m_pDB);
    if (rc != SQLITE_OK)
    {
        m_strLastError = sqlite3_errmsg(m_pDB);
        sqlite3_close(m_pDB);
        m_pDB = nullptr;
        return false;
    }

    // WAL mode (performance)
    sqlite3_exec(m_pDB, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    return CreateTables();
}

void CDatabaseManager::Close()
{
    if (m_pDB)
    {
        sqlite3_close(m_pDB);
        m_pDB = nullptr;
    }
}

bool CDatabaseManager::IsOpen() const
{
    return (m_pDB != nullptr);
}

bool CDatabaseManager::CreateTables()
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS alerts ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp   TEXT    NOT NULL,"
        "  temperature REAL,"
        "  humidity    REAL,"
        "  status      TEXT    NOT NULL,"
        "  content     TEXT"
        ");";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_pDB, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        m_strLastError = errMsg ? errMsg : "CreateTables failed";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

std::string CDatabaseManager::GetCurrentTimestamp()
{
    time_t now = time(nullptr);
    struct tm tmNow;
    localtime_s(&tmNow, &now);

    std::ostringstream oss;
    oss << std::setfill('0')
        << (tmNow.tm_year + 1900) << "-"
        << std::setw(2) << (tmNow.tm_mon + 1) << "-"
        << std::setw(2) << tmNow.tm_mday << " "
        << std::setw(2) << tmNow.tm_hour << ":"
        << std::setw(2) << tmNow.tm_min << ":"
        << std::setw(2) << tmNow.tm_sec;
    return oss.str();
}

// ????????????????????????????????????????????
// Synchronous insert (blocks caller)
// ????????????????????????????????????????????
bool CDatabaseManager::InsertAlert(double temp, double humi,
    const std::string& status,
    const std::string& content)
{
    if (!m_pDB) return false;

    const char* sql =
        "INSERT INTO alerts (timestamp, temperature, humidity, status, content) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        m_strLastError = sqlite3_errmsg(m_pDB);
        return false;
    }

    std::string ts = GetCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, temp);
    sqlite3_bind_double(stmt, 3, humi);
    sqlite3_bind_text(stmt, 4, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, content.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        m_strLastError = sqlite3_errmsg(m_pDB);
        return false;
    }
    return true;
}

// ????????????????????????????????????????????
// Asynchronous insert (queues to worker thread)
// ????????????????????????????????????????????
void CDatabaseManager::InsertAlertAsync(double temp, double humi,
    const std::string& status,
    const std::string& content)
{
    AsyncAlert item;
    item.temp = temp;
    item.humi = humi;
    item.status = status;
    item.content = content;

    EnterCriticalSection(&m_csQueue);
    m_queue.push(item);
    LeaveCriticalSection(&m_csQueue);

    // Wake up worker thread
    if (m_hQueueEvent)
        SetEvent(m_hQueueEvent);
}

// ????????????????????????????????????????????
// Worker thread control
// ????????????????????????????????????????????
void CDatabaseManager::StartWorker()
{
    if (m_hWorkerThread) return;    // already running

    m_bWorkerRunning = true;
    m_hQueueEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_hWorkerThread = CreateThread(nullptr, 0, WorkerThread, this, 0, nullptr);
}

void CDatabaseManager::StopWorker()
{
    if (!m_hWorkerThread) return;

    m_bWorkerRunning = false;
    if (m_hQueueEvent)
        SetEvent(m_hQueueEvent);    // wake up so it can exit

    WaitForSingleObject(m_hWorkerThread, 3000);
    CloseHandle(m_hWorkerThread);
    m_hWorkerThread = nullptr;

    if (m_hQueueEvent)
    {
        CloseHandle(m_hQueueEvent);
        m_hQueueEvent = nullptr;
    }

    // Flush remaining items synchronously before shutdown
    EnterCriticalSection(&m_csQueue);
    while (!m_queue.empty())
    {
        AsyncAlert item = m_queue.front();
        m_queue.pop();
        LeaveCriticalSection(&m_csQueue);

        InsertAlert(item.temp, item.humi, item.status, item.content);

        EnterCriticalSection(&m_csQueue);
    }
    LeaveCriticalSection(&m_csQueue);
}

DWORD WINAPI CDatabaseManager::WorkerThread(LPVOID pParam)
{
    CDatabaseManager* pThis = (CDatabaseManager*)pParam;

    while (pThis->m_bWorkerRunning)
    {
        // Wait for items or stop signal
        WaitForSingleObject(pThis->m_hQueueEvent, 500);

        // Process all queued items
        while (true)
        {
            EnterCriticalSection(&pThis->m_csQueue);
            if (pThis->m_queue.empty())
            {
                LeaveCriticalSection(&pThis->m_csQueue);
                break;
            }
            AsyncAlert item = pThis->m_queue.front();
            pThis->m_queue.pop();
            LeaveCriticalSection(&pThis->m_csQueue);

            // Synchronous insert on worker thread (safe, not blocking UI)
            pThis->InsertAlert(item.temp, item.humi, item.status, item.content);
        }
    }
    return 0;
}

// ????????????????????????????????????????????
// Query methods
// ????????????????????????????????????????????
std::vector<AlertRecord> CDatabaseManager::GetAlerts(int limit)
{
    std::vector<AlertRecord> records;
    if (!m_pDB) return records;

    std::string sql = "SELECT id, timestamp, temperature, humidity, status, content "
        "FROM alerts ORDER BY id DESC LIMIT " + std::to_string(limit) + ";";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_pDB, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return records;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        AlertRecord rec;
        rec.id = sqlite3_column_int(stmt, 0);
        rec.timestamp = (const char*)sqlite3_column_text(stmt, 1);
        rec.temperature = sqlite3_column_double(stmt, 2);
        rec.humidity = sqlite3_column_double(stmt, 3);
        rec.status = (const char*)sqlite3_column_text(stmt, 4);
        rec.content = (const char*)sqlite3_column_text(stmt, 5);
        records.push_back(rec);
    }
    sqlite3_finalize(stmt);
    return records;
}

AlertSummary CDatabaseManager::GetTodaySummary()
{
    AlertSummary summary = { 0, 0, 0, 0 };
    if (!m_pDB) return summary;

    // Today date string
    time_t now = time(nullptr);
    struct tm tmNow;
    localtime_s(&tmNow, &now);
    char dateStr[16];
    sprintf_s(dateStr, "%04d-%02d-%02d",
        tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);

    std::string sql =
        "SELECT status, COUNT(*) FROM alerts "
        "WHERE timestamp LIKE '" + std::string(dateStr) + "%' "
        "AND status != 'DEFECT' "
        "GROUP BY status;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_pDB, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return summary;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        std::string st = (const char*)sqlite3_column_text(stmt, 0);
        int cnt = sqlite3_column_int(stmt, 1);
        summary.totalCount += cnt;

        if (st == "WARNING") summary.warningCount = cnt;
        else if (st == "DANGER") summary.dangerCount = cnt;
        else if (st == "STOP")   summary.stopCount = cnt;
    }
    sqlite3_finalize(stmt);
    return summary;
}

bool CDatabaseManager::ClearAll()
{
    if (!m_pDB) return false;

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_pDB, "DELETE FROM alerts;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        m_strLastError = errMsg ? errMsg : "ClearAll failed";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}