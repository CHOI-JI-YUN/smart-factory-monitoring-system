#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include "ConveyorProtocol.h"

// TcpServer.h
// TCP/IP server: Flask(Python) <-> MFC(C++)

class CTcpServer
{
public:
    CTcpServer();
    ~CTcpServer();

    bool Start(int port, HWND hNotifyWnd);
    void Stop();
    bool IsRunning() const;

    // Send current status to connected client (Flask)
    bool SendToClient(const char* pData, int nLen);
    bool SendJsonStatus(float temp, float humi, int ultr,
                        int motorStep, bool fanOn, bool emgStop);

    // Call after WM_TCP_RECEIVED to get last received JSON
    std::string GetReceivedData() const;

private:
    static DWORD WINAPI AcceptThread(LPVOID pParam);
    static DWORD WINAPI RecvThread(LPVOID pParam);

    SOCKET   m_sockListen;
    SOCKET   m_sockClient;
    HANDLE   m_hAcceptThread;
    HANDLE   m_hRecvThread;
    HWND     m_hNotifyWnd;
    bool     m_bRunning;
    int      m_nPort;

    std::string m_strRecvData;

    void SetClientSocket(SOCKET s);
    void NotifyMainWindow(const std::string& data);
};
