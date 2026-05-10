#pragma once

#include "resource.h"
#include <vector>

class CLoginDlg : public CDialogEx
{
public:
    explicit CLoginDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_LOGIN_DIALOG };
#endif

    CString GetLoggedInUser() const { return m_strUserID; }

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override;       // Enter 키 → 로그인 검증 경유
    virtual void OnCancel() override;    // ESC 키 처리

    afx_msg void    OnBtnLogin();
    afx_msg void    OnPaint();
    afx_msg HBRUSH  OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void    OnSize(UINT nType, int cx, int cy);

    DECLARE_MESSAGE_MAP()

private:
    CString m_strUserID;
    CString m_strPassword;

    CBrush  m_brBg;
    CBrush  m_brPanel;
    CBrush  m_brEdit;

    struct UserAccount
    {
        CString id;
        CString pw;
    };

    std::vector<UserAccount> m_accounts;

    void InitDefaultAccounts();
    bool ValidateLogin(const CString& id, const CString& pw);
    void CenterLoginControls();
};