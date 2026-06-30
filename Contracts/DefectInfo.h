#pragma once

// Plain POD struct — safe across DLL boundaries regardless of CRT linkage.
// All string fields are fixed TCHAR arrays to avoid heap ownership issues.

enum class DefectSeverity { Low, Medium, High, Critical };

struct DefectInfo
{
    int           defectId;         // unique ID within current inspection run
    int           panelId;          // panel sequence number
    int           roiIndex;         // zero-based ROI index in the model
    TCHAR         roiName[64];      // e.g. "ROI_03"
    TCHAR         defectType[64];   // e.g. "Scratch", "Void", "Bridging"
    DefectSeverity severity;
    int           x;                // defect X coordinate in panel image (pixels)
    int           y;                // defect Y coordinate in panel image (pixels)
    int           width;            // bounding box width  (0 if point defect)
    int           height;           // bounding box height (0 if point defect)
};
