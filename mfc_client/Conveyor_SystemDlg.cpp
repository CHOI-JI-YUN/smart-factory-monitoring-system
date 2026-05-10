// Conveyor_SystemDlg.cpp

#include "pch.h"
#include "framework.h"
#include "Conveyor_System.h"
#include "Conveyor_SystemDlg.h"
#include "AlertHistoryDlg.h"
#include "afxdialogex.h"
#include <atlconv.h>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CConveyorSystemDlg::CConveyorSystemDlg(CWnd* pParent)
    : CDialogEx(IDD_CONVEYOR_SYSTEM_DIALOG, pParent)
    , m_hIcon(nullptr)
    , m_fTemp(25.0f)
    , m_fHumi(40.0f)
    , m_fLastValidTemp(25.0f)
    , m_fLastValidHumi(40.0f)
    , m_bSensorValid(false)
    , m_nDistance(0)
    , m_nMotorStep(0)
    , m_bConveyorOn(false)
    , m_bFanOn(false)
    , m_bEmgStop(false)
    , m_bEmgCooldown(false)
    , m_bMonitorMax(false)
    , m_eCommStatus(COMM_OK)
    , m_dwLastRecvTick(0ULL)
    , m_hMonitorBmp(nullptr)
    , m_pImgTemp(nullptr)
    , m_pImgHumi(nullptr)
    , m_pImgConveyor(nullptr)
    , m_pImgFan(nullptr)
    , m_pImgSafety(nullptr)
    , m_brBg(nullptr)
    , m_brPanel(nullptr)
    , m_brAccent(nullptr)
    , m_brDanger(nullptr)
    , m_brWarning(nullptr)
    , m_brBtnNormal(nullptr)
    , m_brGroupAll(nullptr)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CConveyorSystemDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CConveyorSystemDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_WM_CTLCOLOR()
    ON_BN_CLICKED(IDC_BTN_SPEED_1, &CConveyorSystemDlg::OnBtnSpeed1)
    ON_BN_CLICKED(IDC_BTN_SPEED_2, &CConveyorSystemDlg::OnBtnSpeed2)
    ON_BN_CLICKED(IDC_BTN_SPEED_3, &CConveyorSystemDlg::OnBtnSpeed3)
    ON_BN_CLICKED(IDC_BTN_EMERGENCY_STOP, &CConveyorSystemDlg::OnBtnEmergencyStop)
    ON_BN_CLICKED(IDC_BTN_CONVEYOR_ON, &CConveyorSystemDlg::OnBtnConveyorOn)
    ON_BN_CLICKED(IDC_BTN_CONVEYOR_OFF, &CConveyorSystemDlg::OnBtnConveyorOff)
    ON_BN_CLICKED(IDC_BTN_FAN_ON, &CConveyorSystemDlg::OnBtnFanOn)
    ON_BN_CLICKED(IDC_BTN_FAN_OFF, &CConveyorSystemDlg::OnBtnFanOff)
    ON_BN_CLICKED(IDC_BTN_MIN, &CConveyorSystemDlg::OnBtnMin)
    ON_BN_CLICKED(IDC_BTN_MAX, &CConveyorSystemDlg::OnBtnMax)
    ON_BN_CLICKED(IDC_BTN_ALERT_HISTORY, &CConveyorSystemDlg::OnBtnAlertHistory)
    ON_BN_CLICKED(IDC_BTN_LOGOUT, &CConveyorSystemDlg::OnBtnLogout)
    ON_BN_CLICKED(IDC_BTN_SETTINGS, &CConveyorSystemDlg::OnBtnSettings)
    ON_MESSAGE(WM_TCP_RECEIVED, &CConveyorSystemDlg::OnTcpReceived)
    ON_STN_CLICKED(IDC_STATIC_FAN, &CConveyorSystemDlg::OnStnClickedStaticFan)
    ON_STN_CLICKED(IDC_STATIC_CHART, &CConveyorSystemDlg::OnStnClickedStaticChart)
    ON_BN_CLICKED(IDC_BTN_CAP, &CConveyorSystemDlg::OnBtnCap)
END_MESSAGE_MAP()

void CConveyorSystemDlg::CreateBrushes()
{
    m_brGroupAll = new CBrush(CLR_GROUP);
    m_brBg = new CBrush(CLR_BG);
    m_brPanel = new CBrush(CLR_PANEL);
    m_brAccent = new CBrush(CLR_ACCENT);
    m_brDanger = new CBrush(CLR_DANGER);
    m_brWarning = new CBrush(CLR_WARNING);
    m_brBtnNormal = new CBrush(CLR_BTN_NORMAL);
}

void CConveyorSystemDlg::DeleteBrushes()
{
    auto safe_del = [](CBrush*& br) {
        if (br) { br->DeleteObject(); delete br; br = nullptr; }
        };
    safe_del(m_brBg);
    safe_del(m_brPanel);
    safe_del(m_brAccent);
    safe_del(m_brDanger);
    safe_del(m_brWarning);
    safe_del(m_brBtnNormal);
    safe_del(m_brGroupAll);
}

void CConveyorSystemDlg::StyleButton(int nID, COLORREF clrBg, COLORREF clrText)
{
    CWnd* pWnd = GetDlgItem(nID);
    if (!pWnd) return;
    ::SetWindowTheme(pWnd->GetSafeHwnd(), L"", L"");
    m_btnColors[nID] = { clrBg, clrText };
    pWnd->Invalidate();
}

