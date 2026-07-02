
// CIS-Platform.h : main header file for the CIS-Platform application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols


// CCISPlatformApp:
// See CIS-Platform.cpp for the implementation of this class
//

class CCISPlatformApp : public CWinApp
{
public:
	CCISPlatformApp() noexcept;


// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation

public:
	UINT              m_nAppLook;
	ULONG_PTR         m_gdiplusToken = 0;
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
};

extern CCISPlatformApp theApp;
