#pragma once
#include "PanelRecord.h"

// Owned by L4. Implemented by CPanelListPane.
// Called by L3 (SequenceController) from worker threads after each panel completes.
// Implementations must be thread-safe — use PostMessage internally.
class IPanelListListener
{
public:
    virtual ~IPanelListListener() = default;

    // Called once per panel completed during inspection.
    virtual void OnPanelRecorded(const PanelRecord& record) = 0;

    // Called to clear the panel/sheet history list (e.g. new lot started).
    virtual void OnPanelListClear() = 0;
};
