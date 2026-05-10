// Conveyor_System.cpp: Defines application class behavior.
//

#include "pch.h"
#include "framework.h"
#include "Conveyor_System.h"
#include "Conveyor_SystemDlg.h"
#include "LoginDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CConveyorSystemApp

BEGIN_MESSAGE_MAP(CConveyorSystemApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CConveyorSystemApp construction

CConveyorSystemApp::CConveyorSystemApp()
{
	// Restart manager support
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: Add construction code here.
	// Place all significant initialization in InitInstance.
}


// The one and only CConveyorSystemApp object.

CConveyorSystemApp theApp;


// CConveyorSystemApp initialization

BOOL CConveyorSystemApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP.

	if (!AfxOleInit())
		{
		    AfxMessageBox(_T("OLE Init Failed"));
		    return FALSE;
		}
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Include all common control classes used by the application.
	
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	// GDI+ initialization
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken = 0;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);


	// Create shell manager if the dialog contains
	// shell tree view or shell list view controls.
	CShellManager* pShellManager = new CShellManager;

	// Enable "Windows Native" visual manager for MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// Standard initialization



	// Change the registry key under which settings are stored.
	// TODO: Modify this string to your company or organization name.

	SetRegistryKey(_T("ConveyorControlSystem"));

	// ========== Login -> Main -> Logout loop ==========
	bool bKeepRunning = true;
	while (bKeepRunning)
	{
		// 1) Login dialog
		CLoginDlg loginDlg;
		INT_PTR nLoginResult = loginDlg.DoModal();

		if (nLoginResult != IDOK)
		{
			// Login cancelled -> exit program
			break;
		}

		// 2) Login success -> run main dialog
		CString strUser = loginDlg.GetLoggedInUser();
		CConveyorSystemDlg dlg;
		dlg.SetLoggedInUser(strUser);
		m_pMainWnd = &dlg;
		INT_PTR nResponse = dlg.DoModal();

		if (nResponse == ID_LOGOUT_RESULT)
		{
			// Logout -> return to login screen
			m_pMainWnd = nullptr;
			continue;
		}
		else
		{
			// IDOK, IDCANCEL, etc -> exit program
			bKeepRunning = false;
		}
	}

	// Delete the shell manager created above.
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// Since the dialog has been closed, return FALSE so that we exit the
	// application, rather than start the application's message pump.
	Gdiplus::GdiplusShutdown(gdiplusToken);
	return FALSE;
}