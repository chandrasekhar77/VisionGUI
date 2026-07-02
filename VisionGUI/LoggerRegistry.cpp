#include "pch.h"
#include "framework.h"
#include "Contracts/LoggerRegistry.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace VisionLog
{

static ILogger* s_pLogger = nullptr;

void Set(ILogger* pLogger)
{
    s_pLogger = pLogger;
}

ILogger* Get()
{
    return s_pLogger;
}

void Debug(LPCTSTR message)
{
    if (s_pLogger) s_pLogger->Log(LogLevel::Debug, message);
}

void Info(LPCTSTR message)
{
    if (s_pLogger) s_pLogger->Log(LogLevel::Info, message);
}

void Warning(LPCTSTR message)
{
    if (s_pLogger) s_pLogger->Log(LogLevel::Warning, message);
}

void Error(LPCTSTR message)
{
    if (s_pLogger) s_pLogger->Log(LogLevel::Error, message);
}

} // namespace VisionLog
