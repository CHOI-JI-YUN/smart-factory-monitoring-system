#pragma once

#include "DatabaseManager.h"

// Alert History Dialog
class CAlertHistoryDlg : public CDialogEx
{
public:
    explicit CAlertHistoryDlg(CDatabaseManager* pDB, CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ALERT_HISTORY };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override {}
    virtual void OnCancel() override;

    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void   OnBtnRefresh();
    afx_msg void   OnBtnReset();

    DECLARE_MESSAGE_MAP()

private:
    CDatabaseManager* m_pDB;
    CListCtrl         m_listAlerts;

    CBrush  m_brBg;
    CBrush  m_brPanel;

    void LoadAlerts();
    void UpdateSummary();
public:
    afx_msg void OnLvnItemchangedListAlerts(NMHDR* pNMHDR, LRESULT* pResult);
};
