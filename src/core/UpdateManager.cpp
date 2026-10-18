#include "../../include/core/UpdateManager.hpp"
#include "../../include/core/AppConfig.hpp"
#include "../../include/core/Logger.hpp"
#include "../../include/gui/TrayManager.hpp"

#include <winhttp.h>
#include <shlobj.h>
#include <sstream>
#include <iomanip>
#include <regex>
#include <fstream>
#include <thread>

#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif

void UpdateManager::Init(HWND notifyHwnd) {
    std::lock_guard<std::mutex> lock(s_updateMutex);
    s_notifyHwnd = notifyHwnd;
    s_status = UpdateStatus::Idle;
    s_statusMessage = L"Версія: " + AppConfig::GetAppVersion();
    Logger::Info(L"UpdateManager", L"Update subsystem initialized. Current version: " + AppConfig::GetAppVersion());
}

bool UpdateManager::ParseVersion(const std::wstring& verStr, int& outYear, int& outMonth, int& outPatch, int& outStage) {
    outYear = 0;
    outMonth = 0;
    outPatch = 0;
    outStage = 1; // Default to Alpha

    std::wstring lower = verStr;
    for (auto& c : lower) c = towlower(c);

    if (lower.find(L"alpha") != std::wstring::npos) outStage = 1;
    else if (lower.find(L"beta") != std::wstring::npos) outStage = 2;
    else if (lower.find(L"rc") != std::wstring::npos) outStage = 3;
    else outStage = 4; // Stable / Release

    // Find numbers: e.g. 2026.10 or 26.10, and optional .1
    std::wregex re(L"(\\d{2,4})[\\.\\-_](\\d{1,2})(?:[\\.\\-_](\\d+))?");
    std::wsmatch match;
    if (std::regex_search(verStr, match, re)) {
        outYear = std::stoi(match[1].str());
        if (outYear < 100) outYear += 2000;
        outMonth = std::stoi(match[2].str());
        if (match[3].matched) {
            outPatch = std::stoi(match[3].str());
        }
        return true;
    }
    return false;
}

bool UpdateManager::IsNewerVersion(const std::wstring& currentVer, const std::wstring& remoteVer) {
    int curY = 0, curM = 0, curP = 0, curS = 0;
    int remY = 0, remM = 0, remP = 0, remS = 0;

    if (!ParseVersion(currentVer, curY, curM, curP, curS)) return false;
    if (!ParseVersion(remoteVer, remY, remM, remP, remS)) return false;

    if (remY != curY) return remY > curY;
    if (remM != curM) return remM > curM;
    if (remP != curP) return remP > curP;
    return remS > curS;
}

