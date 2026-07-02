// PerfMonitor.cpp
// ---------------------------------------------------------------------------
#include "PerfMonitor.h"

#include <cwchar>

#pragma comment(lib, "pdh.lib")

namespace
{
    ULONGLONG ToULL(const FILETIME& ft)
    {
        return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }
}

PerfMonitor::PerfMonitor()
{
    SYSTEM_INFO si = {};
    GetSystemInfo(&si);
    m_numCores = si.dwNumberOfProcessors > 0 ? static_cast<int>(si.dwNumberOfProcessors) : 1;
}

PerfMonitor::~PerfMonitor()
{
    if (m_gpuQuery) PdhCloseQuery(m_gpuQuery);
}

void PerfMonitor::Sample()
{
    SampleCpu();
    SampleGpu();
}

void PerfMonitor::SampleCpu()
{
    FILETIME creation, exit, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
        return;

    const ULONGLONG kernel100ns = ToULL(kernel);
    const ULONGLONG user100ns   = ToULL(user);
    const ULONGLONG nowMs       = GetTickCount64();

    if (m_lastCpuTickMs != 0)
    {
        const ULONGLONG dtMs = nowMs - m_lastCpuTickMs;
        if (dtMs > 0)
        {
            const ULONGLONG procDelta100ns = (kernel100ns - m_lastProcKernel100ns) +
                                              (user100ns   - m_lastProcUser100ns);
            const double procMs = static_cast<double>(procDelta100ns) / 10000.0; // 100ns -> ms
            m_cpuPercent = static_cast<float>((procMs / static_cast<double>(dtMs)) * 100.0 / m_numCores);
        }
    }

    m_lastCpuTickMs       = nowMs;
    m_lastProcKernel100ns = kernel100ns;
    m_lastProcUser100ns   = user100ns;
}

void PerfMonitor::RebuildGpuCounters()
{
    m_gpuCounters.clear();
    if (m_gpuQuery) { PdhCloseQuery(m_gpuQuery); m_gpuQuery = nullptr; }
    if (PdhOpenQuery(nullptr, 0, &m_gpuQuery) != ERROR_SUCCESS) return;

    wchar_t pidTag[32];
    swprintf_s(pidTag, L"pid_%lu_", GetCurrentProcessId());

    DWORD pathListChars = 0;
    PdhExpandWildCardPathW(nullptr, L"\\GPU Engine(*)\\Utilization Percentage",
        nullptr, &pathListChars, 0);
    if (pathListChars == 0) return;   // counter not present (pre-1803 Windows, etc.)

    std::vector<wchar_t> buf(pathListChars);
    if (PdhExpandWildCardPathW(nullptr, L"\\GPU Engine(*)\\Utilization Percentage",
            buf.data(), &pathListChars, 0) != ERROR_SUCCESS)
        return;

    for (wchar_t* p = buf.data(); *p; p += wcslen(p) + 1)
    {
        if (wcsstr(p, pidTag) == nullptr) continue;
        PDH_HCOUNTER hCounter = nullptr;
        if (PdhAddEnglishCounterW(m_gpuQuery, p, 0, &hCounter) == ERROR_SUCCESS)
            m_gpuCounters.push_back(hCounter);
    }

    // Seed the query so the first real Sample() after a rebuild has a
    // baseline to compute a delta against.
    if (!m_gpuCounters.empty())
        PdhCollectQueryData(m_gpuQuery);
}

void PerfMonitor::SampleGpu()
{
    const ULONGLONG nowMs = GetTickCount64();

    // GPU Engine instances come and go as the process touches different
    // engines (3D, copy, video decode, ...), so periodically re-discover
    // which instances belong to this PID rather than assuming a fixed set.
    if (m_lastGpuRebuildTickMs == 0 || nowMs - m_lastGpuRebuildTickMs > 2000)
    {
        RebuildGpuCounters();
        m_lastGpuRebuildTickMs = nowMs;
    }

    if (!m_gpuQuery || m_gpuCounters.empty())
    {
        m_gpuPercent = 0.0f;
        return;
    }

    if (PdhCollectQueryData(m_gpuQuery) != ERROR_SUCCESS) return;

    // Task Manager's per-process "GPU" column is the busiest single engine,
    // not the sum (a process can drive several engines concurrently without
    // meaningfully exceeding what one of them reports).
    double busiest = 0.0;
    for (PDH_HCOUNTER hCounter : m_gpuCounters)
    {
        PDH_FMT_COUNTERVALUE value = {};
        if (PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS)
            busiest = (value.doubleValue > busiest) ? value.doubleValue : busiest;
    }
    m_gpuPercent = static_cast<float>(busiest);
}
