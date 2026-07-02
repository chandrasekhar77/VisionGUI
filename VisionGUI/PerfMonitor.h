// PerfMonitor.h
// ---------------------------------------------------------------------------
// Lightweight, polled CPU/GPU usage sampler for the current process.
//
// CPU%  : delta of GetProcessTimes() kernel+user time over wall-clock time,
//         normalized by core count (0-100%, matching modern Task Manager).
// GPU%  : the "GPU Engine" PDH performance object Windows exposes since
//         Win10 1803 (the same data Task Manager's per-process GPU column
//         uses), filtered to this process's PID, taking the busiest engine.
//
// This file has NO MFC dependency on purpose, matching D3DRenderer.h, so it
// can be reused outside MFC.
// ---------------------------------------------------------------------------
#pragma once

#include <windows.h>
#include <pdh.h>
#include <vector>

class PerfMonitor
{
public:
    PerfMonitor();
    ~PerfMonitor();

    // Re-samples CPU% and GPU% for the current process. Cheap (sub-ms to a
    // few ms when the GPU counter list is rebuilt); call on a timer, e.g.
    // every 250-1000 ms, not once per rendered frame.
    void Sample();

    float GetCpuPercent() const { return m_cpuPercent; }
    float GetGpuPercent() const { return m_gpuPercent; }

private:
    void SampleCpu();
    void SampleGpu();
    void RebuildGpuCounters();   // re-discovers this process's GPU Engine instances

    // ---- CPU ----
    ULONGLONG m_lastCpuTickMs        = 0;
    ULONGLONG m_lastProcKernel100ns  = 0;
    ULONGLONG m_lastProcUser100ns    = 0;
    int       m_numCores             = 1;
    float     m_cpuPercent           = 0.0f;

    // ---- GPU (PDH "GPU Engine" object, filtered to our PID) ----
    PDH_HQUERY               m_gpuQuery = nullptr;
    std::vector<PDH_HCOUNTER> m_gpuCounters;
    ULONGLONG                 m_lastGpuRebuildTickMs = 0;
    float                     m_gpuPercent = 0.0f;
};
