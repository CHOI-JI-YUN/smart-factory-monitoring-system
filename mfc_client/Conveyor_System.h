
// Conveyor_System.h: Main header file for the application.
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH."
#endif

#include "resource.h"		// main symbols


// CConveyorSystemApp:
// See Conveyor_System.cpp for the implementation of this class.
//

class CConveyorSystemApp : public CWinApp
{
public:
	CConveyorSystemApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CConveyorSystemApp theApp;
