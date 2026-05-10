#include "pch.h"
#include "framework.h"
#include "LoginDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define LOGIN_CLR_BG        RGB(28, 30, 36)
#define LOGIN_CLR_PANEL     RGB(38, 42, 52)
#define LOGIN_CLR_EDIT_BG   RGB(20, 23, 30)
#define LOGIN_CLR_ACCENT    RGB(0, 170, 255)
#define LOGIN_CLR_GLOW      RGB(0, 170, 255)
#define LOGIN_CLR_TEXT      RGB(235, 240, 245)
#define LOGIN_CLR_SUBTEXT   RGB(160, 170, 185)

CLoginDlg::CLoginDlg(CWnd* pParent)
    : CDialogEx(IDD_LOGIN_DIALOG, pParent)
    , m_strUserID(_T(""))
    , m_strPassword(_T(""))
{
    InitDefaultAccounts();
}

void CLoginDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT_LOGIN_ID, m_strUserID);
    DDX_Text(pDX, IDC_EDIT_LOGIN_PW, m_strPassword);
}

BEGIN_MESSAGE_MAP(CLoginDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_LOGIN, &CLoginDlg::OnBtnLogin)
    ON_WM_PAINT()
    ON_WM_CTLCOLOR()
    ON_WM_SIZE()
END_MESSAGE_MAP()

void CLoginDlg::InitDefaultAccounts()
{
    m_accounts.clear();

    m_accounts.push_back({ _T("admin"), _T("1234") });
    m_accounts.push_back({ _T("user1"), _T("1234") });
    m_accounts.push_back({ _T("user2"), _T("1234") });
    m_accounts.push_back({ _T("guest"), _T("1234") });
}

bool CLoginDlg::ValidateLogin(const CString& id, const CString& pw)
{
    for (const auto& acc : m_accounts)
    {
        if (acc.id.CompareNoCase(id) == 0 && acc.pw == pw)
            return true;
    }
    return false;
}

BOOL CLoginDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Conveyor System Login"));

    m_brBg.CreateSolidBrush(LOGIN_CLR_BG);
    m_brPanel.CreateSolidBrush(LOGIN_CLR_PANEL);
    m_brEdit.CreateSolidBrush(LOGIN_CLR_EDIT_BG);

    CWnd* pIdEdit = GetDlgItem(IDC_EDIT_LOGIN_ID);
    if (pIdEdit)
    {
        pIdEdit->ModifyStyleEx(WS_EX_CLIENTEDGE, 0);
        pIdEdit->SetFocus();
    }

    CEdit* pPwEdit = (CEdit*)GetDlgItem(IDC_EDIT_LOGIN_PW);
    if (pPwEdit)
    {
        pPwEdit->ModifyStyleEx(WS_EX_CLIENTEDGE, 0);
        pPwEdit->SetPasswordChar(_T('*'));
    }

    CenterLoginControls();

    return FALSE;
}

void CLoginDlg::CenterLoginControls()
{
    CRect rect;
    GetClientRect(&rect);

    int panelW = 680;
    int panelH = 420;

    int px = (rect.Width() - panelW) / 2;
    int py = (rect.Height() - panelH) / 2;

    CWnd* pTitle = GetDlgItem(IDC_STATIC_LOGIN_TITLE);
    CWnd* pIdLabel = GetDlgItem(IDC_STATIC_LOGIN_ID);
    CWnd* pPwLabel = GetDlgItem(IDC_STATIC_LOGIN_PW);
    CWnd* pIdEdit = GetDlgItem(IDC_EDIT_LOGIN_ID);
    CWnd* pPwEdit = GetDlgItem(IDC_EDIT_LOGIN_PW);
    CWnd* pLogin = GetDlgItem(IDC_BTN_LOGIN);

    if (pTitle)
        pTitle->MoveWindow(px, py + 45, panelW, 35);

    int editW = 380;
    int editH = 42;
    int labelW = 45;

    int centerX = px + panelW / 2;
    int editX = centerX - editW / 2 + 20;
    int labelX = editX - labelW - 15;

    int idY = py + 145;
    int pwY = py + 220;

    if (pIdLabel)
        pIdLabel->MoveWindow(labelX, idY + 8, labelW, 28);

    if (pIdEdit)
        pIdEdit->MoveWindow(editX, idY, editW, editH);

    if (pPwLabel)
        pPwLabel->MoveWindow(labelX, pwY + 8, labelW, 28);

    if (pPwEdit)
        pPwEdit->MoveWindow(editX, pwY, editW, editH);

    int btnW = 140;
    int btnH = 38;
    int btnX = centerX - btnW / 2;

    if (pLogin)
        pLogin->MoveWindow(btnX, py + 315, btnW, btnH);
}