HBRUSH CConveyorSystemDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
    int nID = pWnd->GetDlgCtrlID();

    if (nCtlColor == CTLCOLOR_DLG)
    {
        pDC->SetBkColor(CLR_BG);
        return (HBRUSH)m_brBg->GetSafeHandle();
    }

    if (nCtlColor == CTLCOLOR_STATIC)
    {
        int id = pWnd->GetDlgCtrlID();

        switch (id)
        {
        case IDC_STATIC_SYSTEM_STATE_TEXT:
        case IDC_STATIC_LOG_TEXT:
        case IDC_STATIC_FAN_ONOFF_TEXT:
        case IDC_STATIC_SPEED_TEXT:
        case IDC_GROUP_CONVEYOR_TEXT:
        case IDC_GROUP_TEMP_TEXT:
        case IDC_GROUP_SPEED_TEXT:
        case IDC_GROUP_MONITORING_TEXT:
            pDC->SetTextColor(CLR_TEXT);
            pDC->SetBkMode(TRANSPARENT);
            return (HBRUSH)m_brBg->GetSafeHandle();
        }

        if (id == IDC_STATIC_STS_CONVEYOR)
        {
            pDC->SetTextColor(m_bConveyorOn ? CLR_ACCENT : CLR_SUBTEXT);
            pDC->SetBkColor(CLR_BG);
            return (HBRUSH)m_brBg->GetSafeHandle();
        }
        if (id == IDC_STATIC_STS_FAN)
        {
            pDC->SetTextColor(m_bFanOn ? CLR_ACCENT : CLR_SUBTEXT);
            pDC->SetBkColor(CLR_BG);
            return (HBRUSH)m_brBg->GetSafeHandle();
        }
        if (id == IDC_STATIC_STS_SAFETY)
        {
            pDC->SetTextColor(m_bEmgStop ? CLR_DANGER : CLR_ACCENT);
            pDC->SetBkColor(CLR_BG);
            return (HBRUSH)m_brBg->GetSafeHandle();
        }
        if (id == IDC_STATIC_TEMP_VALUE || id == IDC_STATIC_HUMI_VALUE)
        {
            pDC->SetTextColor(CLR_ACCENT);
            pDC->SetBkColor(CLR_BG);
            return (HBRUSH)m_brBg->GetSafeHandle();
        }
        if (id == IDC_STATIC_SPEED_VALUE)
        {
            pDC->SetTextColor(CLR_WARNING);
            pDC->SetBkColor(CLR_BG);
            return (HBRUSH)m_brBg->GetSafeHandle();
        }
        if (id == IDC_STATIC_MAIN_TITLE)
        {
            pDC->SetTextColor(CLR_TEXT);
            pDC->SetBkColor(CLR_BG);
            return (HBRUSH)m_brBg->GetSafeHandle();
        }
        if (id == IDC_STATIC_EMERGENCY_COLOR)
        {
            if (m_bEmgCooldown)
            {
                // 빨강: 비상정지
                pDC->SetBkColor(RGB(255, 0, 0));
                static CBrush brRed(RGB(255, 0, 0));
                return (HBRUSH)brRed.GetSafeHandle();
            }
            else if (m_bConveyorOn && m_nMotorStep == 0)
            {
                // 노랑: IR 센서 검증 중 (물체 감지 → 일시정지)
                pDC->SetBkColor(RGB(255, 200, 0));
                static CBrush brYellow(RGB(255, 200, 0));
                return (HBRUSH)brYellow.GetSafeHandle();
            }
            else
            {
                // 초록: 정상
                pDC->SetBkColor(RGB(0, 200, 0));
                static CBrush brGreen(RGB(0, 200, 0));
                return (HBRUSH)brGreen.GetSafeHandle();
            }
        }

        pDC->SetTextColor(CLR_SUBTEXT);
        pDC->SetBkColor(CLR_BG);
        return (HBRUSH)m_brBg->GetSafeHandle();
    }

    if (nCtlColor == CTLCOLOR_LISTBOX)
    {
        pDC->SetTextColor(CLR_TEXT);
        pDC->SetBkColor(CLR_PANEL);
        return (HBRUSH)m_brPanel->GetSafeHandle();
    }

    return hbr;
}