void UpdateManager::WorkerCheck(std::function<void(UpdateStatus, const UpdateInfo&)> onComplete) {
    Logger::Info(L"UpdateManager", L"Checking for updates online...");
    {
        std::lock_guard<std::mutex> lock(s_updateMutex);
        s_status = UpdateStatus::Checking;
        s_statusMessage = L"Пошук оновлень...";
    }
    if (s_notifyHwnd) PostMessage(s_notifyHwnd, WM_USER + 2, 0, 0);

    std::wstring remoteVer = L"";
    std::wstring releaseNotes = L"";
    std::wstring downloadUrl = L"";

    HINTERNET hSession = WinHttpOpen(L"EnergySavior-Updater/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);

    bool checkOk = false;

    if (hSession) {
        HINTERNET hConnect = WinHttpConnect(hSession, APP_UPDATE_HOST,
                                            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                                                    APP_UPDATE_PATH,
                                                    NULL, WINHTTP_NO_REFERER,
                                                    WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                    WINHTTP_FLAG_SECURE);
            if (hRequest) {
                if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                    WinHttpReceiveResponse(hRequest, NULL)) {

                    DWORD statusCode = 0;
                    DWORD statusSize = sizeof(statusCode);
                    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

                    std::string responseBody;
                    if (statusCode == 200) {
                        DWORD bytesRead = 0;
                        char buffer[4096];
                        while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                            responseBody.append(buffer, bytesRead);
                        }
                    } else {
                        Logger::Warn(L"UpdateManager", L"HTTP error during update check: " + std::to_wstring(statusCode));
                    }

                    if (!responseBody.empty()) {
                        auto extractVal = [&](const std::string& key) -> std::wstring {
                            std::string search = "\"" + key + "\":";
                            size_t pos = responseBody.find(search);
                            if (pos != std::string::npos) {
                                size_t start = responseBody.find("\"", pos + search.length());
                                if (start != std::string::npos) {
                                    size_t end = responseBody.find("\"", start + 1);
                                    if (end != std::string::npos) {
                                        std::string val = responseBody.substr(start + 1, end - start - 1);
                                        int wlen = MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, NULL, 0);
                                        if (wlen > 0) {
                                            std::wstring wval(wlen - 1, 0);
                                            MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, &wval[0], wlen);
                                            return wval;
                                        }
                                    }
                                }
                            }
                            return L"";
                        };

                        remoteVer = extractVal("version");
                        downloadUrl = extractVal("download_url");
                        releaseNotes = extractVal("release_notes");

                        if (!remoteVer.empty()) {
                            checkOk = true;
                        }
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }

    UpdateStatus finalStatus;
    UpdateInfo finalInfo;
    {
        std::lock_guard<std::mutex> lock(s_updateMutex);
        if (!checkOk) {
            remoteVer = AppConfig::GetAppVersion();
            s_status = UpdateStatus::UpToDate;
            s_statusMessage = L"[OK] Актуальна версія (" + AppConfig::GetAppVersion() + L")";
            Logger::Info(L"UpdateManager", L"Актуальна версія збірки: " + AppConfig::GetAppVersion());
        } else {
            if (IsNewerVersion(AppConfig::GetAppVersion(), remoteVer)) {
                s_status = UpdateStatus::UpdateAvailable;
                s_info.latestVersion = remoteVer;
                s_info.downloadUrl = downloadUrl;
                s_info.releaseNotes = releaseNotes;
                s_statusMessage = L"[!] Доступне оновлення: " + remoteVer;
                Logger::Info(L"UpdateManager", L"Знайдено нову версію: " + remoteVer);
            } else {
                s_status = UpdateStatus::UpToDate;
                s_statusMessage = L"[OK] Актуальна версія (" + AppConfig::GetAppVersion() + L")";
                Logger::Info(L"UpdateManager", L"Встановлена найновіша версія: " + AppConfig::GetAppVersion());
            }
        }
        finalStatus = s_status;
        finalInfo = s_info;
    }

    if (s_notifyHwnd) PostMessage(s_notifyHwnd, WM_USER + 2, 0, 0);
    if (onComplete) onComplete(finalStatus, finalInfo);
}

void UpdateManager::CheckForUpdatesAsync(std::function<void(UpdateStatus, const UpdateInfo&)> onComplete) {
    bool expected = false;
    if (!s_isChecking.compare_exchange_strong(expected, true)) {
        return; // Already checking
    }

    std::thread([onComplete]() {
        WorkerCheck(onComplete);
        s_isChecking.store(false);
    }).detach();
}

void UpdateManager::WorkerDownload(std::function<void(bool, const std::wstring&)> onComplete) {
    std::wstring dlUrl;
    std::wstring ver;
    {
        std::lock_guard<std::mutex> lock(s_updateMutex);
        dlUrl = s_info.downloadUrl;
        ver = s_info.latestVersion;
        if (dlUrl.empty()) {
            s_status = UpdateStatus::Failed;
            s_statusMessage = L"Помилка: не знайдено посилання";
            if (onComplete) onComplete(false, s_statusMessage);
            return;
        }
        s_status = UpdateStatus::Downloading;
        s_statusMessage = L"Завантаження оновлення " + ver + L"...";
    }

    Logger::Info(L"UpdateManager", L"Downloading update from " + dlUrl);
    if (s_notifyHwnd) PostMessage(s_notifyHwnd, WM_USER + 2, 0, 0);

    wchar_t appData[MAX_PATH] = { 0 };
    std::wstring updateDir = L".";
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData))) {
        updateDir = std::wstring(appData) + L"\\EnergySavior\\updates";
        CreateDirectoryW(updateDir.c_str(), NULL);
    }
    std::wstring updateExe = updateDir + L"\\EnergySavior_update.exe";

    URLDownloadToFileW(NULL, dlUrl.c_str(), updateExe.c_str(), 0, NULL);

    DWORD attr = GetFileAttributesW(updateExe.c_str());
    bool ok = (attr != INVALID_FILE_ATTRIBUTES);
    std::wstring resMsg;
    {
        std::lock_guard<std::mutex> lock(s_updateMutex);
        if (ok) {
            s_status = UpdateStatus::ReadyToRestart;
            s_statusMessage = L"[OK] Оновлення завантажено. Перезапустіть для оновлення!";
            resMsg = updateExe;
            Logger::Info(L"UpdateManager", L"Update successfully downloaded to " + updateExe);
        } else {
            s_status = UpdateStatus::Failed;
            s_statusMessage = L"Не вдалося завантажити файл";
            resMsg = s_statusMessage;
            Logger::Error(L"UpdateManager", L"Failed to download update binary");
        }
    }

    if (s_notifyHwnd) PostMessage(s_notifyHwnd, WM_USER + 2, 0, 0);
    if (onComplete) onComplete(ok, resMsg);
}

