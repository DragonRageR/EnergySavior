#pragma once
#include <windows.h>
#include <string>
#include <mutex>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
private:
    static inline std::mutex s_mutex;
    static inline std::wstring s_logFilePath;
    static inline bool s_initialized = false;

    static std::wstring GetTimestamp();
    static std::wstring LevelToString(LogLevel level);
    static void RotateIfNeeded();

public:
    static void Init();
    static void Log(LogLevel level, const std::wstring& moduleName, const std::wstring& message);
    static void Info(const std::wstring& moduleName, const std::wstring& message);
    static void Warn(const std::wstring& moduleName, const std::wstring& message);
    static void Error(const std::wstring& moduleName, const std::wstring& message);
    static void Debug(const std::wstring& moduleName, const std::wstring& message);
    
    static std::wstring GetLogFilePath();
    static void OpenLogFile();
};