BOOL CConveyorSystemDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    CreateBrushes();
    LoadPngIcons();

    // Load settings from INI
    m_appSettings.LoadFromFile(_T(".\\conveyor_settings.ini"));

    StyleButton(IDC_BTN_SPEED_1, CLR_BTN_NORMAL, CLR_TEXT);
    StyleButton(IDC_BTN_SPEED_2, CLR_BTN_NORMAL, CLR_TEXT);
    StyleButton(IDC_BTN_SPEED_3, CLR_BTN_NORMAL, CLR_TEXT);
    StyleButton(IDC_BTN_CONVEYOR_ON, CLR_ACCENT, CLR_BG);
    StyleButton(IDC_BTN_CONVEYOR_OFF, CLR_DANGER, CLR_TEXT);
    StyleButton(IDC_BTN_FAN_ON, CLR_ACCENT, CLR_BG);
    StyleButton(IDC_BTN_FAN_OFF, CLR_DANGER, CLR_TEXT);
    StyleButton(IDC_BTN_EMERGENCY_STOP, CLR_DANGER, CLR_TEXT);
    StyleButton(IDC_BTN_LOGOUT, RGB(80, 80, 100), CLR_TEXT);
    StyleButton(IDC_BTN_SETTINGS, RGB(70, 75, 90), CLR_TEXT);

    Invalidate();

    UpdateSpeedLabel();

    UpdateStatusLabels();

    // Display logged-in user
    {
        CString strWelcome;
        strWelcome.Format(_T("[System] Conveyor System Start  [User: %s]"), (LPCTSTR)m_strLoggedInUser);
        AddLog(strWelcome);
    }

    // SQLite DB
    if (m_dbManager.Open("conveyor.db"))
        AddLog(_T("[DB] SQLite Connected"));
    else
        AddLog(_T("[DB] SQLite Failed"));

    GetDlgItem(IDC_STATIC_CHART)->GetWindowRect(&m_rcChartOld);
    GetDlgItem(IDC_GROUP_MONITORING)->GetWindowRect(&m_rcGroupOld);
    GetDlgItem(IDC_GROUP_MONITORING_TEXT)->GetWindowRect(&m_rcTitleOld);
    GetDlgItem(IDC_BTN_MAX)->GetWindowRect(&m_rcBtnMaxOld);
    GetDlgItem(IDC_BTN_MIN)->GetWindowRect(&m_rcBtnMinOld);
    GetDlgItem(IDC_BTN_CAP)->GetWindowRect(&m_rcBtnCapOld);

    ScreenToClient(&m_rcChartOld);
    ScreenToClient(&m_rcGroupOld);
    ScreenToClient(&m_rcTitleOld);
    ScreenToClient(&m_rcBtnMaxOld);
    ScreenToClient(&m_rcBtnMinOld);
    ScreenToClient(&m_rcBtnCapOld);

    // Serial: use settings value
    if (m_serial.Open((LPCTSTR)m_appSettings.strSerialPort, m_appSettings.nBaudRate))
    {
        CString strLog;
        strLog.Format(_T("[Serial] %s Connected"), (LPCTSTR)m_appSettings.strSerialPort);
        AddLog(strLog);
        SetTimer(TIMER_SERIAL_READ, SERIAL_POLL_MS, nullptr);
    }
    else
    {
        CString strLog;
        strLog.Format(_T("[Serial] %s Failed"), (LPCTSTR)m_appSettings.strSerialPort);
        AddLog(strLog);
    }

    // TCP: use settings value
    {
        CStringA ipA(m_appSettings.strFlaskIP);
        m_tcpClient.Connect(std::string((LPCSTR)ipA), m_appSettings.nTcpPort, GetSafeHwnd());
    }
    AddLog(_T("[TCP] Connecting to Flask..."));

    // HTTP & Monitor: use settings value
    {
        CStringA ipA(m_appSettings.strFlaskIP);
        std::string host((LPCSTR)ipA);
        m_httpClient.SetServer(host, m_appSettings.nHttpPort);
        m_monitorClient.SetServer(host, m_appSettings.nHttpPort);
    }

    SetTimer(TIMER_COMM_WATCHDOG, COMM_TIMEOUT_MS, nullptr);
    m_dwLastRecvTick = GetTickCount64();

    // Monitor frame timer
    SetTimer(TIMER_MONITOR_FRAME, MONITOR_FRAME_MS, nullptr);
    AddLog(_T("[Monitor] Frame polling started"));


    // ★ PDF 리포트 생성기 초기화
    m_reportGen.SetDBManager(&m_dbManager);
    m_reportGen.SetOperatorName(m_strLoggedInUser);
    m_reportGen.SetOutputFolder(_T(".\\Reports"));


    return TRUE;
}

void CConveyorSystemDlg::OnDestroy()
{
    KillTimer(TIMER_SERIAL_READ);
    KillTimer(TIMER_COMM_WATCHDOG);
    KillTimer(TIMER_MONITOR_FRAME);
    KillTimer(TIMER_EMG_AUTO_RELEASE);
    m_serial.Close();
    m_tcpClient.Disconnect();
    m_dbManager.Close();

    if (m_hMonitorBmp)
    {
        DeleteObject(m_hMonitorBmp);
        m_hMonitorBmp = nullptr;
    }

    DeleteBrushes();
    FreePngIcons();

    CDialogEx::OnDestroy();
}

void CConveyorSystemDlg::OnCancel()
{
    if (MessageBox(_T("Exit?"), _T("Confirm"), MB_YESNO | MB_ICONQUESTION) == IDYES)
        DestroyWindow();
}

void CConveyorSystemDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);
        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        dc.DrawIcon((rect.Width() - cxIcon + 1) / 2, (rect.Height() - cyIcon + 1) / 2, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();

        if (m_bMonitorMax) return;

        CClientDC dc(this);
        Gdiplus::Graphics g(dc.GetSafeHdc());
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        int iconSize = 22;

        auto GetCtrlPos = [&](int nID, int& x, int& y) {
            CWnd* p = GetDlgItem(nID);
            if (!p) return false;
            CRect rc;
            p->GetWindowRect(&rc);
            ScreenToClient(&rc);
            x = rc.left;
            y = rc.top;
            return true;
            };

        int cx, cy;
        int iconSizeTH = 55;
        int iconSizeS = 73;

        if (m_pImgTemp && GetCtrlPos(IDC_STATIC_TEMP, cx, cy))
            g.DrawImage(m_pImgTemp, cx - iconSizeTH - 15, cy - 3, iconSizeTH, iconSizeTH);
        if (m_pImgHumi && GetCtrlPos(IDC_STATIC_HUMI, cx, cy))
            g.DrawImage(m_pImgHumi, cx - iconSizeTH - 12, cy, iconSizeTH, iconSizeTH);
        if (m_pImgConveyor && GetCtrlPos(IDC_STATIC_CONVEYOR2, cx, cy))
            g.DrawImage(m_pImgConveyor, cx - iconSizeS + 10, cy - 8, iconSizeS, iconSizeS);
        if (m_pImgFan && GetCtrlPos(IDC_STATIC_FAN, cx, cy))
            g.DrawImage(m_pImgFan, cx - iconSizeS - 10, cy - 3, iconSizeS, iconSizeS);
        if (m_pImgSafety && GetCtrlPos(IDC_STATIC_SAFETY, cx, cy))
            g.DrawImage(m_pImgSafety, cx - iconSizeS - 10, cy - 5, iconSizeS, iconSizeS);
    }
}

