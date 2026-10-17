#include "../../include/core/CrashHandler.hpp"
#include "../../include/core/Logger.hpp"
#include "../../include/core/AppConfig.hpp"
#include <shlobj.h>
#include <dbghelp.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <exception>

typedef BOOL(WINAPI* MiniDumpWriteDumpFn)(
    HANDLE hProcess,
    DWORD ProcessId,
    HANDLE hFile,
    MINIDUMP_TYPE DumpType,
    PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    PMINIDUMP_CALLBACK_INFORMATION CallbackParam
);

static LPTOP_LEVEL_EXCEPTION_FILTER s_prevFilter = nullptr;

std::wstring CrashHandler::GetExceptionDescription(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:         return L"EXCEPTION_ACCESS_VIOLATION (0xC0000005) — Неприпустимий доступ до пам'яті (читання/запис)";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return L"EXCEPTION_ARRAY_BOUNDS_EXCEEDED (0xC000008C) — Вихід за межі масиву";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return L"EXCEPTION_DATATYPE_MISALIGNMENT (0x80000002) — Невирівняні дані в пам'яті";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return L"EXCEPTION_FLT_DIVIDE_BY_ZERO (0xC000008E) — Ділення на нуль (плаваюча кома)";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return L"EXCEPTION_ILLEGAL_INSTRUCTION (0xC000001D) — Непідтримувана інструкція процесора";
        case EXCEPTION_IN_PAGE_ERROR:            return L"EXCEPTION_IN_PAGE_ERROR (0xC0000006) — Помилка сторінки пам'яті / диска";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return L"EXCEPTION_INT_DIVIDE_BY_ZERO (0xC0000094) — Цілочисельне ділення на нуль";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return L"EXCEPTION_NONCONTINUABLE_EXCEPTION (0xC0000025) — Невідновлюваний збій";
        case EXCEPTION_PRIV_INSTRUCTION:         return L"EXCEPTION_PRIV_INSTRUCTION (0xC0000096) — Спроба виконання привілейованої команди";
        case EXCEPTION_STACK_OVERFLOW:           return L"EXCEPTION_STACK_OVERFLOW (0xC00000FD) — Переповнення стеку викликів";
        case 0xE06D7363:                         return L"MSVC_CPP_EXCEPTION (0xE06D7363) — Неперехоплений C++ Exception (throw)";
        default: {
            wchar_t hex[32];
            swprintf_s(hex, L"UNKNOWN_EXCEPTION (0x%08X)", code);
            return std::wstring(hex);
        }
    }
}

