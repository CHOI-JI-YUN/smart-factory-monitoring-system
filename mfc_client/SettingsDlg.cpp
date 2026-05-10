// SettingsDlg.cpp

#include "pch.h"
#include "resource.h"
#include "SettingsDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

void AppSettings::SaveToFile(LPCTSTR path)
{
    CString str;

    WritePrivateProfileString(_T("Communication"), _T("SerialPort"), strSerialPort, path);

    str.Format(_T("%d"), nBaudRate);
    WritePrivateProfileString(_T("Communication"), _T("BaudRate"), str, path);

    WritePrivateProfileString(_T("Communication"), _T("FlaskIP"), strFlaskIP, path);

    str.Format(_T("%d"), nTcpPort);
    WritePrivateProfileString(_T("Communication"), _T("TcpPort"), str, path);

    str.Format(_T("%d"), nHttpPort);
    WritePrivateProfileString(_T("Communication"), _T("HttpPort"), str, path);

    str.Format(_T("%.1f"), fTempWarning);
    WritePrivateProfileString(_T("Alert"), _T("TempWarning"), str, path);

    str.Format(_T("%.1f"), fTempDanger);
    WritePrivateProfileString(_T("Alert"), _T("TempDanger"), str, path);

    str.Format(_T("%.1f"), fHumiWarning);
    WritePrivateProfileString(_T("Alert"), _T("HumiWarning"), str, path);

    str.Format(_T("%.1f"), fHumiDanger);
    WritePrivateProfileString(_T("Alert"), _T("HumiDanger"), str, path);

    str.Format(_T("%d"), nEmgCooldownSec);
    WritePrivateProfileString(_T("Emergency"), _T("CooldownSec"), str, path);
}

void AppSettings::LoadFromFile(LPCTSTR path)
{
    TCHAR buf[256];

    GetPrivateProfileString(_T("Communication"), _T("SerialPort"),
        _T("COM4"), buf, 256, path);
    strSerialPort = buf;

    nBaudRate = GetPrivateProfileInt(_T("Communication"), _T("BaudRate"), 9600, path);

    GetPrivateProfileString(_T("Communication"), _T("FlaskIP"),
        _T("192.168.0.30"), buf, 256, path);
    strFlaskIP = buf;

    nTcpPort = GetPrivateProfileInt(_T("Communication"), _T("TcpPort"), 9000, path);
    nHttpPort = GetPrivateProfileInt(_T("Communication"), _T("HttpPort"), 5000, path);

    GetPrivateProfileString(_T("Alert"), _T("TempWarning"), _T("40.0"), buf, 256, path);
    fTempWarning = (float)_tstof(buf);

    GetPrivateProfileString(_T("Alert"), _T("TempDanger"), _T("50.0"), buf, 256, path);
    fTempDanger = (float)_tstof(buf);

    GetPrivateProfileString(_T("Alert"), _T("HumiWarning"), _T("40.0"), buf, 256, path);
    fHumiWarning = (float)_tstof(buf);

    GetPrivateProfileString(_T("Alert"), _T("HumiDanger"), _T("70.0"), buf, 256, path);
    fHumiDanger = (float)_tstof(buf);   // 중요: 여기 fTempDanger 아님

    nEmgCooldownSec = GetPrivateProfileInt(_T("Emergency"), _T("CooldownSec"), 60, path);
}

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialogEx)
    ON_WM_HSCROLL()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

CSettingsDlg::CSettingsDlg(AppSettings* pSettings, CWnd* pParent)
    : CDialogEx(IDD_SETTINGS, pParent)
    , m_pSettings(pSettings)
{
}

void CSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_SLIDER_TEMP_WARN, m_sliderTempWarn);
    DDX_Control(pDX, IDC_SLIDER_TEMP_DANGER, m_sliderTempDanger);
    DDX_Control(pDX, IDC_SLIDER_HUMI_WARN, m_sliderHumiWarn);
    DDX_Control(pDX, IDC_SLIDER_HUMI_DANGER, m_sliderHumiDanger); // 중요
}

