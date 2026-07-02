#pragma once

// Plain POD struct — safe across DLL boundaries regardless of CRT linkage.
// All string fields are fixed TCHAR arrays to avoid heap ownership issues.

enum class PanelStatus { OK, NG, Pending };

struct PanelRecord
{
    int         index;          // row/sequence number
    TCHAR       lot[32];
    TCHAR       id[32];
    int         defectCount;
    double      defectArea;
    PanelStatus status;
};