HCURSOR CConveyorSystemDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CConveyorSystemDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_SERIAL_READ)
    {
        BYTE pkt[PACKET_SIZE] = {};
        int nRead = m_serial.ReadPacket(pkt, PACKET_SIZE);
        if (nRead == PACKET_SIZE && CSerialComm::ValidatePacket(pkt, PACKET_SIZE))
        {
            m_dwLastRecvTick = GetTickCount64();
            if (m_eCommStatus != COMM_OK)
                UpdateCommStatus(COMM_OK);
            if (pkt[IDX_CMD] == CMD_STATUS_REPORT)
                ProcessStatusPacket(pkt);
        }
    }
    else if (nIDEvent == TIMER_COMM_WATCHDOG)
    {
        ULONGLONG elapsed = GetTickCount64() - m_dwLastRecvTick;
        if (elapsed > COMM_TIMEOUT_MS && m_eCommStatus == COMM_OK)
            UpdateCommStatus(COMM_TIMEOUT);

        m_httpClient.PostStatus(
            m_fTemp, m_fHumi, m_nDistance,
            m_nMotorStep, m_bConveyorOn, m_bFanOn, m_bEmgStop
        );
    }
    else if (nIDEvent == TIMER_MONITOR_FRAME)
    {
        UpdateMonitorFrame();
    }
    else if (nIDEvent == TIMER_EMG_AUTO_RELEASE)
    {
        KillTimer(TIMER_EMG_AUTO_RELEASE);
        m_bEmgStop = false;
        m_bEmgCooldown = false;
        UpdateStatusLabels();

        if (CWnd* p = GetDlgItem(IDC_STATIC_EMERGENCY_COLOR))
            p->Invalidate();
        if (CWnd* p = GetDlgItem(IDC_BTN_EMERGENCY_STOP))
            p->Invalidate();

        AddLog(_T("[Emergency] Auto-released. Conveyor ON available."));
    }
    CDialogEx::OnTimer(nIDEvent);
}

void CConveyorSystemDlg::UpdateMonitorFrame()
{
    std::vector<BYTE> jpegData;
    if (!m_monitorClient.FetchFrame(jpegData))
    {
        static int nFailCount = 0;
        nFailCount++;
        if (nFailCount <= 3 || nFailCount % 50 == 0)
        {
            CString strErr;
            std::string err = m_monitorClient.GetLastError();
            strErr.Format(_T("[Monitor] Frame fetch failed(%d): %S"),
                nFailCount, err.c_str());
            AddLog(strErr);
        }
        return;
    }

    HBITMAP hNewBmp = CMonitorClient::JpegToBitmap(jpegData);
    if (!hNewBmp)
    {
        AddLog(_T("[Monitor] JpegToBitmap failed"));
        return;
    }

    if (m_hMonitorBmp)
        DeleteObject(m_hMonitorBmp);
    m_hMonitorBmp = hNewBmp;

    CWnd* pChart = GetDlgItem(IDC_STATIC_CHART);
    if (pChart)
        CMonitorClient::DrawToStatic(pChart->GetSafeHwnd(), m_hMonitorBmp);
}

void CConveyorSystemDlg::ProcessStatusPacket(const BYTE* pkt)
{
    BYTE tempDec = (pkt[IDX_ULTR] >> 4) & 0x0F;
    BYTE humiDec = pkt[IDX_ULTR] & 0x0F;

    float newTemp = (float)pkt[IDX_TEMP] + tempDec * 0.1f;
    float newHumi = (float)pkt[IDX_HUMI] + humiDec * 0.1f;
    m_nDistance = 0;
    m_nMotorStep = pkt[IDX_MOTR];

    // 비현실적 센서값 필터링 — 5도/5% 미만이면 센서 미안정으로 판단, 이전 값 유지
    if (newTemp >= 5.0f && newHumi >= 5.0f)
    {
        m_fTemp = newTemp;
        m_fHumi = newHumi;
        m_fLastValidTemp = m_fTemp;
        m_fLastValidHumi = m_fHumi;
        m_bSensorValid = true;
    }

    CString strTemp, strHumi;
    strTemp.Format(_T("%.1f"), m_fTemp);
    strHumi.Format(_T("%.1f"), m_fHumi);
    SetDlgItemText(IDC_STATIC_TEMP_VALUE, strTemp);
    SetDlgItemText(IDC_STATIC_HUMI_VALUE, strHumi);
    UpdateSpeedLabel();

    if (CWnd* p = GetDlgItem(IDC_STATIC_EMERGENCY_COLOR))
        p->Invalidate();


    // Anomaly detection - use settings thresholds
    float tempWarn = m_appSettings.fTempWarning;
    float tempDanger = m_appSettings.fTempDanger;
    float humiWarn = m_appSettings.fHumiWarning;
    float humiDanger = m_appSettings.fHumiWarning + 10.0f;

    if (m_fTemp >= tempDanger || m_fHumi >= humiDanger)
    {
        char buf[128];
        if (m_fTemp >= tempDanger && m_fHumi >= humiDanger)
            sprintf_s(buf, "Temp %.1fC, Humi %.1f%% exceeded", m_fTemp, m_fHumi);
        else if (m_fTemp >= tempDanger)
            sprintf_s(buf, "Temp %.1fC exceeded (limit: %.0fC)", m_fTemp, tempDanger);
        else
            sprintf_s(buf, "Humi %.1f%% exceeded (limit: %.0f%%)", m_fHumi, humiDanger);
        m_dbManager.InsertAlert(m_fTemp, m_fHumi, "DANGER", buf);

    }
    else if (m_fTemp >= tempWarn || m_fHumi >= humiWarn)
    {
        char buf[128];
        if (m_fTemp >= tempWarn && m_fHumi >= humiWarn)
            sprintf_s(buf, "Temp %.1fC, Humi %.1f%% warning", m_fTemp, m_fHumi);
        else if (m_fTemp >= tempWarn)
            sprintf_s(buf, "Temp %.1fC warning (limit: %.0fC)", m_fTemp, tempWarn);
        else
            sprintf_s(buf, "Humi %.1f%% warning (limit: %.0f%%)", m_fHumi, humiWarn);
        m_dbManager.InsertAlert(m_fTemp, m_fHumi, "WARNING", buf);
    }
}