BOOL CSettingsDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_brBg.CreateSolidBrush(RGB(200, 200, 200));

    CComboBox* pComPort = (CComboBox*)GetDlgItem(IDC_COMBO_SERIAL_PORT);
    if (pComPort)
    {
        pComPort->AddString(_T("COM1"));
        pComPort->AddString(_T("COM2"));
        pComPort->AddString(_T("COM3"));
        pComPort->AddString(_T("COM4"));
        pComPort->AddString(_T("COM5"));
        pComPort->AddString(_T("COM6"));

        int idx = pComPort->FindStringExact(-1, m_pSettings->strSerialPort);
        pComPort->SetCurSel(idx >= 0 ? idx : 2);
    }

    CComboBox* pBaud = (CComboBox*)GetDlgItem(IDC_COMBO_BAUD_RATE);
    if (pBaud)
    {
        pBaud->AddString(_T("4800"));
        pBaud->AddString(_T("9600"));
        pBaud->AddString(_T("19200"));
        pBaud->AddString(_T("38400"));
        pBaud->AddString(_T("57600"));
        pBaud->AddString(_T("115200"));

        CString strBaud;
        strBaud.Format(_T("%d"), m_pSettings->nBaudRate);
        int idx = pBaud->FindStringExact(-1, strBaud);
        pBaud->SetCurSel(idx >= 0 ? idx : 1);
    }

    SetDlgItemText(IDC_EDIT_FLASK_IP, m_pSettings->strFlaskIP);

    CString strPort;
    strPort.Format(_T("%d"), m_pSettings->nTcpPort);
    SetDlgItemText(IDC_EDIT_TCP_PORT, strPort);

    strPort.Format(_T("%d"), m_pSettings->nHttpPort);
    SetDlgItemText(IDC_EDIT_HTTP_PORT, strPort);

    m_sliderTempWarn.SetRange(150, 400);
    m_sliderTempWarn.SetPos((int)(m_pSettings->fTempWarning * 10));

    m_sliderTempDanger.SetRange(150, 500);
    m_sliderTempDanger.SetPos((int)(m_pSettings->fTempDanger * 10));

    m_sliderHumiWarn.SetRange(0, 1000);
    m_sliderHumiWarn.SetPos((int)(m_pSettings->fHumiWarning * 10));

    m_sliderHumiDanger.SetRange(0, 1000);
    m_sliderHumiDanger.SetPos((int)(m_pSettings->fHumiDanger * 10));

    CComboBox* pCooldown = (CComboBox*)GetDlgItem(IDC_COMBO_EMG_COOLDOWN);
    if (pCooldown)
    {
        pCooldown->AddString(_T("30s"));
        pCooldown->AddString(_T("60s"));
        pCooldown->AddString(_T("90s"));
        pCooldown->AddString(_T("120s"));
        pCooldown->AddString(_T("180s"));

        int idx = 1;
        switch (m_pSettings->nEmgCooldownSec)
        {
        case 30:  idx = 0; break;
        case 60:  idx = 1; break;
        case 90:  idx = 2; break;
        case 120: idx = 3; break;
        case 180: idx = 4; break;
        }

        pCooldown->SetCurSel(idx);
    }

    UpdateSliderLabels();

    return TRUE;
}

void CSettingsDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
    UpdateSliderLabels();
}

void CSettingsDlg::UpdateSliderLabels()
{
    CString str;

    float tw = m_sliderTempWarn.GetPos() / 10.0f;
    str.Format(_T("%.1f"), tw);
    SetDlgItemText(IDC_STATIC_VAL_TEMP_WARN, str);

    float td = m_sliderTempDanger.GetPos() / 10.0f;
    str.Format(_T("%.1f"), td);
    SetDlgItemText(IDC_STATIC_VAL_TEMP_DANGER, str);

    float hw = m_sliderHumiWarn.GetPos() / 10.0f;
    str.Format(_T("%.1f"), hw);
    SetDlgItemText(IDC_STATIC_VAL_HUMI_WARN, str);

    float hd = m_sliderHumiDanger.GetPos() / 10.0f;
    str.Format(_T("%.1f"), hd);
    SetDlgItemText(IDC_STATIC_VAL_HUMI_DANGER, str);
}

