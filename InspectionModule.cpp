#include "pch.h"
#include "framework.h"
#include "InspectionModule.h"
#include "MonitoringView.h"
#include "ContentView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

LPCTSTR CInspectionModule::GetNavLabel(int idx) const
{
	static const LPCTSTR labels[] = {
		_T("Monitoring"), _T("Results"), _T("Recipe"), _T("Statistics"), _T("Config")
	};
	return (idx >= 0 && idx < GetNavCount()) ? labels[idx] : _T("");
}

CWnd* CInspectionModule::CreateView(int navIndex, CWnd* pParent, UINT nID)
{
	switch (navIndex)
	{
	case 0:
	{
		auto* p = new CMonitoringView();
		p->Create(nullptr, nullptr, AFX_WS_DEFAULT_VIEW & ~WS_VISIBLE,
			CRect(0, 0, 0, 0), pParent, nID);
		return p;
	}
	case 1: { auto* p = new CContentView(); p->Create(_T("Results"),     pParent, nID); return p; }
	case 2: { auto* p = new CContentView(); p->Create(_T("Recipe"), pParent, nID); return p; }
	case 3: { auto* p = new CContentView(); p->Create(_T("Statistics"),  pParent, nID); return p; }
	case 4: { auto* p = new CContentView(); p->Create(_T("Configuration"), pParent, nID); return p; }
	default: return nullptr;
	}
}

void CInspectionModule::OnConnect()
{
	m_connected = !m_connected;
	if (!m_connected) m_running = false;
}

void CInspectionModule::OnStart()
{
	if (m_connected) m_running = true;
}

void CInspectionModule::OnStop()
{
	m_running = false;
}
