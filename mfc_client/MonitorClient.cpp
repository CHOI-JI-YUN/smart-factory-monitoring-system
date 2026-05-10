// MonitorClient.cpp
// Thread-safe: background thread fetches frames, main thread retrieves via TakeLatestBitmap
#include "pch.h"
#include "MonitorClient.h"
#include <sstream>

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Ws2_32.lib")

static ULONG_PTR s_gdipToken = 0;
static bool s_gdipInit = false;

static void EnsureGdiPlus()
{
    if (!s_gdipInit)
    {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&s_gdipToken, &input, nullptr);
        s_gdipInit = true;
    }
}

CMonitorClient::CMonitorClient()
    : m_strHost(HTTP_FLASK_HOST)
    , m_nPort(HTTP_FLASK_PORT)
    , m_hFetchThread(nullptr)
    , m_bFetchRunning(false)
    , m_nFetchIntervalMs(200)
    , m_hLatestBitmap(nullptr)
{
    EnsureGdiPlus();
    InitializeCriticalSection(&m_csBitmap);
}

CMonitorClient::~CMonitorClient()
{
    StopFetchThread();

    if (m_hLatestBitmap)
    {
        DeleteObject(m_hLatestBitmap);
        m_hLatestBitmap = nullptr;
    }
    DeleteCriticalSection(&m_csBitmap);
}

void CMonitorClient::SetServer(const std::string& host, int port)
{
    m_strHost = host;
    m_nPort = port;
}

void CMonitorClient::StartFetchThread(int intervalMs)
{
    if (m_bFetchRunning) return;
    m_nFetchIntervalMs = intervalMs;
    m_bFetchRunning = true;
    m_hFetchThread = CreateThread(nullptr, 0, FetchThread, this, 0, nullptr);
}

void CMonitorClient::StopFetchThread()
{
    if (!m_bFetchRunning) return;
    m_bFetchRunning = false;

    if (m_hFetchThread)
    {
        WaitForSingleObject(m_hFetchThread, 5000);
        CloseHandle(m_hFetchThread);
        m_hFetchThread = nullptr;
    }
}

DWORD WINAPI CMonitorClient::FetchThread(LPVOID pParam)
{
    CMonitorClient* pThis = (CMonitorClient*)pParam;

    while (pThis->m_bFetchRunning)
    {
        std::vector<BYTE> jpegData;
        if (pThis->FetchFrame(jpegData))
        {
            HBITMAP hNewBmp = JpegToBitmap(jpegData);
            if (hNewBmp)
            {
                EnterCriticalSection(&pThis->m_csBitmap);
                // Delete previous unconsumed bitmap
                if (pThis->m_hLatestBitmap)
                    DeleteObject(pThis->m_hLatestBitmap);
                pThis->m_hLatestBitmap = hNewBmp;
                LeaveCriticalSection(&pThis->m_csBitmap);
            }
        }

        // Sleep in small intervals so we can exit promptly
        for (int i = 0; i < pThis->m_nFetchIntervalMs / 50 && pThis->m_bFetchRunning; i++)
            Sleep(50);
    }
    return 0;
}

HBITMAP CMonitorClient::TakeLatestBitmap()
{
    HBITMAP hBmp = nullptr;
    EnterCriticalSection(&m_csBitmap);
    hBmp = m_hLatestBitmap;
    m_hLatestBitmap = nullptr;    // ownership transferred to caller
    LeaveCriticalSection(&m_csBitmap);
    return hBmp;
}