void CSettingsDlg::OnOK()
{
    CComboBox* pComPort = (CComboBox*)GetDlgItem(IDC_COMBO_SERIAL_PORT);
    if (pComPort)
    {
        int sel = pComPort->GetCurSel();
        if (sel >= 0)
            pComPort->GetLBText(sel, m_pSettings->strSerialPort);
    }

    CComboBox* pBaud = (CComboBox*)GetDlgItem(IDC_COMBO_BAUD_RATE);
    if (pBaud)
    {
        CString strBaud;
        int sel = pBaud->GetCurSel();

        if (sel >= 0)
        {
            pBaud->GetLBText(sel, strBaud);
            m_pSettings->nBaudRate = _ttoi(strBaud);
        }
    }

    CString strVal;

    GetDlgItemText(IDC_EDIT_FLASK_IP, strVal);
    m_pSettings->strFlaskIP = strVal;

    GetDlgItemText(IDC_EDIT_TCP_PORT, strVal);
    m_pSettings->nTcpPort = _ttoi(strVal);

    GetDlgItemText(IDC_EDIT_HTTP_PORT, strVal);
    m_pSettings->nHttpPort = _ttoi(strVal);

    m_pSettings->fTempWarning = m_sliderTempWarn.GetPos() / 10.0f;
    m_pSettings->fTempDanger = m_sliderTempDanger.GetPos() / 10.0f;
    m_pSettings->fHumiWarning = m_sliderHumiWarn.GetPos() / 10.0f;
    m_pSettings->fHumiDanger = m_sliderHumiDanger.GetPos() / 10.0f;

    if (m_pSettings->fTempWarning >= m_pSettings->fTempDanger)
    {
        MessageBox(_T("Temperature warning must be lower than danger threshold."),
            _T("Validation Error"), MB_OK | MB_ICONWARNING);
        return;
    }

    if (m_pSettings->fHumiWarning >= m_pSettings->fHumiDanger)
    {
        MessageBox(_T("Humidity warning must be lower than danger threshold."),
            _T("Validation Error"), MB_OK | MB_ICONWARNING);
        return;
    }

    CComboBox* pCooldown = (CComboBox*)GetDlgItem(IDC_COMBO_EMG_COOLDOWN);
    if (pCooldown)
    {
        int cooldownValues[] = { 30, 60, 90, 120, 180 };
        int sel = pCooldown->GetCurSel();

        if (sel >= 0 && sel < 5)
            m_pSettings->nEmgCooldownSec = cooldownValues[sel];
    }

    m_pSettings->SaveToFile(_T(".\\conveyor_settings.ini"));

    CDialogEx::OnOK();
}

HBRUSH CSettingsDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

    if (nCtlColor == CTLCOLOR_DLG)
    {
        return (HBRUSH)m_brBg.GetSafeHandle();
    }

    if (nCtlColor == CTLCOLOR_STATIC)
    {
        int nID = pWnd->GetDlgCtrlID();

        if (nID == IDC_STATIC_SECT_COMM)
            pDC->SetTextColor(RGB(0, 120, 215));
        else if (nID == IDC_STATIC_SECT_ALERT)
            pDC->SetTextColor(RGB(230, 120, 0));
        else if (nID == IDC_STATIC_SECT_EMG)
            pDC->SetTextColor(RGB(30, 130, 90));
        else if (nID == IDC_STATIC_VAL_TEMP_WARN)
            pDC->SetTextColor(RGB(210, 120, 20));
        else if (nID == IDC_STATIC_VAL_TEMP_DANGER)
            pDC->SetTextColor(RGB(220, 60, 60));
        else if (nID == IDC_STATIC_VAL_HUMI_WARN)
            pDC->SetTextColor(RGB(210, 120, 20));
        else if (nID == IDC_STATIC_VAL_HUMI_DANGER)
            pDC->SetTextColor(RGB(220, 60, 60));
        else
            pDC->SetTextColor(RGB(40, 50, 65));

        pDC->SetBkMode(TRANSPARENT);
        return (HBRUSH)m_brBg.GetSafeHandle();
    }

    if (nCtlColor == CTLCOLOR_EDIT)
    {
        pDC->SetTextColor(RGB(40, 50, 65));
        pDC->SetBkColor(RGB(255, 255, 255));

        static CBrush brEdit(RGB(255, 255, 255));
        return (HBRUSH)brEdit.GetSafeHandle();
    }

    return hbr;
}