LRESULT CConveyorSystemDlg::OnTcpReceived(WPARAM, LPARAM)
{
    std::string data = m_tcpClient.GetReceivedData();
    {
        CString strDebug;
        strDebug.Format(_T("[TCP-DBG] recv(%d): %S"), (int)data.size(), data.c_str());
        AddLog(strDebug);
    }


    if (data == "TCP:CONNECTED") { AddLog(_T("[TCP] Flask Connected"));    return 0; }
    if (data == "TCP:DISCONNECTED") { AddLog(_T("[TCP] Flask Disconnected")); return 0; }

    if (data.find("EMERGENCY:STOP") == 0)
    {
        m_bEmgStop = true; m_bConveyorOn = false; m_nMotorStep = 0;
        m_bEmgCooldown = true;
        m_serial.SendEmergencyStop();
        UpdateSpeedLabel(); UpdateStatusLabels();
        // 비상정지 색상 표시
        if (CWnd* p = GetDlgItem(IDC_STATIC_EMERGENCY_COLOR))
            p->Invalidate();
        // 쿨다운 타이머 (설정값 적용)
        SetTimer(TIMER_EMG_AUTO_RELEASE,
            (UINT)(m_appSettings.nEmgCooldownSec * 1000), nullptr);
        // DB 기록
        m_dbManager.InsertAlert(0, 0, "STOP",
            "Emergency stop from Flask web");

        CString strLog;
        strLog.Format(_T("[TCP] Emergency Stop from Flask (%ds cooldown)"), m_appSettings.nEmgCooldownSec);
        AddLog(strLog);
    }
    else if (data.find("EMERGENCY:RELEASE") == 0)
    {
        m_bEmgStop = false;
        m_bEmgCooldown = false;
        KillTimer(TIMER_EMG_AUTO_RELEASE);
        UpdateStatusLabels();
        if (CWnd* p = GetDlgItem(IDC_STATIC_EMERGENCY_COLOR))
            p->Invalidate();
        AddLog(_T("[TCP] Emergency Release"));
    }
    else if (data.find("CONVEYOR:ON") == 0)
    {
        m_bConveyorOn = true;
        m_serial.SendConveyorControl(CTRL_ON);
        UpdateStatusLabels();
        AddLog(_T("[TCP] Conveyor ON"));
    }
    else if (data.find("CONVEYOR:OFF") == 0)
    {
        m_bConveyorOn = false;
        m_serial.SendConveyorControl(CTRL_OFF);
        UpdateStatusLabels();
        AddLog(_T("[TCP] Conveyor OFF"));
    }
    else if (data.find("DEFECT:SERVO") == 0)
    {
        // Flask에서 불량 검출 → 라즈베리파이에 서보모터 작동 명령 전달
        m_serial.SendDefectServo();

        // 유효 센서값 사용 (시리얼 미수신 시 0 대신 마지막 정상값)
        float logTemp = m_bSensorValid ? m_fLastValidTemp : m_fTemp;
        float logHumi = m_bSensorValid ? m_fLastValidHumi : m_fHumi;

        CString strLog;
        strLog.Format(_T("[TCP] Defect detected -> Servo reject (Temp:%.1f, Humi:%.1f)"),
            logTemp, logHumi);
        AddLog(strLog);

        // DB 기록 (non-blocking)
        m_dbManager.InsertAlertAsync(logTemp, logHumi, "DEFECT",
            "Defect detected - servo reject activated");
    }
    else
    {
        CString strLog;
        strLog.Format(_T("[TCP] %S"), data.c_str());
        AddLog(strLog);
    }
    return 0;
}

void CConveyorSystemDlg::OnBtnSpeed1()
{
    if (m_bEmgStop) { AddLog(_T("[Warn] Emergency Stop Active")); return; }
    m_nMotorStep = 1; m_serial.SendSpeedSet(1); UpdateSpeedLabel();
    AddLog(_T("[Control] Speed 1 (Low)"));
}

void CConveyorSystemDlg::OnBtnSpeed2()
{
    if (m_bEmgStop) { AddLog(_T("[Warn] Emergency Stop Active")); return; }
    m_nMotorStep = 2; m_serial.SendSpeedSet(2); UpdateSpeedLabel();
    AddLog(_T("[Control] Speed 2 (Mid)"));
}

