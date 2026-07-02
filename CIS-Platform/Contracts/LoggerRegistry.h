#pragma once
#include "ILogger.h"

// Global logger registry — single point of access for all DLLs and the EXE.
//
// Usage in EXE (MainFrm::OnCreate):
//   VisionLog::Set(&m_wndLogPane);
//
// Usage in any DLL or layer (any thread):
//   VisionLog::Info(_T("Camera initialized"));
//   VisionLog::Error(_T("Model load failed"));
//
// DLLs can also receive ILogger* directly via their Initialize() method
// for unit-test scenarios where there is no UI.

namespace VisionLog
{
    // Called once at startup by the EXE before any DLL is used.
    void     Set(ILogger* pLogger);

    // Returns nullptr if Set() has not been called yet.
    ILogger* Get();

    // Null-safe convenience wrappers — safe to call from any thread, any DLL.
    // No-ops if Set() has not been called.
    void Debug  (LPCTSTR message);
    void Info   (LPCTSTR message);
    void Warning(LPCTSTR message);
    void Error  (LPCTSTR message);
}
