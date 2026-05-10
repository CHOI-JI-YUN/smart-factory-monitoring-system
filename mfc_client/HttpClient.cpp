// HttpClient.cpp
// Simple HTTP client implementation (WinSock raw socket)
// Thread-safe: PostStatusAsync queues to background worker thread

#include "pch.h"
#include "ConveyorProtocol.h"
#include "HttpClient.h"
#include <sstream>
#include <iomanip>

#pragma comment(lib, "Ws2_32.lib")

CHttpClient::CHttpClient()
    : m_strHost(HTTP_FLASK_HOST)
    , m_nPort(HTTP_FLASK_PORT)
    , m_nLastCode(0)
    , m_hWorkerThread(nullptr)
    , m_bWorkerRunning(false)
    , m_hQueueEvent(nullptr)
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    InitializeCriticalSection(&m_csQueue);
    m_hQueueEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

CHttpClient::~CHttpClient()
{
    StopWorker();
    if (m_hQueueEvent)
    {
        CloseHandle(m_hQueueEvent);
        m_hQueueEvent = nullptr;
    }
    DeleteCriticalSection(&m_csQueue);
    WSACleanup();
}

void CHttpClient::StartWorker()
{
    if (m_bWorkerRunning) return;
    m_bWorkerRunning = true;
    m_hWorkerThread = CreateThread(nullptr, 0, WorkerThread, this, 0, nullptr);
}

void CHttpClient::StopWorker()
{
    if (!m_bWorkerRunning) return;
    m_bWorkerRunning = false;
    SetEvent(m_hQueueEvent);    // wake up worker to exit

    if (m_hWorkerThread)
    {
        WaitForSingleObject(m_hWorkerThread, 3000);
        CloseHandle(m_hWorkerThread);
        m_hWorkerThread = nullptr;
    }

    // Drain remaining queue
    EnterCriticalSection(&m_csQueue);
    while (!m_postQueue.empty())
        m_postQueue.pop();
    LeaveCriticalSection(&m_csQueue);
}

void CHttpClient::SetServer(const std::string& host, int port)
{
    m_strHost = host;
    m_nPort   = port;
}

void CHttpClient::PostStatusAsync(float temp, float humi, int distance,
    int motorStep, bool conveyorOn, bool fanOn, bool emgStop)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "{"
        << "\"temp\":" << temp << ","
        << "\"humi\":" << humi << ","
        << "\"distance\":" << distance << ","
        << "\"speed\":" << motorStep << ","
        << "\"conveyor_on\":" << (conveyorOn ? "true" : "false") << ","
        << "\"fan\":" << (fanOn ? "true" : "false") << ","
        << "\"emg_stop\":" << (emgStop ? "true" : "false")
        << "}";

    EnterCriticalSection(&m_csQueue);
    while (!m_postQueue.empty())  m_postQueue.pop();
    m_postQueue.push(oss.str());
    LeaveCriticalSection(&m_csQueue);

    SetEvent(m_hQueueEvent);
}

DWORD WINAPI CHttpClient::WorkerThread(LPVOID pParam)
{
    CHttpClient* pThis = (CHttpClient*)pParam;

    while (pThis->m_bWorkerRunning)
    {
        WaitForSingleObject(pThis->m_hQueueEvent, 1000);

        while (pThis->m_bWorkerRunning)
        {
            std::string body;

            EnterCriticalSection(&pThis->m_csQueue);
            if (!pThis->m_postQueue.empty())
            {
                body = pThis->m_postQueue.front();
                pThis->m_postQueue.pop();
            }
            LeaveCriticalSection(&pThis->m_csQueue);

            if (body.empty()) break;

            std::string response;
            pThis->SendRequest("POST", HTTP_ENDPOINT_STATUS, body, response);
        }
    }
    return 0;
}

bool CHttpClient::PostStatus(float temp, float humi, int distance,
    int motorStep, bool conveyorOn, bool fanOn, bool emgStop)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "{"
        << "\"temp\":" << temp << ","
        << "\"humi\":" << humi << ","
        << "\"distance\":" << distance << ","
        << "\"speed\":" << motorStep << ","
        << "\"conveyor_on\":" << (conveyorOn ? "true" : "false") << ","
        << "\"fan\":" << (fanOn ? "true" : "false") << ","
        << "\"emg_stop\":" << (emgStop ? "true" : "false")
        << "}";

    std::string body = oss.str();
    std::string response;
    return SendRequest("POST", HTTP_ENDPOINT_STATUS, body, response);
}

bool CHttpClient::GetControl(std::string& outJson)
{
    std::string response;
    if (!SendRequest("GET", HTTP_ENDPOINT_CONTROL, "", response))
        return false;
    outJson = ExtractBody(response);
    return (m_nLastCode == 200);
}

bool CHttpClient::SendRequest(const std::string& method,
                               const std::string& path,
                               const std::string& body,
                               std::string& outResponse)
{
    m_nLastCode = 0;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        m_strLastError = "Socket create failed";
        return false;
    }

    // Use DWORD for setsockopt, renamed to avoid Windows macro conflict
    DWORD sockTmMs = 2000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&sockTmMs, sizeof(sockTmMs));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
               (const char*)&sockTmMs, sizeof(sockTmMs));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)m_nPort);

    if (inet_pton(AF_INET, m_strHost.c_str(), &addr.sin_addr) != 1)
    {
        m_strLastError = "Invalid server address: " + m_strHost;
        closesocket(sock);
        return false;
    }

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        m_strLastError = "Flask connect failed: " + m_strHost;
        closesocket(sock);
        return false;
    }

    std::ostringstream req;
    req << method << " " << path << " HTTP/1.1\r\n";
    req << "Host: " << m_strHost << ":" << m_nPort << "\r\n";
    req << "Content-Type: application/json\r\n";
    req << "Connection: close\r\n";
    if (!body.empty())
        req << "Content-Length: " << body.size() << "\r\n";
    req << "\r\n";
    if (!body.empty())
        req << body;

    std::string reqStr = req.str();
    if (send(sock, reqStr.c_str(), (int)reqStr.size(), 0) == SOCKET_ERROR)
    {
        m_strLastError = "Send failed";
        closesocket(sock);
        return false;
    }

    outResponse.clear();
    char buf[2048];
    int  nRecv;
    while ((nRecv = recv(sock, buf, sizeof(buf) - 1, 0)) > 0)
    {
        buf[nRecv] = '\0';
        outResponse += buf;
    }

    closesocket(sock);
    m_nLastCode = ParseStatusCode(outResponse);
    return (m_nLastCode >= 200 && m_nLastCode < 300);
}

int CHttpClient::ParseStatusCode(const std::string& response)
{
    size_t pos = response.find("HTTP/");
    if (pos == std::string::npos) return 0;
    pos = response.find(' ', pos);
    if (pos == std::string::npos) return 0;
    return std::stoi(response.substr(pos + 1, 3));
}

std::string CHttpClient::ExtractBody(const std::string& response)
{
    size_t pos = response.find("\r\n\r\n");
    if (pos == std::string::npos) return "";
    return response.substr(pos + 4);
}