void CConveyorSystemDlg::OnBtnSpeed3()
{
    if (m_bEmgStop) { AddLog(_T("[Warn] Emergency Stop Active")); return; }
    m_nMotorStep = 3; m_serial.SendSpeedSet(3); UpdateSpeedLabel();
    AddLog(_T("[Control] Speed 3 (High)"));
}

void CConveyorSystemDlg::OnBtnEmergencyStop()
{
    m_bEmgStop = true; m_bConveyorOn = false; m_nMotorStep = 0;
    m_bEmgCooldown = true;
    m_serial.SendEmergencyStop();
    UpdateSpeedLabel(); UpdateStatusLabels();

    if (CWnd* p = GetDlgItem(IDC_STATIC_EMERGENCY_COLOR))
        p->Invalidate();

    // Use settings cooldown value
    SetTimer(TIMER_EMG_AUTO_RELEASE, (UINT)(m_appSettings.nEmgCooldownSec * 1000), nullptr);

    CString strLog;
    strLog.Format(_T("[Emergency] EMERGENCY STOP! (%ds cooldown)"), m_appSettings.nEmgCooldownSec);
    AddLog(strLog);
    m_dbManager.InsertAlert(0, 0, "STOP", "Emergency stop button pressed");

}

void CConveyorSystemDlg::OnBtnConveyorOn()
{
    if (m_bEmgStop) { AddLog(_T("[Warn] Release Emergency First")); return; }
    m_bConveyorOn = true; m_serial.SendConveyorControl(CTRL_ON);
    UpdateStatusLabels(); AddLog(_T("[Control] Conveyor ON"));
}

void CConveyorSystemDlg::OnBtnConveyorOff()
{
    m_bConveyorOn = false; m_serial.SendConveyorControl(CTRL_OFF);
    UpdateStatusLabels(); AddLog(_T("[Control] Conveyor OFF"));
}

void CConveyorSystemDlg::OnBtnFanOn()
{
    m_bFanOn = true; m_serial.SendFanControl(CTRL_ON);
    UpdateStatusLabels(); AddLog(_T("[Control] Fan ON"));
}

void CConveyorSystemDlg::OnBtnFanOff()
{
    m_bFanOn = false; m_serial.SendFanControl(CTRL_OFF);
    UpdateStatusLabels(); AddLog(_T("[Control] Fan OFF"));
}

void CConveyorSystemDlg::UpdateSpeedLabel()
{
    CString str;
    switch (m_nMotorStep)
    {
    case 1: str = _T("Step 1 (Low)");  break;
    case 2: str = _T("Step 2 (Mid)");  break;
    case 3: str = _T("Step 3 (High)"); break;
    default: str = _T("STOP");         break;
    }
    SetDlgItemText(IDC_STATIC_SPEED_VALUE, str);
    if (CWnd* p = GetDlgItem(IDC_STATIC_SPEED_VALUE)) p->Invalidate();
}

void CConveyorSystemDlg::UpdateStatusLabels()
{
    SetDlgItemText(IDC_STATIC_STS_CONVEYOR, m_bConveyorOn ? _T("ON") : _T("OFF"));
    SetDlgItemText(IDC_STATIC_STS_FAN, m_bFanOn ? _T("ON") : _T("OFF"));
    SetDlgItemText(IDC_STATIC_STS_SAFETY, m_bEmgStop ? _T("E-STOP") : _T("NORMAL"));
    if (CWnd* p = GetDlgItem(IDC_STATIC_STS_CONVEYOR)) p->Invalidate();
    if (CWnd* p = GetDlgItem(IDC_STATIC_STS_FAN))      p->Invalidate();
    if (CWnd* p = GetDlgItem(IDC_STATIC_STS_SAFETY))   p->Invalidate();
}

void CConveyorSystemDlg::UpdateCommStatus(eCommStatus status)
{
    m_eCommStatus = status;
    switch (status)
    {
    case COMM_OK:      AddLog(_T("[Comm] OK"));      break;
    case COMM_ERROR:   AddLog(_T("[Comm] Error"));   break;
    case COMM_TIMEOUT: AddLog(_T("[Comm] Timeout")); break;
    }
}

void CConveyorSystemDlg::AddLog(const CString& msg)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    CString strEntry;
    strEntry.Format(_T("[%02d:%02d:%02d] %s"),
        st.wHour, st.wMinute, st.wSecond, (LPCTSTR)msg);
    CListBox* pList = reinterpret_cast<CListBox*>(GetDlgItem(IDC_LIST_LOG));
    if (pList)
    {
        int nIdx = pList->AddString(strEntry);
        pList->SetCurSel(nIdx);
    }
}

void CConveyorSystemDlg::OnBnClickedStaticSpeed() {}
void CConveyorSystemDlg::OnStnClickedStaticFan() {}
void CConveyorSystemDlg::OnStnClickedStaticChart() {}

void CConveyorSystemDlg::OnBtnAlertHistory()
{
    CAlertHistoryDlg dlg(&m_dbManager, this);
    dlg.DoModal();
}

// ============================================================
// Settings
// ============================================================
void CConveyorSystemDlg::OnBtnSettings()
{
    CSettingsDlg dlg(&m_appSettings, this);

    if (dlg.DoModal() == IDOK)
    {
        AddLog(_T("[System] Settings updated"));

        CString strInfo;
        strInfo.Format(
            _T("Settings saved.\n\n")
            _T("Serial: %s @ %d\n")
            _T("Server: %s (TCP:%d, HTTP:%d)\n")
            _T("Temp Warning: %.1f C, Danger: %.1f C\n")
            _T("Humi Warning: %.1f%%\n")
            _T("EMG Cooldown: %ds\n\n")
            _T("Serial/TCP changes require restart."),
            (LPCTSTR)m_appSettings.strSerialPort,
            m_appSettings.nBaudRate,
            (LPCTSTR)m_appSettings.strFlaskIP,
            m_appSettings.nTcpPort,
            m_appSettings.nHttpPort,
            m_appSettings.fTempWarning,
            m_appSettings.fTempDanger,
            m_appSettings.fHumiWarning,
            m_appSettings.nEmgCooldownSec
        );
        MessageBox(strInfo, _T("Settings Applied"), MB_OK | MB_ICONINFORMATION);
    }
}

