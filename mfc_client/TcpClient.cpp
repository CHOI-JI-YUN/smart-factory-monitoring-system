// TcpClient.cpp
// TCP/IP client implementation
// MFC connects TO Flask TCP server (port 9000)

#include "pch.h"
#include "ConveyorProtocol.h"
#include "TcpClient.h"
#include <sstream>
#include <iomanip>

#pragma comment(lib, "Ws2_32.lib")

CTcpClient::CTcpClient()
    : m_sock(INVALID_SOCKET)
    , m_hConnectThread(nullptr)
    , m_hRecvThread(nullptr)
    , m_hNotifyWnd(nullptr)
    , m_bRunning(false)
    , m_bConnected(false)
    , m_strHost(TCP_FLASK_HOST)
    , m_nPort(TCP_FLASK_PORT)
{
    // WSAStartup already called by HttpClient (Windows ref counting)
    // Duplicate call is safe for standalone execution
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    InitializeCriticalSection(&m_cs);
}

CTcpClient::~CTcpClient()
{
    Disconnect();
    DeleteCriticalSection(&m_cs);
    // Do not call WSACleanup - HttpClient destructor handles final cleanup
    // (Double cleanup may prematurely release sockets still in use)
}

bool CTcpClient::Connect(const std::string& host, int port, HWND hNotifyWnd)
{
    if (m_bRunning) return true;

    m_strHost    = host;
    m_nPort      = port;
    m_hNotifyWnd = hNotifyWnd;
    m_bRunning   = true;

    // Start connect thread (handles connection + auto-reconnect)
    m_hConnectThread = CreateThread(nullptr, 0, ConnectThread, this, 0, nullptr);
    return (m_hConnectThread != nullptr);
}

void CTcpClient::Disconnect()
{
    m_bRunning   = false;
    m_bConnected = false;

    if (m_sock != INVALID_SOCKET)
    {
        shutdown(m_sock, SD_BOTH);
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }

    if (m_hRecvThread)
    {
        WaitForSingleObject(m_hRecvThread, 2000);
        CloseHandle(m_hRecvThread);
        m_hRecvThread = nullptr;
    }
    if (m_hConnectThread)
    {
        WaitForSingleObject(m_hConnectThread, 3000);
        CloseHandle(m_hConnectThread);
        m_hConnectThread = nullptr;
    }
}

bool CTcpClient::IsConnected() const { return m_bConnected; }

bool CTcpClient::DoConnect()
{
    // Close existing socket if any
    if (m_sock != INVALID_SOCKET)
    {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }

    m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_sock == INVALID_SOCKET) return false;

    // Set send timeout only (recv will block until data arrives or socket is closed)
    DWORD sendTmMs = 3000;
    setsockopt(m_sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&sendTmMs, sizeof(sendTmMs));
    // NO SO_RCVTIMEO — recv() must block indefinitely.
    // Flask sends PING every 30s; a short recv timeout would cause
    // false disconnections every few seconds.

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)m_nPort);

    if (inet_pton(AF_INET, m_strHost.c_str(), &addr.sin_addr) != 1)
    {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }

    if (connect(m_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }

    m_bConnected = true;
    return true;
}

bool CTcpClient::SendToServer(const char* pData, int nLen)
{
    EnterCriticalSection(&m_cs);
    bool ok = false;
    if (m_bConnected && m_sock != INVALID_SOCKET)
    {
        int sent = send(m_sock, pData, nLen, 0);
        ok = (sent == nLen);
        if (!ok)
            m_bConnected = false;  // Mark disconnected on send failure
    }
    LeaveCriticalSection(&m_cs);
    return ok;
}

bool CTcpClient::SendMessage(const std::string& msg)
{
    std::string data = msg + "\n";
    return SendToServer(data.c_str(), (int)data.size());
}

