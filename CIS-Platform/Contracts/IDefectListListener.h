#pragma once
#include "DefectInfo.h"

// Owned by L4. Implemented by CDefectListPane.
// Called by L2 (SequenceController) from worker threads after each defect is found.
// Implementations must be thread-safe — use PostMessage internally.
class IDefectListListener
{
public:
    virtual ~IDefectListListener() = default;

    // Called once per defect found during inspection.
    virtual void OnDefectAdded(const DefectInfo& defect) = 0;

    // Called when a new panel starts — clears the previous panel's defect list.
    virtual void OnClear() = 0;
};
