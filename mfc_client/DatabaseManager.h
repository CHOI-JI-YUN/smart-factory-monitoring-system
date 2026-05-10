#pragma once

#include "sqlite3.h"
#include <string>
#include <vector>
#include <queue>
#include <windows.h>

// Alert record structure
struct AlertRecord
{
    int         id;
    std::string timestamp;      // "2025-05-27 14:32:11"
    double      temperature;    // 31.2
    double      humidity;       // 72.4
    std::string status;         // NORMAL / WARNING / DANGER / STOP / DEFECT
    std::string content;        // Description text
};

// Summary count
struct AlertSummary
{
    int totalCount;
    int warningCount;
    int dangerCount;
    int stopCount;
};

class CDatabaseManager
{
public:
    CDatabaseManager();
    ~CDatabaseManager();

    // DB open/close (called once at start/end)
    bool Open(const std::string& dbPath = "conveyor.db");
    void Close();
    bool IsOpen() const;

    // 式式 Synchronous (blocks caller) 式式
    bool InsertAlert(double temp, double humi,
        const std::string& status,
        const std::string& content);

    // 式式 Asynchronous (queues to worker thread, non-blocking) 式式
    void InsertAlertAsync(double temp, double humi,
        const std::string& status,
        const std::string& content);

    // Worker thread control
    void StartWorker();
    void StopWorker();

    // Get all records (newest first)
    std::vector<AlertRecord> GetAlerts(int limit = 200);

    AlertSummary GetTodaySummary();

    bool ClearAll();

    std::string GetLastError() const { return m_strLastError; }

private:
    sqlite3* m_pDB;
    std::string m_strLastError;

    bool CreateTables();
    std::string GetCurrentTimestamp();

    // 式式 Worker thread internals 式式
    struct AsyncAlert
    {
        double      temp;
        double      humi;
        std::string status;
        std::string content;
    };

    HANDLE              m_hWorkerThread;
    HANDLE              m_hQueueEvent;      // signaled when queue has items
    bool                m_bWorkerRunning;
    CRITICAL_SECTION    m_csQueue;
    std::queue<AsyncAlert> m_queue;

    static DWORD WINAPI WorkerThread(LPVOID pParam);
};