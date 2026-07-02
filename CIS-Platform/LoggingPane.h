#pragma once
#include "Contracts/ILogger.h"
#include "UITheme.h"

// Bottom dock pane — rich-edit log with per-level colour coding.
// Thread-safe: Log() posts WM_LOG_MESSAGE; rich edit is only touched on the UI thread.
class CLoggingPane : public CDockablePane, public ILogger
{
    DECLARE_DYNAMIC(CLoggingPane)

public:
    void Log(LogLevel level, LPCTSTR message) override;
    void Clear();

protected:
    CRichEditCtrl m_richEdit;

    afx_msg int     OnCreate(LPCREATESTRUCT lp);
    afx_msg void    OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL    OnEraseBkgnd(CDC* pDC);
    afx_msg LRESULT OnLogMessage(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()
};
