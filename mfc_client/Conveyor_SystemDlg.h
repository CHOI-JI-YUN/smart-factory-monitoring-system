#pragma once

#include <map>
#include "ConveyorProtocol.h"
#include "SerialComm.h"
#include "TcpClient.h"
#include "HttpClient.h"
#include "MonitorClient.h"
#include "DatabaseManager.h"
#include "SettingsDlg.h"
#include "ReportGenerator.h"


#define CLR_BG          RGB( 35,  48,  75) 
#define CLR_PANEL       RGB( 15,  20,  35)   
#define CLR_GROUP       RGB( 35,  48,  75)   

#define CLR_ACCENT      RGB(  0, 170, 255)   
#define CLR_DANGER      RGB(255,  82,  82)   
#define CLR_WARNING     RGB(255, 193,   7)  

#define CLR_TEXT        RGB(245, 247, 250)   
#define CLR_SUBTEXT     RGB(180, 200, 220)  

#define CLR_BTN_NORMAL  RGB(  0, 120, 215)  
#define CLR_BTN_HOVER   RGB(  0, 150, 255)

// Monitor frame timer
#define TIMER_MONITOR_FRAME     1004
#define MONITOR_FRAME_MS        200

struct BtnColor { COLORREF bg; COLORREF text; };

class CConveyorSystemDlg : public CDialogEx
{
public:
    explicit CConveyorSystemDlg(CWnd* pParent = nullptr);

    void SetLoggedInUser(const CString& user) { m_strLoggedInUser = user; }

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_CONVEYOR_SYSTEM_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;

private:
    CSerialComm      m_serial;
    CTcpClient       m_tcpClient;
    CHttpClient      m_httpClient;
    CMonitorClient   m_monitorClient;
    CDatabaseManager m_dbManager;

    AppSettings      m_appSettings;
    CReportGenerator    m_reportGen;

    float        m_fTemp;
    float        m_fHumi;

    // 마지막으로 유효성 검증된 센서값 (DEFECT 기록용)
    float        m_fLastValidTemp;
    float        m_fLastValidHumi;
    bool         m_bSensorValid;        // 최소 1회 이상 유효 패킷 수신 여부

    bool         m_bMonitorMax;

    CRect        m_rcChartOld;
    CRect        m_rcGroupOld;
    CRect        m_rcBtnMaxOld;
    CRect        m_rcBtnMinOld;
    CRect        m_rcBtnCapOld;
    CRect        m_rcTitleOld;
    int          m_nDistance;
    int          m_nMotorStep;
    bool         m_bConveyorOn;
    bool         m_bFanOn;
    bool         m_bEmgStop;
    bool         m_bEmgCooldown;
    eCommStatus  m_eCommStatus;
    ULONGLONG    m_dwLastRecvTick;

    CString      m_strLoggedInUser;

    HBITMAP      m_hMonitorBmp;

    Gdiplus::Image* m_pImgTemp;
    Gdiplus::Image* m_pImgHumi;
    Gdiplus::Image* m_pImgConveyor;
    Gdiplus::Image* m_pImgFan;
    Gdiplus::Image* m_pImgSafety;

    void LoadPngIcons();
    void FreePngIcons();

    CBrush* m_brBg;
    CBrush* m_brPanel;
    CBrush* m_brAccent;
    CBrush* m_brDanger;
    CBrush* m_brWarning;
    CBrush* m_brBtnNormal;
    CBrush* m_brGroupAll;

    std::map<int, BtnColor> m_btnColors;

    void CaptureFullWindow();
    void SaveBitmapToFile(CBitmap& bitmap, CString path);

    void CreateBrushes();
    void DeleteBrushes();
    void StyleButton(int nID, COLORREF clrBg, COLORREF clrText);

    void UpdateSpeedLabel();
    void UpdateStatusLabels();
    void UpdateCommStatus(eCommStatus status);
    void AddLog(const CString& msg);
    void ProcessStatusPacket(const BYTE* pkt);
    void UpdateMonitorFrame();
    void GenerateDailyReport();


protected:
    HICON m_hIcon;

    virtual BOOL    OnInitDialog() override;
    virtual void    OnOK()         override {}
    virtual void    OnCancel()     override;

    afx_msg void    OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void    OnTimer(UINT_PTR nIDEvent);
    afx_msg void    OnDestroy();
    afx_msg HBRUSH  OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

    afx_msg void    OnBtnSpeed1();
    afx_msg void    OnBtnSpeed2();
    afx_msg void    OnBtnSpeed3();
    afx_msg void    OnBtnEmergencyStop();
    afx_msg void    OnBtnConveyorOn();
    afx_msg void    OnBtnConveyorOff();
    afx_msg void    OnBtnFanOn();
    afx_msg void    OnBtnFanOff();
    afx_msg void    OnBtnMin();
    afx_msg void    OnBtnMax();
    afx_msg void    OnBtnAlertHistory();
    afx_msg void    OnBtnLogout();
    afx_msg void    OnBtnSettings();

    afx_msg LRESULT OnTcpReceived(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnBnClickedStaticSpeed();
    afx_msg void OnStnClickedStaticFan();
    afx_msg void OnStnClickedStaticChart();
    afx_msg void OnBtnCap();
};