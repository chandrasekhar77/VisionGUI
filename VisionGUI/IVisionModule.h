#pragma once


// Pure interface every application module must implement.
// The shell (CMainFrame, CTopBar) talks only through this — it never
// knows which concrete module is loaded.
class IVisionModule
{
public:
	virtual ~IVisionModule() = default;

	// ── Identity ────────────────────────────────────────────────────────
	virtual LPCTSTR GetName() const = 0;          // e.g. "Inspection"

	// ── Navigation tabs ─────────────────────────────────────────────────
	virtual int     GetNavCount()          const = 0;
	virtual LPCTSTR GetNavLabel(int index) const = 0;

	// Shell calls this once per tab when loading the module.
	// Module allocates the CWnd, shell owns lifetime (DestroyWindow + delete).
	virtual CWnd* CreateView(int navIndex, CWnd* pParent, UINT nID) = 0;

	// ── Actions (Connect / Start / Stop buttons) ─────────────────────────
	virtual void OnConnect() {}
	virtual void OnStart()   {}
	virtual void OnStop()    {}

	virtual bool IsConnected() const { return false; }
	virtual bool IsRunning()   const { return false; }
};
