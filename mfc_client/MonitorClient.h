#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <vector>
#include "ConveyorProtocol.h"

// MonitorClient.h
// Fetch latest JPEG frame from Flask /api/monitor/frame
// and display it on MFC Static control (IDC_STATIC_CHART)
// Thread-safe: FetchFrame runs in background thread, result retrieved via GetLatestBitmap

class CMonitorClient
{
public:
    CMonitorClient();
    ~CMonitorClient();

    void SetServer(const std::string& host, int port);

    // Start/Stop background fetch thread
    void StartFetchThread(int intervalMs = 200);
    void StopFetchThread();

    // Fetch latest JPEG frame from Flask (blocking, original)
    bool FetchFrame(std::vector<BYTE>& jpegData);

    // Get latest bitmap from background thread (thread-safe)
    // Returns new HBITMAP ownership to caller, or nullptr if no new frame
    HBITMAP TakeLatestBitmap();

    // Convert JPEG bytes to HBITMAP (GDI+)
    static HBITMAP JpegToBitmap(const std::vector<BYTE>& jpegData);

    // Draw bitmap to Static control
    static void DrawToStatic(HWND hStatic, HBITMAP hBmp);

    std::string GetLastError() const { return m_strLastError; }

private:
    std::string m_strHost;
    int         m_nPort;
    std::string m_strLastError;

    // Background fetch thread
    HANDLE              m_hFetchThread;
    bool                m_bFetchRunning;
    int                 m_nFetchIntervalMs;
    CRITICAL_SECTION    m_csBitmap;
    HBITMAP             m_hLatestBitmap;    // latest fetched bitmap (protected by m_csBitmap)

    static DWORD WINAPI FetchThread(LPVOID pParam);
};
