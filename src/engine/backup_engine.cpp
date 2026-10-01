#include "backup_engine.h"
#include <iostream>
#include <ctime>

std::wstring BackupEngine::GetBackupDir() {
    std::wstring localApp = GetKnownFolderLocalApp();
    std::wstring bDir = localApp + L"\\ChromeDebloater\\Backups";
    if (!PathExists(bDir)) {
        CreateDirectoryW((localApp + L"\\ChromeDebloater").c_str(), NULL);
        CreateDirectoryW(bDir.c_str(), NULL);
    }
    return bDir;
}

bool BackupEngine::CreateSnapshot(const BrowserTarget& browser, std::wstring& outPath) {
    std::wstring bDir = GetBackupDir();

    time_t now = time(nullptr);
    tm ltm;
    localtime_s(&ltm, &now);

    wchar_t fname[128];
    swprintf_s(fname, L"Backup_%s_%04d-%02d-%02d_%02d%02d%02d.reg",
        browser.id.c_str(),
        ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
        ltm.tm_hour, ltm.tm_min, ltm.tm_sec
    );

    outPath = bDir + L"\\" + fname;

    std::wstring cmd = L"export \"HKLM\\" + browser.policyKey + L"\" \"" + outPath + L"\" /y";
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"open";
    sei.lpFile = L"reg.exe";
    sei.lpParameters = cmd.c_str();
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (ShellExecuteExW(&sei) && sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 5000);
        DWORD exitCode = 0;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);
        return (exitCode == 0 || PathExists(outPath));
    }
    return false;
}

std::vector<BackupSnapshot> BackupEngine::ListSnapshots() {
    std::vector<BackupSnapshot> list;
    std::wstring bDir = GetBackupDir();
    std::wstring search = bDir + L"\\Backup_*.reg";

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return list;

    do {
        BackupSnapshot s;
        s.filePath = bDir + L"\\" + fd.cFileName;
        s.id = fd.cFileName;

        // Parse filename: Backup_<browser>_<date>_<time>.reg
        std::wstring name = fd.cFileName;
        s.browserName = L"Browser Snapshot";
        if (name.find(L"chrome") != std::wstring::npos) s.browserName = L"Google Chrome";
        else if (name.find(L"brave") != std::wstring::npos) s.browserName = L"Brave Browser";
        else if (name.find(L"edge") != std::wstring::npos) s.browserName = L"Microsoft Edge";

        s.timestamp = name.substr(name.find(L"_") + 1);
        list.push_back(s);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return list;
}

bool BackupEngine::RestoreSnapshot(const std::wstring& filePath) {
    if (!PathExists(filePath)) return false;

    std::wstring cmd = L"import \"" + filePath + L"\"";
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"open";
    sei.lpFile = L"reg.exe";
    sei.lpParameters = cmd.c_str();
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (ShellExecuteExW(&sei) && sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 5000);
        DWORD exitCode = 0;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);
        return (exitCode == 0);
    }
    return false;
}

bool BackupEngine::ResetBrowserPolicies(const BrowserTarget& browser) {
    LSTATUS status = RegDeleteTreeW(HKEY_LOCAL_MACHINE, browser.policyKey.c_str());
    return (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND);
}