void CrashHandler::GenerateCrashReport(EXCEPTION_POINTERS* ep, const wchar_t* customReason) {
    wchar_t appData[MAX_PATH] = { 0 };
    std::wstring dir = L".";
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData))) {
        dir = std::wstring(appData) + L"\\EnergyControl";
        CreateDirectoryW(dir.c_str(), NULL);
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[64];
    swprintf_s(timeBuf, L"%04d-%02d-%02d_%02d-%02d-%02d",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::wstring reportPath = dir + L"\\crash_report_" + timeBuf + L".log";
    std::wstring dumpPath   = dir + L"\\crash_dump_" + timeBuf + L".dmp";

    // 1. Write MiniDump if DbgHelp is available
    HMODULE hDbgHelp = LoadLibraryW(L"dbghelp.dll");
    bool dumpCreated = false;
    if (hDbgHelp) {
        MiniDumpWriteDumpFn pDump = (MiniDumpWriteDumpFn)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
        if (pDump) {
            HANDLE hFile = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                MINIDUMP_EXCEPTION_INFORMATION mei;
                mei.ThreadId = GetCurrentThreadId();
                mei.ExceptionPointers = ep;
                mei.ClientPointers = FALSE;

                dumpCreated = (pDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                                     MiniDumpNormal, ep ? &mei : NULL, NULL, NULL) != FALSE);
                CloseHandle(hFile);
            }
        }
        FreeLibrary(hDbgHelp);
    }

    // 2. Format detailed text report
    std::wstringstream report;
    report << L"======================================================================\n";
    report << L"               LENOVO ENERGYCONTROL — CRASH REPORT                    \n";
    report << L"======================================================================\n\n";

    report << L"Версія програми:      " << AppConfig::GetAppVersion() << L" (" << AppConfig::GetAppStage() << L")\n";
    report << L"Час збою:             " << timeBuf << L"\n";
    report << L"PID процесу:          " << GetCurrentProcessId() << L"\n";
    report << L"TID потоку:           " << GetCurrentThreadId() << L"\n";
    report << L"Права адміністратора: " << (AppConfig::IsElevated() ? L"ТАК (Elevated)" : L"НІ (Standard User)") << L"\n\n";

    if (customReason && *customReason) {
        report << L"Причина збою (User/Runtime): " << customReason << L"\n\n";
    }

    if (ep && ep->ExceptionRecord) {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        void* addr = ep->ExceptionRecord->ExceptionAddress;

        report << L"--- [ ДЕТАЛІ СИСТЕМНОГО ЗБОЮ ] ---\n";
        report << L"Код винятку:         " << GetExceptionDescription(code) << L"\n";
        report << L"Адреса інструкції:   0x" << std::hex << (uintptr_t)addr << std::dec << L"\n";

        // Query module of faulting address
        HMODULE hMod = NULL;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCWSTR)addr, &hMod) && hMod) {
            wchar_t modName[MAX_PATH] = { 0 };
            GetModuleFileNameW(hMod, modName, MAX_PATH);
            uintptr_t offset = (uintptr_t)addr - (uintptr_t)hMod;
            report << L"Модуль збою:         " << modName << L" (+0x" << std::hex << offset << std::dec << L")\n";
        }

        if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
            ULONG_PTR op = ep->ExceptionRecord->ExceptionInformation[0];
            ULONG_PTR targetAddr = ep->ExceptionRecord->ExceptionInformation[1];
            report << L"Операція доступу:    " << (op == 0 ? L"Спроба ЧИТАННЯ з" : (op == 1 ? L"Спроба ЗАПИСУ в" : L"Спроба ВИКОНАННЯ за адресою"))
                   << L" 0x" << std::hex << targetAddr << std::dec << L"\n";
        }
        report << L"\n";

        // Registers
        if (ep->ContextRecord) {
            CONTEXT* ctx = ep->ContextRecord;
            report << L"--- [ РЕГІСТРИ ПРОЦЕСОРА (x64) ] ---\n";
#if defined(_M_X64) || defined(__x86_64__)
            report << L"RAX = 0x" << std::hex << std::setw(16) << std::setfill(L'0') << ctx->Rax << L"  "
                   << L"RBX = 0x" << std::hex << std::setw(16) << std::setfill(L'0') << ctx->Rbx << L"\n"
                   << L"RCX = 0x" << std::hex << std::setw(16) << std::setfill(L'0') << ctx->Rcx << L"  "
                   << L"RDX = 0x" << std::hex << std::setw(16) << std::setfill(L'0') << ctx->Rdx << L"\n"
                   << L"RSI = 0x" << std::hex << std::setw(16) << std::setfill(L'0') << ctx->Rsi << L"  "
                   << L"RDI = 0x" << std::hex << std::setw(16) << std::setfill(L'0') << ctx->Rdi << L"\n"
                   << L"RBP = 0x" << std::hex << std::setw(16) << std::setfill(L'0') << ctx->Rbp << L"  "
                   << L"RSP = 0x" << std::hex << std::setw(16) << std::setfill(L'0') << ctx->Rsp << L"\n"
                   << L"RIP = 0x" << std::hex << std::setw(16) << std::setfill(L'0') << ctx->Rip << L"  "
                   << L"EFLAGS = 0x" << std::hex << std::setw(8) << std::setfill(L'0') << ctx->EFlags << std::dec << L"\n";
#endif
            report << L"\n";
        }

        // Stack Backtrace
        report << L"--- [ СТЕК ВИКЛИКІВ (STACK BACKTRACE) ] ---\n";
        void* stack[32];
        WORD frames = RtlCaptureStackBackTrace(0, 32, stack, NULL);
        for (WORD i = 0; i < frames; i++) {
            HMODULE hStackMod = NULL;
            wchar_t sModName[MAX_PATH] = L"UnknownModule";
            uintptr_t sOffset = (uintptr_t)stack[i];

            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCWSTR)stack[i], &hStackMod) && hStackMod) {
                GetModuleFileNameW(hStackMod, sModName, MAX_PATH);
                // Strip directory
                wchar_t* lastSlash = wcsrchr(sModName, L'\\');
                const wchar_t* shortMod = lastSlash ? (lastSlash + 1) : sModName;
                sOffset = (uintptr_t)stack[i] - (uintptr_t)hStackMod;
                report << L"#" << std::setw(2) << std::setfill(L'0') << i << L": "
                       << shortMod << L" + 0x" << std::hex << sOffset << std::dec << L" (0x" << (uintptr_t)stack[i] << L")\n";
            } else {
                report << L"#" << std::setw(2) << std::setfill(L'0') << i << L": 0x" << std::hex << sOffset << std::dec << L"\n";
            }
        }
        report << L"\n";
    }

    report << L"--- [ ФАЙЛИ ЗВІТУ ТА ДАМПУ ] ---\n";
    report << L"Текстовий звіт: " << reportPath << L"\n";
    if (dumpCreated) {
        report << L"Дамп пам'яті (Minidump): " << dumpPath << L" (може бути відкритий у WinDbg / Visual Studio)\n";
    }
    report << L"Основний журнал подій: " << Logger::GetLogFilePath() << L"\n\n";

    report << L"======================================================================\n";
    report << L"Будь ласка, прикріпіть цей файл або його зміст при відкритті Issue на GitHub!\n";
    report << L"======================================================================\n";

    // 3. Save to report file
    std::wstring fullStr = report.str();
    int len = WideCharToMultiByte(CP_UTF8, 0, fullStr.c_str(), (int)fullStr.length(), NULL, 0, NULL, NULL);
    if (len > 0) {
        std::string utf8(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, fullStr.c_str(), (int)fullStr.length(), &utf8[0], len, NULL, NULL);
        FILE* fp = _wfopen(reportPath.c_str(), L"wb");
        if (fp) {
            fwrite(utf8.data(), 1, utf8.size(), fp);
            fclose(fp);
        }
    }

    // 4. Log to regular application log as well
    Logger::Error(L"CRASH", L"CRITICAL APPLICATION CRASH OCCURRED! Report: " + reportPath);

    // 5. Native User Error Dialog (suppressed if --silent is passed)
    std::wstring msgBoxText = L"⚠️ СТАВСЯ КРИТИЧНИЙ ЗБІЙ ПРОГРАМИ (CRASH)\n\n"
                              L"EnergyControl зіткнувся з помилкою та не може продовжувати роботу.\n\n"
                              L"Звіт про помилку та дамп пам'яті збережено:\n" +
                              reportPath + L"\n\n"
                              L"Натисніть 'ОК', щоб відкрити папку зі звітом для надсилання розробнику.";

    bool isSilent = (wcsstr(GetCommandLineW(), L"--silent") != nullptr);
    if (!isSilent) {
        int btn = MessageBoxW(NULL, msgBoxText.c_str(), L"EnergyControl Crash Handler", MB_OKCANCEL | MB_ICONERROR | MB_SYSTEMMODAL);
        if (btn == IDOK) {
            // Highlight report in explorer
            std::wstring exploreCmd = L"/select,\"" + reportPath + L"\"";
            ShellExecuteW(NULL, L"open", L"explorer.exe", exploreCmd.c_str(), NULL, SW_SHOW);
        }
    }
}

