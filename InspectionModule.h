#pragma once
#include "IVisionModule.h"

class CInspectionModule : public IVisionModule
{
public:
	LPCTSTR GetName()             const override { return _T("Inspection"); }
	int     GetNavCount()         const override { return 5; }
	LPCTSTR GetNavLabel(int idx)  const override;

	CWnd* CreateView(int navIndex, CWnd* pParent, UINT nID) override;

	void OnConnect() override;
	void OnStart()   override;
	void OnStop()    override;

	bool IsConnected() const override { return m_connected; }
	bool IsRunning()   const override { return m_running; }

private:
	bool m_connected = false;
	bool m_running   = false;
};