bool CMonitorClient::FetchFrame(std::vector<BYTE>& jpegData)
{
    jpegData.clear();
    m_strLastError.clear();

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        m_strLastError = "Socket create failed";
        return false;
    }

    DWORD tmMs = 3000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmMs, sizeof(tmMs));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tmMs, sizeof(tmMs));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)m_nPort);

    if (inet_pton(AF_INET, m_strHost.c_str(), &addr.sin_addr) != 1)
    {
        m_strLastError = "Invalid address";
        closesocket(sock);
        return false;
    }

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        m_strLastError = "Connect failed";
        closesocket(sock);
        return false;
    }

    // Build HTTP request
    std::ostringstream req;
    req << "GET /api/monitor/frame HTTP/1.1\r\n";
    req << "Host: " << m_strHost << ":" << m_nPort << "\r\n";
    req << "Accept: image/jpeg\r\n";
    req << "Connection: close\r\n";
    req << "\r\n";

    std::string reqStr = req.str();
    if (send(sock, reqStr.c_str(), (int)reqStr.size(), 0) == SOCKET_ERROR)
    {
        m_strLastError = "Send failed";
        closesocket(sock);
        return false;
    }

    // Receive full response
    std::vector<BYTE> rawResponse;
    char buf[8192];
    int nRecv;
    while ((nRecv = recv(sock, buf, sizeof(buf), 0)) > 0)
        rawResponse.insert(rawResponse.end(), buf, buf + nRecv);
    closesocket(sock);

    if (rawResponse.empty())
    {
        m_strLastError = "Empty response";
        return false;
    }

    // Find header/body boundary
    size_t headerEnd = 0;
    for (size_t i = 0; i + 3 < rawResponse.size(); i++)
    {
        if (rawResponse[i] == '\r' && rawResponse[i + 1] == '\n' &&
            rawResponse[i + 2] == '\r' && rawResponse[i + 3] == '\n')
        {
            headerEnd = i + 4;
            break;
        }
    }

    if (headerEnd == 0)
    {
        m_strLastError = "No HTTP header end";
        return false;
    }

    // Extract HTTP status line for debugging
    std::string header(rawResponse.begin(), rawResponse.begin() + headerEnd);

    // Parse status code from "HTTP/1.x NNN ..."
    int httpCode = 0;
    size_t spacePos = header.find(' ');
    if (spacePos != std::string::npos)
        httpCode = atoi(header.c_str() + spacePos + 1);

    // 204 = No Content (no frame yet from C#), silently skip
    if (httpCode == 204)
    {
        m_strLastError = "No frame yet (204)";
        return false;
    }

    // Only accept 200
    if (httpCode != 200)
    {
        char errBuf[64];
        sprintf_s(errBuf, "HTTP %d", httpCode);
        m_strLastError = errBuf;
        return false;
    }

    // Extract body
    if (headerEnd >= rawResponse.size())
    {
        m_strLastError = "Empty body";
        return false;
    }

    jpegData.assign(rawResponse.begin() + headerEnd, rawResponse.end());

    // Validate JPEG signature (FF D8)
    if (jpegData.size() < 2 || jpegData[0] != 0xFF || jpegData[1] != 0xD8)
    {
        m_strLastError = "Not a JPEG";
        jpegData.clear();
        return false;
    }

    return true;
}

HBITMAP CMonitorClient::JpegToBitmap(const std::vector<BYTE>& jpegData)
{
    EnsureGdiPlus();
    if (jpegData.empty()) return nullptr;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, jpegData.size());
    if (!hMem) return nullptr;

    void* pMem = GlobalLock(hMem);
    memcpy(pMem, jpegData.data(), jpegData.size());
    GlobalUnlock(hMem);

    IStream* pStream = nullptr;
    if (CreateStreamOnHGlobal(hMem, TRUE, &pStream) != S_OK)
    {
        GlobalFree(hMem);
        return nullptr;
    }

    Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromStream(pStream);
    HBITMAP hBmp = nullptr;

    if (pBitmap && pBitmap->GetLastStatus() == Gdiplus::Ok)
        pBitmap->GetHBITMAP(Gdiplus::Color(0, 0, 0), &hBmp);

    delete pBitmap;
    pStream->Release();
    return hBmp;
}

void CMonitorClient::DrawToStatic(HWND hStatic, HBITMAP hBmp)
{
    if (!hStatic || !hBmp) return;

    RECT rc;
    ::GetClientRect(hStatic, &rc);

    BITMAP bm;
    GetObject(hBmp, sizeof(bm), &bm);

    HDC hDC = ::GetDC(hStatic);
    HDC hMemDC = CreateCompatibleDC(hDC);
    HBITMAP hOld = (HBITMAP)SelectObject(hMemDC, hBmp);

    SetStretchBltMode(hDC, HALFTONE);
    StretchBlt(hDC, 0, 0, rc.right, rc.bottom,
        hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

    SelectObject(hMemDC, hOld);
    DeleteDC(hMemDC);
    ::ReleaseDC(hStatic, hDC);
}