void CConveyorSystemDlg::GenerateDailyReport()
{
    m_reportGen.SetOperatorName(m_strLoggedInUser);

    // DB 연결 상태 재확인 — 닫혀있으면 재연결
    if (!m_dbManager.IsOpen())
    {
        AddLog(_T("[Report] DB not open, attempting re-open..."));
        if (m_dbManager.Open("conveyor.db"))
        {
            AddLog(_T("[Report] DB re-opened successfully"));
        }
        else
        {
            CString err(_T("Report failed: Database is not available"));
            AddLog(err);
            MessageBox(err, _T("Report Error"), MB_OK | MB_ICONERROR);
            return;
        }
    }
    // DB 포인터 재설정 (NULL 방지)
    m_reportGen.SetDBManager(&m_dbManager);

    CString filePath = m_reportGen.GenerateDailyReport();

    if (filePath.IsEmpty())
    {
        CString err;
        err.Format(_T("Report failed: %s"),
            (LPCTSTR)m_reportGen.GetLastError());
        AddLog(err);
        MessageBox(err, _T("Report Error"), MB_OK | MB_ICONERROR);
    }
    else
    {
        CString msg;
        msg.Format(_T("[Report] Saved: %s"), (LPCTSTR)filePath);
        AddLog(msg);

        MessageBox(filePath, _T("Report Generated"), MB_OK | MB_ICONINFORMATION);

        // 생성된 PDF 자동 열기
        ShellExecute(NULL, _T("open"), filePath, NULL, NULL, SW_SHOWNORMAL);
    }
}

// ============================================================
// Logout
// ============================================================
// ── 3-7. OnBtnLogout() 수정 — 로그아웃 시 자동 리포트 생성 ──
// 기존 OnBtnLogout() 전체를 아래로 교체:

