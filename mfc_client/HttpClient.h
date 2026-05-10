#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <queue>
#include "ConveyorProtocol.h"

// HttpClient.h
// Simple HTTP client (WinSock, no external libs)
// MFC -> Flask: POST status / GET control
// Thread-safe: PostStatus queues work to a background thread

class CHttpClient
{
public:
    CHttpClient();
    ~CHttpClient();

    void SetServer(const std::string& host, int port);

    // Start/Stop worker thread
    void StartWorker();
    void StopWorker();

    // POST /api/status (non-blocking: queues to worker thread)
    void PostStatusAsync(float temp, float humi, int distance,
        int motorStep, bool conveyorOn, bool fanOn, bool emgStop);

    // POST /api/status (blocking, original behavior)
    bool PostStatus(float temp, float humi, int distance,
        int motorStep, bool conveyorOn, bool fanOn, bool emgStop);

    // GET /api/control -> outJson
    bool GetControl(std::string& outJson);

    int         GetLastHttpCode() const { return m_nLastCode; }
    std::string GetLastError()    const { return m_strLastError; }

private:
    std::string m_strHost;
    int         m_nPort;
    int         m_nLastCode;
    std::string m_strLastError;

    // Worker thread for async POST
    HANDLE              m_hWorkerThread;
    bool                m_bWorkerRunning;
    CRITICAL_SECTION    m_csQueue;
    HANDLE              m_hQueueEvent;      // signaled when queue has items
    std::queue<std::string> m_postQueue;

    static DWORD WINAPI WorkerThread(LPVOID pParam);

    bool SendRequest(const std::string& method,
                     const std::string& path,
                     const std::string& body,
                     std::string& outResponse);

    int         ParseStatusCode(const std::string& response);
    std::string ExtractBody(const std::string& response);
};
