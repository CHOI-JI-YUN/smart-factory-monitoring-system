// TcpServer.cpp
// TCP/IP server implementation (Flask <-> MFC)

#include "pch.h"
#include "ConveyorProtocol.h"
#include "TcpServer.h"
#include <sstream>
#include <iomanip>

#pragma comment(lib, "Ws2_32.lib")

CTcpServer::CTcpServer()
    : m_sockListen(INVALID_SOCKET)
    , m_sockClient(INVALID_SOCKET)
    , m_hAcceptThread(nullptr)
    , m_hRecvThread(nullptr)
    , m_hNotifyWnd(nullptr)
    , m_bRunning(false)
    , m_nPort(TCP_SERVER_PORT)
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

CTcpServer::~CTcpServer()
{
    Stop();
    WSACleanup();
}

bool CTcpServer::Start(int port, HWND hNotifyWnd)
{
    if (m_bRunning) return true;

    m_nPort      = port;
    m_hNotifyWnd = hNotifyWnd;

    m_sockListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_sockListen == INVALID_SOCKET) return false;

    int opt = 1;
    setsockopt(m_sockListen, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, sizeof(opt));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((u_short)port);

    if (bind(m_sockListen, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(m_sockListen);
        m_sockListen = INVALID_SOCKET;
        return false;
    }

    if (listen(m_sockListen, 1) == SOCKET_ERROR)
    {
        closesocket(m_sockListen);
        m_sockListen = INVALID_SOCKET;
        return false;
    }

    m_bRunning = true;
    m_hAcceptThread = CreateThread(nullptr, 0, AcceptThread, this, 0, nullptr);
    return true;
}

void CTcpServer::Stop()
{
    m_bRunning = false;

    if (m_sockClient != INVALID_SOCKET)
    {
        closesocket(m_sockClient);
        m_sockClient = INVALID_SOCKET;
    }
    if (m_sockListen != INVALID_SOCKET)
    {
        closesocket(m_sockListen);
        m_sockListen = INVALID_SOCKET;
    }
    if (m_hAcceptThread)
    {
        WaitForSingleObject(m_hAcceptThread, 2000);
        CloseHandle(m_hAcceptThread);
        m_hAcceptThread = nullptr;
    }
    if (m_hRecvThread)
    {
        WaitForSingleObject(m_hRecvThread, 2000);
        CloseHandle(m_hRecvThread);
        m_hRecvThread = nullptr;
    }
}

bool CTcpServer::IsRunning() const { return m_bRunning; }

bool CTcpServer::SendToClient(const char* pData, int nLen)
{
    if (m_sockClient == INVALID_SOCKET) return false;
    int sent = send(m_sockClient, pData, nLen, 0);
    return (sent == nLen);
}

bool CTcpServer::SendJsonStatus(float temp, float humi, int ultr,
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
    return SendToClient(json.c_str(), (int)json.size());
}

std::string CTcpServer::GetReceivedData() const
{
    return m_strRecvData;
}

void CTcpServer::SetClientSocket(SOCKET s)
{
    if (m_sockClient != INVALID_SOCKET)
        closesocket(m_sockClient);
    m_sockClient = s;
}

void CTcpServer::NotifyMainWindow(const std::string& data)
{
    m_strRecvData = data;
    if (m_hNotifyWnd)
        PostMessage(m_hNotifyWnd, WM_TCP_RECEIVED, 0, 0);
}

DWORD WINAPI CTcpServer::AcceptThread(LPVOID pParam)
{
    CTcpServer* pThis = (CTcpServer*)pParam;

    while (pThis->m_bRunning)
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(pThis->m_sockListen, &readSet);

        timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        int selRet = select(0, &readSet, nullptr, nullptr, &tv);
        if (selRet <= 0) continue;

        sockaddr_in clientAddr;
        memset(&clientAddr, 0, sizeof(clientAddr));
        int addrLen  = sizeof(clientAddr);
        SOCKET newSk = accept(pThis->m_sockListen,
                              (sockaddr*)&clientAddr, &addrLen);
        if (newSk == INVALID_SOCKET) continue;

        pThis->SetClientSocket(newSk);

        if (pThis->m_hRecvThread)
        {
            WaitForSingleObject(pThis->m_hRecvThread, 1000);
            CloseHandle(pThis->m_hRecvThread);
        }
        pThis->m_hRecvThread = CreateThread(nullptr, 0,
                                            RecvThread, pThis, 0, nullptr);
    }
    return 0;
}

DWORD WINAPI CTcpServer::RecvThread(LPVOID pParam)
{
    CTcpServer* pThis = (CTcpServer*)pParam;
    char buf[1024];

    while (pThis->m_bRunning && pThis->m_sockClient != INVALID_SOCKET)
    {
        memset(buf, 0, sizeof(buf));
        int nRecv = recv(pThis->m_sockClient, buf, sizeof(buf) - 1, 0);
        if (nRecv <= 0)
        {
            closesocket(pThis->m_sockClient);
            pThis->m_sockClient = INVALID_SOCKET;
            break;
        }
        buf[nRecv] = '\0';
        pThis->NotifyMainWindow(std::string(buf, nRecv));
    }
    return 0;
}
