#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>

enum class UpdateStatus {
    Idle,
    Checking,
    UpToDate,
    UpdateAvailable,
    Downloading,
    ReadyToRestart,
    Failed
};

struct UpdateInfo {
    std::wstring latestVersion;
    std::wstring releaseDate;
    std::wstring releaseNotes;
    std::wstring downloadUrl;
    DWORD fileSize = 0;
    DWORD bytesDownloaded = 0;
};

class UpdateManager {
private:
    static inline std::mutex s_updateMutex;
    static inline std::atomic<bool> s_isChecking{false};
    static inline std::atomic<bool> s_isDownloading{false};
    static inline std::atomic<UpdateStatus> s_status{UpdateStatus::Idle};
    static inline UpdateInfo s_info;
    static inline std::wstring s_statusMessage = L"Оновлень не перевірялось";
    static inline HWND s_notifyHwnd = NULL;

    static void WorkerCheck(std::function<void(UpdateStatus, const UpdateInfo&)> onComplete);
    static void WorkerDownload(std::function<void(bool, const std::wstring&)> onComplete);

public:
    static void Init(HWND notifyHwnd = NULL);
    static void CheckForUpdatesAsync(std::function<void(UpdateStatus, const UpdateInfo&)> onComplete = nullptr);
    static void DownloadAndInstallAsync(std::function<void(bool, const std::wstring&)> onComplete = nullptr);

    static UpdateStatus GetStatus();
    static UpdateInfo GetInfo();
    static std::wstring GetStatusMessage();
    static std::wstring GetStatusDisplayString();

    static bool ParseVersion(const std::wstring& verStr, int& outYear, int& outMonth, int& outPatch, int& outStage);
    static bool IsNewerVersion(const std::wstring& currentVer, const std::wstring& remoteVer);
    static bool PerformSelfUpdateAndRestart(const std::wstring& updateFilePath);
};