static LONG WINAPI TopLevelExceptionHandler(EXCEPTION_POINTERS* ep) {
    CrashHandler::GenerateCrashReport(ep);
    if (s_prevFilter) return s_prevFilter(ep);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void TerminateHandler() {
    std::wstring reason = L"std::terminate() викликано через неперехоплений C++ Exception.";
    try {
        std::exception_ptr p = std::current_exception();
        if (p) std::rethrow_exception(p);
    } catch (const std::exception& e) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, NULL, 0);
        if (wlen > 0) {
            std::wstring we(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, &we[0], wlen);
            reason += L" Повідомлення: " + we;
        }
    } catch (...) {
        reason += L" Невідомий тип винятку (не std::exception).";
    }

    CrashHandler::GenerateCrashReport(nullptr, reason.c_str());
    abort();
}

void CrashHandler::Init() {
    s_prevFilter = SetUnhandledExceptionFilter(TopLevelExceptionHandler);
    std::set_terminate(TerminateHandler);
    Logger::Info(L"CrashHandler", L"Structured Exception Handler (SEH) & Terminate Handlers registered");
}

void CrashHandler::TriggerSyntheticCrash() {
    Logger::Warn(L"CrashHandler", L"Triggering synthetic crash for Crash Handler verification...");
    volatile int* ptr = nullptr;
    *ptr = 42; // Access Violation 0xC0000005
}
