#pragma once

// Shared across all DLLs and the EXE.
// Pure virtual — safe across DLL boundaries when all modules use /MD (shared CRT).
// IMPORTANT: all projects in the solution must use /MD (not /MT) to share the heap.

enum class LogLevel { Debug, Info, Warning, Error };

class ILogger
{
public:
    virtual ~ILogger() = default;

    // Thread-safe — implementations must use PostMessage internally,
    // never touch UI controls directly from a worker thread.
    virtual void Log(LogLevel level, LPCTSTR message) = 0;
};
