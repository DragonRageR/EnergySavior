#pragma once
#include <windows.h>
#include <string>

class CrashHandler {
public:
    static void Init();
    static std::wstring GetExceptionDescription(DWORD code);
    static void GenerateCrashReport(EXCEPTION_POINTERS* ep, const std::wstring& customReason = L"");
    static void TriggerSyntheticCrash(); // For --test-crash verification
};