void CConveyorSystemDlg::OnBtnLogout()
{
    if (MessageBox(_T("Confirm logout?\nDaily report will be generated."),
        _T("Logout"), MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        GenerateDailyReport();
        AddLog(_T("[System] User logged out"));
        EndDialog(ID_LOGOUT_RESULT);
    }
}

void CConveyorSystemDlg::OnBtnMin()
{
    if (!m_bMonitorMax) return;

    CWnd* pChart = GetDlgItem(IDC_STATIC_CHART);
    CWnd* pGroup = GetDlgItem(IDC_GROUP_MONITORING);
    CWnd* pTitle = GetDlgItem(IDC_GROUP_MONITORING_TEXT);
    CWnd* pBtnMax = GetDlgItem(IDC_BTN_MAX);
    CWnd* pBtnMin = GetDlgItem(IDC_BTN_MIN);
    CWnd* pBtnCap = GetDlgItem(IDC_BTN_CAP);

    if (!pChart || !pGroup || !pTitle || !pBtnMax || !pBtnMin)
        return;

    pChart->MoveWindow(&m_rcChartOld);
    pGroup->MoveWindow(&m_rcGroupOld);
    pTitle->MoveWindow(&m_rcTitleOld);
    pBtnMax->MoveWindow(&m_rcBtnMaxOld);
    pBtnMin->MoveWindow(&m_rcBtnMinOld);
    pBtnCap->MoveWindow(&m_rcBtnCapOld);

    for (CWnd* pChild = GetWindow(GW_CHILD); pChild != NULL; pChild = pChild->GetNextWindow())
    {
        pChild->ShowWindow(SW_SHOW);
    }

    m_bMonitorMax = false;
    Invalidate();
    UpdateWindow();
}

void CConveyorSystemDlg::OnBtnMax()
{
    if (m_bMonitorMax) return;

    CWnd* pChart = GetDlgItem(IDC_STATIC_CHART);
    CWnd* pGroup = GetDlgItem(IDC_GROUP_MONITORING);
    CWnd* pTitle = GetDlgItem(IDC_GROUP_MONITORING_TEXT);
    CWnd* pBtnMax = GetDlgItem(IDC_BTN_MAX);
    CWnd* pBtnMin = GetDlgItem(IDC_BTN_MIN);
    CWnd* pBtnCap = GetDlgItem(IDC_BTN_CAP);

    if (!pChart || !pGroup || !pTitle || !pBtnMax || !pBtnMin || !pBtnCap)
        return;

    CRect rcClient;
    GetClientRect(&rcClient);

    for (CWnd* pChild = GetWindow(GW_CHILD); pChild != NULL; pChild = pChild->GetNextWindow())
    {
        int id = pChild->GetDlgCtrlID();
        if (id != IDC_STATIC_CHART &&
            id != IDC_GROUP_MONITORING &&
            id != IDC_GROUP_MONITORING_TEXT &&
            id != IDC_BTN_MAX &&
            id != IDC_BTN_MIN &&
            id != IDC_BTN_CAP)
        {
            pChild->ShowWindow(SW_HIDE);
        }
    }

    CRect rcGroup = rcClient;
    rcGroup.DeflateRect(20, 25, 20, 20);
    pGroup->MoveWindow(&rcGroup);

    CRect rcChart = rcGroup;
    rcChart.DeflateRect(10, 45, 10, 10);
    pChart->MoveWindow(&rcChart);

    CRect rcTitle(
        rcGroup.left + (rcGroup.Width() / 2) - 100,
        rcGroup.top + 2,
        rcGroup.left + (rcGroup.Width() / 2) + 140,
        rcGroup.top + 32
    );
    pTitle->MoveWindow(&rcTitle);

    CRect rcBtnCap(
        rcGroup.left + 18, rcGroup.top + 18,
        rcGroup.left + 108, rcGroup.top + 43
    );
    pBtnCap->MoveWindow(&rcBtnCap);

    CRect rcBtnMin(
        rcGroup.right - 105, rcGroup.top + 18,
        rcGroup.right - 55, rcGroup.top + 43
    );
    pBtnMin->MoveWindow(&rcBtnMin);

    CRect rcBtnMax(
        rcGroup.right - 50, rcGroup.top + 18,
        rcGroup.right - 5, rcGroup.top + 43
    );
    pBtnMax->MoveWindow(&rcBtnMax);

    pBtnCap->ShowWindow(SW_SHOW);
    pBtnMin->ShowWindow(SW_SHOW);
    pBtnMax->ShowWindow(SW_SHOW);

    m_bMonitorMax = true;
    Invalidate();
    UpdateWindow();
}

void CConveyorSystemDlg::OnBtnCap()
{
    CaptureFullWindow();
}

void CConveyorSystemDlg::CaptureFullWindow()
{
    CRect rect;
    GetWindowRect(&rect);
    int width = rect.Width();
    int height = rect.Height();

    CDC* pScreenDC = CDC::FromHandle(::GetDC(NULL));
    CDC memDC;
    memDC.CreateCompatibleDC(pScreenDC);

    CBitmap bitmap;
    bitmap.CreateCompatibleBitmap(pScreenDC, width, height);
    CBitmap* pOldBitmap = memDC.SelectObject(&bitmap);

    memDC.BitBlt(0, 0, width, height, pScreenDC, rect.left, rect.top, SRCCOPY);
    memDC.SelectObject(pOldBitmap);

    CString folder = _T("C:\\suyoung\\Project_2\\MFC\\ScreenShot\\");
    ::CreateDirectory(folder, NULL);

    CTime time = CTime::GetCurrentTime();
    CString fileName;
    fileName.Format(
        _T("%sCapture_%04d%02d%02d_%02d%02d%02d.bmp"),
        (LPCTSTR)folder,
        time.GetYear(), time.GetMonth(), time.GetDay(),
        time.GetHour(), time.GetMinute(), time.GetSecond()
    );

    SaveBitmapToFile(bitmap, fileName);
    ::ReleaseDC(NULL, pScreenDC->GetSafeHdc());
    AfxMessageBox(_T("Screenshot saved"));
}

void CConveyorSystemDlg::SaveBitmapToFile(CBitmap& bitmap, CString path)
{
    BITMAP bmp;
    bitmap.GetBitmap(&bmp);

    BITMAPINFOHEADER bi;
    ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    DWORD dwBmpSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;

    HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
    if (hDIB == NULL) return;

    char* lpbitmap = (char*)GlobalLock(hDIB);
    if (lpbitmap == NULL) { GlobalFree(hDIB); return; }

    CDC dc;
    dc.CreateCompatibleDC(NULL);
    GetDIBits(dc.GetSafeHdc(), (HBITMAP)bitmap.GetSafeHandle(),
        0, (UINT)bmp.bmHeight, lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        GlobalUnlock(hDIB); GlobalFree(hDIB); return;
    }

    BITMAPFILEHEADER bmfHeader;
    bmfHeader.bfType = 0x4D42;
    bmfHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
    bmfHeader.bfReserved1 = 0;
    bmfHeader.bfReserved2 = 0;
    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    DWORD dwBytesWritten = 0;
    WriteFile(hFile, &bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
    WriteFile(hFile, &bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
    WriteFile(hFile, lpbitmap, dwBmpSize, &dwBytesWritten, NULL);

    CloseHandle(hFile);
    GlobalUnlock(hDIB);
    GlobalFree(hDIB);
}

void CConveyorSystemDlg::LoadPngIcons()
{
    m_pImgTemp = Gdiplus::Image::FromFile(L"images\\icon_temp.png");
    m_pImgHumi = Gdiplus::Image::FromFile(L"images\\icon_humi.png");
    m_pImgConveyor = Gdiplus::Image::FromFile(L"images\\icon_conveyor.png");
    m_pImgFan = Gdiplus::Image::FromFile(L"images\\icon_fan.png");
    m_pImgSafety = Gdiplus::Image::FromFile(L"images\\icon_safety.png");

    auto check = [](Gdiplus::Image*& img) {
        if (img && img->GetLastStatus() != Gdiplus::Ok) {
            delete img; img = nullptr;
        }
        };
    check(m_pImgTemp);
    check(m_pImgHumi);
    check(m_pImgConveyor);
    check(m_pImgFan);
    check(m_pImgSafety);
}

void CConveyorSystemDlg::FreePngIcons()
{
    delete m_pImgTemp;     m_pImgTemp = nullptr;
    delete m_pImgHumi;     m_pImgHumi = nullptr;
    delete m_pImgConveyor; m_pImgConveyor = nullptr;
    delete m_pImgFan;      m_pImgFan = nullptr;
    delete m_pImgSafety;   m_pImgSafety = nullptr;
}