void CLoginDlg::OnPaint()
{
    CPaintDC dc(this);

    CRect rect;
    GetClientRect(&rect);

    dc.FillSolidRect(&rect, LOGIN_CLR_BG);

    int panelW = 700;
    int panelH = 420;

    int px = (rect.Width() - panelW) / 2;
    int py = (rect.Height() - panelH) / 2;

    CRect rcPanel(px, py, px + panelW, py + panelH);

    dc.FillSolidRect(&rcPanel, LOGIN_CLR_PANEL);

    CPen penBorder;
    penBorder.CreatePen(PS_SOLID, 1, LOGIN_CLR_ACCENT);

    CPen* pOldPen = dc.SelectObject(&penBorder);
    CBrush* pOldBrush = (CBrush*)dc.SelectStockObject(NULL_BRUSH);

    dc.RoundRect(rcPanel.left, rcPanel.top, rcPanel.right, rcPanel.bottom, 12, 12);

    dc.SelectObject(pOldPen);
    dc.SelectObject(pOldBrush);
    penBorder.DeleteObject();

    auto DrawEditGlow = [&](int editID)
        {
            CWnd* pEdit = GetDlgItem(editID);
            if (!pEdit) return;

            CRect rc;
            pEdit->GetWindowRect(&rc);
            ScreenToClient(&rc);

            rc.InflateRect(4, 4);

            CPen penGlow;
            penGlow.CreatePen(PS_SOLID, 2, LOGIN_CLR_GLOW);

            CPen* pOldGlowPen = dc.SelectObject(&penGlow);
            CBrush* pOldGlowBrush = (CBrush*)dc.SelectStockObject(NULL_BRUSH);

            dc.RoundRect(rc.left, rc.top, rc.right, rc.bottom, 7, 7);

            dc.SelectObject(pOldGlowPen);
            dc.SelectObject(pOldGlowBrush);
            penGlow.DeleteObject();
        };

    DrawEditGlow(IDC_EDIT_LOGIN_ID);
    DrawEditGlow(IDC_EDIT_LOGIN_PW);
}

HBRUSH CLoginDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
    int nID = pWnd->GetDlgCtrlID();

    if (nCtlColor == CTLCOLOR_DLG)
    {
        pDC->SetBkColor(LOGIN_CLR_BG);
        return (HBRUSH)m_brBg.GetSafeHandle();
    }

    if (nCtlColor == CTLCOLOR_STATIC)
    {
        if (nID == IDC_STATIC_LOGIN_TITLE)
        {
            pDC->SetTextColor(LOGIN_CLR_ACCENT);
            pDC->SetBkMode(TRANSPARENT);
            return (HBRUSH)m_brPanel.GetSafeHandle();
        }

        pDC->SetTextColor(LOGIN_CLR_TEXT);
        pDC->SetBkMode(TRANSPARENT);
        return (HBRUSH)m_brPanel.GetSafeHandle();
    }

    if (nCtlColor == CTLCOLOR_EDIT)
    {
        pDC->SetTextColor(LOGIN_CLR_TEXT);
        pDC->SetBkColor(LOGIN_CLR_EDIT_BG);
        return (HBRUSH)m_brEdit.GetSafeHandle();
    }

    return hbr;
}

void CLoginDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    if (GetSafeHwnd())
        CenterLoginControls();

    Invalidate();
}

void CLoginDlg::OnOK()
{
    // Enter 키를 누르면 여기로 오는데,
    // 바로 다이얼로그를 닫지 않고 로그인 검증을 거치도록 함
    OnBtnLogin();
}

void CLoginDlg::OnCancel()
{
    CDialogEx::OnCancel();
}

void CLoginDlg::OnBtnLogin()
{
    UpdateData(TRUE);

    if (m_strUserID.IsEmpty())
    {
        MessageBox(_T("Please enter ID."), _T("Login"), MB_ICONWARNING);
        GetDlgItem(IDC_EDIT_LOGIN_ID)->SetFocus();
        return;
    }

    if (m_strPassword.IsEmpty())
    {
        MessageBox(_T("Please enter password."), _T("Login"), MB_ICONWARNING);
        GetDlgItem(IDC_EDIT_LOGIN_PW)->SetFocus();
        return;
    }

    if (ValidateLogin(m_strUserID, m_strPassword))
    {
        CDialogEx::OnOK();
    }
    else
    {
        MessageBox(_T("Invalid ID or password."),
            _T("Login Failed"), MB_ICONERROR);

        m_strPassword.Empty();
        UpdateData(FALSE);
        GetDlgItem(IDC_EDIT_LOGIN_PW)->SetFocus();
    }
}