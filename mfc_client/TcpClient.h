#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include "ConveyorProtocol.h"

// TcpClient.h
// TCP/IP client: MFC connects TO Flask TCP server (port 9000)
// Flask sends commands (EMERGENCY:STOP, DEFECT:SERVO, etc.)
// MFC sends ACK/PONG/status back

class CTcpClient
{
public:
    CTcpClient();
    ~CTcpClient();

    // Connect to Flask TCP server
    bool Connect(const std::string& host, int port, HWND hNotifyWnd);
    void Disconnect();
    bool IsConnected() const;

    // Send data to Flask TCP server
    bool SendToServer(const char* pData, int nLen);
    bool SendMessage(const std::string& msg);
    bool SendJsonStatus(float temp, float humi, int ultr,
                        int motorStep, bool fanOn, bool emgStop);

    // Call after WM_TCP_RECEIVED to get last received data
    std::string GetReceivedData() const;

private:
    static DWORD WINAPI ConnectThread(LPVOID pParam);
    static DWORD WINAPI RecvThread(LPVOID pParam);

    SOCKET      m_sock;
    HANDLE      m_hConnectThread;
    HANDLE      m_hRecvThread;
    HWND        m_hNotifyWnd;
    bool        m_bRunning;
    bool        m_bConnected;

    std::string m_strHost;
    int         m_nPort;
    std::string m_strRecvData;

    CRITICAL_SECTION m_cs;

    bool        DoConnect();
    void        NotifyMainWindow(const std::string& data);
};
