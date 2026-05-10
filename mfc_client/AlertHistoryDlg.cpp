// AlertHistoryDlg.cpp

#include "pch.h"
#include "framework.h"
#include "resource.h"
#include "AlertHistoryDlg.h"
#include "Conveyor_SystemDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define AH_CLR_BG          RGB(175, 175, 180)   
#define AH_CLR_PANEL       RGB(240, 240, 240)  
#define AH_CLR_TEXT        RGB( 20,  20,  20)  
#define AH_CLR_SUBTEXT     RGB(100, 100, 100)
#define AH_CLR_WARNING    RGB(200, 120, 0)
#define AH_CLR_DANGER      RGB(220,  50,  50)
#define AH_CLR_STOP        RGB(200,   0,   0)

static CString Utf8ToCString(const std::string& utf8)
{
    if (utf8.empty()) return _T("");
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    if (wlen <= 0) return _T("");
    CString result;
    LPWSTR buf = result.GetBuffer(wlen);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, buf, wlen);
    result.ReleaseBuffer();
    return result;
}

CAlertHistoryDlg::CAlertHistoryDlg(CDatabaseManager* pDB, CWnd* pParent)
    : CDialogEx(IDD_ALERT_HISTORY, pParent)
    , m_pDB(pDB)
{
}

void CAlertHistoryDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_ALERTS, m_listAlerts);
}

BEGIN_MESSAGE_MAP(CAlertHistoryDlg, CDialogEx)
    ON_WM_CTLCOLOR()
    ON_BN_CLICKED(IDC_BTN_ALERT_REFRESH, &CAlertHistoryDlg::OnBtnRefresh)
    ON_BN_CLICKED(IDC_BTN_ALERT_RESET, &CAlertHistoryDlg::OnBtnReset)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_ALERTS, &CAlertHistoryDlg::OnLvnItemchangedListAlerts)
END_MESSAGE_MAP()

BOOL CAlertHistoryDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_brBg.CreateSolidBrush(AH_CLR_BG);
    m_brPanel.CreateSolidBrush(AH_CLR_PANEL);

    m_listAlerts.SetExtendedStyle(
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    m_listAlerts.InsertColumn(0, _T("No."), LVCFMT_CENTER, 55);
    m_listAlerts.InsertColumn(1, _T("Time"), LVCFMT_CENTER, 200);
    m_listAlerts.InsertColumn(2, _T("Temp (C)"), LVCFMT_CENTER, 100);
    m_listAlerts.InsertColumn(3, _T("Humi (%)"), LVCFMT_CENTER, 100);
    m_listAlerts.InsertColumn(4, _T("Status"), LVCFMT_CENTER, 120);
    m_listAlerts.InsertColumn(5, _T("Content"), LVCFMT_CENTER, 420);

    m_listAlerts.SetBkColor(AH_CLR_PANEL);
    m_listAlerts.SetTextBkColor(AH_CLR_PANEL);
    m_listAlerts.SetTextColor(AH_CLR_TEXT);

    LoadAlerts();
    UpdateSummary();

    SetWindowText(_T("Alert History"));
    SetDlgItemText(IDC_STATIC_ALERT_TITLE, _T("Event Log"));

    return TRUE;
}

void CAlertHistoryDlg::OnCancel()
{
    m_brBg.DeleteObject();
    m_brPanel.DeleteObject();
    CDialogEx::OnCancel();
}

HBRUSH CAlertHistoryDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    UINT nID = pWnd->GetDlgCtrlID();

    if (nCtlColor == CTLCOLOR_DLG)
        return (HBRUSH)m_brBg.GetSafeHandle();

    if (nCtlColor == CTLCOLOR_STATIC)
    {
        if (nID == IDC_STATIC_ALERT_WARNING)
            pDC->SetTextColor(RGB(200, 120, 0));
        else if (nID == IDC_STATIC_ALERT_DANGER)
            pDC->SetTextColor(RGB(220, 50, 50));
        else if (nID == IDC_STATIC_ALERT_STOP)
            pDC->SetTextColor(RGB(180, 0, 0));
        else
            pDC->SetTextColor(AH_CLR_TEXT);

        pDC->SetBkMode(TRANSPARENT);
        return (HBRUSH)m_brBg.GetSafeHandle();
    }

    return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CAlertHistoryDlg::LoadAlerts()
{
    m_listAlerts.DeleteAllItems();
    if (!m_pDB || !m_pDB->IsOpen()) return;

    auto records = m_pDB->GetAlerts(200);
    int row = 0;

    for (const auto& rec : records)
    {
        if (rec.status == "DEFECT")
            continue;

        CString strNo, strTemp, strHumi;
        strNo.Format(_T("%d"), row + 1);

        CString strTime = Utf8ToCString(rec.timestamp);
        CString strStatus = Utf8ToCString(rec.status);
        CString strContent = Utf8ToCString(rec.content);

        if (rec.status == "STOP")
        {
            strTemp = _T("-");
            strHumi = _T("-");
        }
        else
        {
            strTemp.Format(_T("%.1f"), rec.temperature);
            strHumi.Format(_T("%.1f"), rec.humidity);
        }

        int idx = m_listAlerts.InsertItem(row, strNo);
        m_listAlerts.SetItemText(idx, 1, strTime);
        m_listAlerts.SetItemText(idx, 2, strTemp);
        m_listAlerts.SetItemText(idx, 3, strHumi);
        m_listAlerts.SetItemText(idx, 4, strStatus);
        m_listAlerts.SetItemText(idx, 5, strContent);
        row++;
    }
}

void CAlertHistoryDlg::UpdateSummary()
{
    if (!m_pDB || !m_pDB->IsOpen()) return;

    AlertSummary s = m_pDB->GetTodaySummary();

    CString strTotal, strWarn, strDanger, strStop;
    strTotal.Format(_T("Today's Total: %d"), s.totalCount);
    strWarn.Format(_T("Warning: %d"), s.warningCount);
    strDanger.Format(_T("Danger: %d"), s.dangerCount);
    strStop.Format(_T("Emergency Stop: %d"), s.stopCount);

    SetDlgItemText(IDC_STATIC_ALERT_TOTAL, strTotal);
    SetDlgItemText(IDC_STATIC_ALERT_WARNING, strWarn);
    SetDlgItemText(IDC_STATIC_ALERT_DANGER, strDanger);
    SetDlgItemText(IDC_STATIC_ALERT_STOP, strStop);
}

void CAlertHistoryDlg::OnBtnRefresh()
{
    LoadAlerts();
    UpdateSummary();
}

void CAlertHistoryDlg::OnBtnReset()
{
    if (MessageBox(_T("Delete all alert records?"),
        _T("Reset"), MB_YESNO | MB_ICONWARNING) == IDYES)
    {
        if (m_pDB && m_pDB->ClearAll())
        {
            CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_ALERTS);

            if (pList)
                pList->DeleteAllItems();
            LoadAlerts();
            UpdateSummary();
            MessageBox(_T("Reset Complete"), _T("Info"), MB_OK | MB_ICONINFORMATION);
        }
    }
}

void CAlertHistoryDlg::OnLvnItemchangedListAlerts(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    // TODO: Add control notification handler code here.
    *pResult = 0;
}