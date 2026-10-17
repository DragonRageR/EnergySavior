#include "../../include/core/Logger.hpp"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>

std::wstring Logger::GetTimestamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64];
    swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d.%03d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return std::wstring(buf);
}

std::wstring Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return L"DEBUG";
        case LogLevel::Info:    return L"INFO ";
        case LogLevel::Warning: return L"WARN ";
        case LogLevel::Error:   return L"ERROR";
        default:                return L"INFO ";
    }
}

void Logger::Init() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_initialized) return;

    wchar_t appData[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData))) {
        std::wstring dir = std::wstring(appData) + L"\\EnergyControl";
        CreateDirectoryW(dir.c_str(), NULL);
        s_logFilePath = dir + L"\\energy_control.log";
    } else {
        s_logFilePath = L"energy_control.log";
    }

    s_initialized = true;
    RotateIfNeeded();
}

void Logger::RotateIfNeeded() {
    struct _stat64 st;
    if (_wstat64(s_logFilePath.c_str(), &st) == 0) {
        if (st.st_size > 2 * 1024 * 1024) { // Rotate at 2 MB
            std::wstring oldPath = s_logFilePath + L".old";
            DeleteFileW(oldPath.c_str());
            MoveFileW(s_logFilePath.c_str(), oldPath.c_str());
        }
    }
}

void Logger::Log(LogLevel level, const std::wstring& moduleName, const std::wstring& message) {
    if (!s_initialized) Init();

    std::lock_guard<std::mutex> lock(s_mutex);
    RotateIfNeeded();

    std::wstring timeStr = GetTimestamp();
    std::wstring levelStr = LevelToString(level);

    std::wstringstream line;
    line << L"[" << timeStr << L"] [" << levelStr << L"] [" << moduleName << L"] " << message << L"\n";

    // Write UTF-8 to file
    std::wstring fullStr = line.str();
    int len = WideCharToMultiByte(CP_UTF8, 0, fullStr.c_str(), (int)fullStr.length(), NULL, 0, NULL, NULL);
    if (len > 0) {
        std::string utf8Str(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, fullStr.c_str(), (int)fullStr.length(), &utf8Str[0], len, NULL, NULL);

        FILE* fp = _wfopen(s_logFilePath.c_str(), L"ab");
        if (fp) {
            fwrite(utf8Str.data(), 1, utf8Str.size(), fp);
            fclose(fp);
        }
    }
}

void Logger::Info(const std::wstring& moduleName, const std::wstring& message) {
    Log(LogLevel::Info, moduleName, message);
}

void Logger::Warn(const std::wstring& moduleName, const std::wstring& message) {
    Log(LogLevel::Warning, moduleName, message);
}

void Logger::Error(const std::wstring& moduleName, const std::wstring& message) {
    Log(LogLevel::Error, moduleName, message);
}

void Logger::Debug(const std::wstring& moduleName, const std::wstring& message) {
    Log(LogLevel::Debug, moduleName, message);
}

std::wstring Logger::GetLogFilePath() {
    if (!s_initialized) Init();
    return s_logFilePath;
}

void Logger::OpenLogFile() {
    if (!s_initialized) Init();
    ShellExecuteW(NULL, L"open", s_logFilePath.c_str(), NULL, NULL, SW_SHOW);
}