void UpdateManager::DownloadAndInstallAsync(std::function<void(bool, const std::wstring&)> onComplete) {
    bool expected = false;
    if (!s_isDownloading.compare_exchange_strong(expected, true)) {
        return;
    }

    std::thread([onComplete]() {
        WorkerDownload(onComplete);
        s_isDownloading.store(false);
    }).detach();
}

bool UpdateManager::PerformSelfUpdateAndRestart(const std::wstring& updateFilePath) {
    wchar_t currentExe[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, currentExe, MAX_PATH);

    wchar_t appData[MAX_PATH] = { 0 };
    std::wstring updateDir = L".";
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData))) {
        updateDir = std::wstring(appData) + L"\\EnergySavior\\updates";
    }
    std::wstring batPath = updateDir + L"\\apply_update.bat";

    FILE* fp = _wfopen(batPath.c_str(), L"w");
    if (!fp) return false;

    std::string sCurrentExe;
    int lenCur = WideCharToMultiByte(CP_ACP, 0, currentExe, -1, NULL, 0, NULL, NULL);
    if (lenCur > 0) {
        sCurrentExe.resize(lenCur - 1);
        WideCharToMultiByte(CP_ACP, 0, currentExe, -1, &sCurrentExe[0], lenCur, NULL, NULL);
    }

    std::string sUpdatePath;
    int lenUp = WideCharToMultiByte(CP_ACP, 0, updateFilePath.c_str(), -1, NULL, 0, NULL, NULL);
    if (lenUp > 0) {
        sUpdatePath.resize(lenUp - 1);
        WideCharToMultiByte(CP_ACP, 0, updateFilePath.c_str(), -1, &sUpdatePath[0], lenUp, NULL, NULL);
    }

    std::ostringstream ss;
    ss << "@echo off\n";
    ss << "timeout /t 1 /nobreak >nul\n";
    ss << ":WAIT_PROCESS\n";
    ss << "tasklist | find /i \"EnergySavior.exe\" >nul\n";
    ss << "if not errorlevel 1 (\n";
    ss << "    timeout /t 1 /nobreak >nul\n";
    ss << "    goto :WAIT_PROCESS\n";
    ss << ")\n";
    ss << "copy /y \"" << sUpdatePath << "\" \"" << sCurrentExe << "\" >nul\n";
    ss << "start \"\" \"" << sCurrentExe << "\"\n";
    ss << "del \"" << sUpdatePath << "\" 2>nul\n";
    ss << "del \"%~f0\" 2>nul\n";
    ss << "exit\n";

    std::string script = ss.str();
    fwrite(script.data(), 1, script.size(), fp);
    fclose(fp);

    Logger::Info(L"UpdateManager", L"Applying self-update and restarting application via " + batPath);

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"open";
    sei.lpFile = batPath.c_str();
    sei.nShow = SW_HIDE;
    if (ShellExecuteExW(&sei)) {
        ExitProcess(0);
        return true;
    }
    return false;
}

UpdateStatus UpdateManager::GetStatus() {
    return s_status.load();
}

UpdateInfo UpdateManager::GetInfo() {
    std::lock_guard<std::mutex> lock(s_updateMutex);
    return s_info;
}

std::wstring UpdateManager::GetStatusMessage() {
    std::lock_guard<std::mutex> lock(s_updateMutex);
    return s_statusMessage;
}

std::wstring UpdateManager::GetStatusDisplayString() {
    std::lock_guard<std::mutex> lock(s_updateMutex);
    switch (s_status.load()) {
        case UpdateStatus::Checking:        return L"[~] Перевірка...";
        case UpdateStatus::UpdateAvailable: return L"[+] Доступне оновлення!";
        case UpdateStatus::Downloading:     return L"[>] Завантаження...";
        case UpdateStatus::ReadyToRestart:  return L"[!] Перезапуск";
        case UpdateStatus::Failed:          return L"[X] Помилка мережі";
        case UpdateStatus::UpToDate:
        default:                            return L"[OK] " + AppConfig::GetAppVersion();
    }
}