bool CTcpClient::SendJsonStatus(float temp, float humi, int ultr,
                                int motorStep, bool fanOn, bool emgStop)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "{"
        << "\"temp\":"     << temp      << ","
        << "\"humi\":"     << humi      << ","
        << "\"distance\":" << ultr      << ","
        << "\"speed\":"    << motorStep << ","
        << "\"fan\":"      << (fanOn    ? "true" : "false") << ","
        << "\"emg_stop\":" << (emgStop  ? "true" : "false")
        << "}\n";
    std::string json = oss.str();
    return SendToServer(json.c_str(), (int)json.size());
}

std::string CTcpClient::GetReceivedData() const
{
    return m_strRecvData;
}

void CTcpClient::NotifyMainWindow(const std::string& data)
{
    m_strRecvData = data;
    if (m_hNotifyWnd)
        PostMessage(m_hNotifyWnd, WM_TCP_RECEIVED, 0, 0);
}

// Connect thread: tries to connect, then auto-reconnects on disconnect
DWORD WINAPI CTcpClient::ConnectThread(LPVOID pParam)
{
    CTcpClient* pThis = (CTcpClient*)pParam;

    while (pThis->m_bRunning)
    {
        if (!pThis->m_bConnected)
        {
            if (pThis->DoConnect())
            {
                // Connected! Start receive thread
                if (pThis->m_hRecvThread)
                {
                    WaitForSingleObject(pThis->m_hRecvThread, 1000);
                    CloseHandle(pThis->m_hRecvThread);
                }
                pThis->m_hRecvThread = CreateThread(nullptr, 0,
                                                    RecvThread, pThis, 0, nullptr);
                // Notify main window of connection
                pThis->NotifyMainWindow("TCP:CONNECTED");
            }
            else
            {
                // Connection failed, retry after 3 seconds
                for (int i = 0; i < 30 && pThis->m_bRunning; i++)
                    Sleep(100);
            }
        }
        else
        {
            // Already connected, check periodically
            Sleep(1000);
        }
    }
    return 0;
}

// Receive thread: reads data from Flask TCP server
DWORD WINAPI CTcpClient::RecvThread(LPVOID pParam)
{
    CTcpClient* pThis = (CTcpClient*)pParam;
    char buf[1024];
    std::string buffer;

    while (pThis->m_bRunning && pThis->m_bConnected)
    {
        memset(buf, 0, sizeof(buf));
        int nRecv = recv(pThis->m_sock, buf, sizeof(buf) - 1, 0);

        if (nRecv <= 0)
        {
            // Disconnected or error
            EnterCriticalSection(&pThis->m_cs);
            pThis->m_bConnected = false;
            if (pThis->m_sock != INVALID_SOCKET)
            {
                closesocket(pThis->m_sock);
                pThis->m_sock = INVALID_SOCKET;
            }
            LeaveCriticalSection(&pThis->m_cs);

            pThis->NotifyMainWindow("TCP:DISCONNECTED");
            break;
        }

        buf[nRecv] = '\0';
        buffer += buf;

        // Process line-based protocol
        while (true)
        {
            size_t pos = buffer.find('\n');
            if (pos == std::string::npos) break;

            std::string line = buffer.substr(0, pos);
            buffer = buffer.substr(pos + 1);

            // Trim \r if present
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.empty()) continue;

            // Handle PING from Flask
            if (line == "PING")
            {
                pThis->SendMessage("PONG");
                continue;
            }

            // Notify main window for all other messages
            // (EMERGENCY:STOP, EMERGENCY:RELEASE, DEFECT:SERVO,
            //  CONVEYOR:ON, CONVEYOR:OFF, ALERT:xxx)
            pThis->NotifyMainWindow(line);

            // Send ACK for commands
            if (line.find("EMERGENCY:") == 0 ||
                line.find("DEFECT:")    == 0 ||
                line.find("CONVEYOR:")  == 0 ||
                line.find("ALERT:")     == 0)
            {
                pThis->SendMessage("ACK:OK");
            }
        }
    }
    return 0;
}
