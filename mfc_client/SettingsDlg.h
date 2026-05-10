#pragma once

#include "resource.h"
#include <afxwin.h>
#include <afxcmn.h>

// App settings struct
struct AppSettings
{
    // Communication
    CString     strSerialPort;
    int         nBaudRate;
    CString     strFlaskIP;
    int         nTcpPort;
    int         nHttpPort;

    // Alert Thresholds
    float       fTempWarning;
    float       fTempDanger;
    float       fHumiWarning;
    float       fHumiDanger;

    // Emergency
    int         nEmgCooldownSec;

    AppSettings()
    {
        strSerialPort = _T("COM4");
        nBaudRate = 9600;
        strFlaskIP = _T("192.168.0.30");
        nTcpPort = 9000;
        nHttpPort = 5000;
        fTempWarning = 28.0f;
        fTempDanger = 33.0f;
        fHumiWarning = 70.0f;
        nEmgCooldownSec = 60;
    }

    void SaveToFile(LPCTSTR path);
    void LoadFromFile(LPCTSTR path);
};


class CSettingsDlg : public CDialogEx
{
public:
    CSettingsDlg(AppSettings* pSettings, CWnd* pParent = nullptr);

    enum { IDD = IDD_SETTINGS };

    AppSettings* m_pSettings;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnOK();

    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

private:
    CBrush m_brBg;
    CBrush m_brEdit;

    CSliderCtrl m_sliderTempWarn;
    CSliderCtrl m_sliderTempDanger;
    CSliderCtrl m_sliderHumiWarn;
    CSliderCtrl m_sliderHumiDanger;

    void UpdateSliderLabels();

    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnStnClickedStaticSectComm();
};