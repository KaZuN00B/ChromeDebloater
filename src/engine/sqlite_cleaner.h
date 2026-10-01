#pragma once
#include <windows.h>
#include <string>
#include <vector>

class SQLiteCleaner {
private:
    typedef int (*sqlite3_open_v2_fn)(const char *filename, void **ppDb, int flags, const char *zVfs);
    typedef int (*sqlite3_exec_fn)(void *db, const char *sql, int (*callback)(void*,int,char**,char**), void *arg, char **errmsg);
    typedef int (*sqlite3_close_fn)(void *db);

    HMODULE m_hDll = nullptr;
    sqlite3_open_v2_fn m_pOpen = nullptr;
    sqlite3_exec_fn m_pExec = nullptr;
    sqlite3_close_fn m_pClose = nullptr;

public:
    SQLiteCleaner() {
        m_hDll = LoadLibraryW(L"winsqlite3.dll");
        if (m_hDll) {
            m_pOpen = (sqlite3_open_v2_fn)GetProcAddress(m_hDll, "sqlite3_open_v2");
            m_pExec = (sqlite3_exec_fn)GetProcAddress(m_hDll, "sqlite3_exec");
            m_pClose = (sqlite3_close_fn)GetProcAddress(m_hDll, "sqlite3_close");
        }
    }

    ~SQLiteCleaner() {
        if (m_hDll) {
            FreeLibrary(m_hDll);
            m_hDll = nullptr;
        }
    }

    bool IsAvailable() const {
        return (m_hDll && m_pOpen && m_pExec && m_pClose);
    }

    INT64 GetFileSize(const std::wstring& path) {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
            LARGE_INTEGER size;
            size.HighPart = fad.nFileSizeHigh;
            size.LowPart = fad.nFileSizeLow;
            return size.QuadPart;
        }
        return 0;
    }

    // Returns bytes saved
    INT64 OptimizeDatabase(const std::wstring& dbPathW) {
        if (!IsAvailable()) return 0;

        INT64 sizeBefore = GetFileSize(dbPathW);
        if (sizeBefore <= 0) return 0;

        int len = WideCharToMultiByte(CP_UTF8, 0, dbPathW.c_str(), -1, NULL, 0, NULL, NULL);
        if (len <= 0) return 0;
        std::string dbPathA(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, dbPathW.c_str(), -1, &dbPathA[0], len, NULL, NULL);

        void* db = nullptr;
        int rc = m_pOpen(dbPathA.c_str(), &db, 2 /* SQLITE_OPEN_READWRITE */, nullptr);
        if (rc != 0 || !db) return 0;

        char* errmsg = nullptr;
        m_pExec(db, "VACUUM;", nullptr, nullptr, &errmsg);
        m_pExec(db, "REINDEX;", nullptr, nullptr, &errmsg);
        m_pClose(db);

        INT64 sizeAfter = GetFileSize(dbPathW);
        return (sizeBefore > sizeAfter) ? (sizeBefore - sizeAfter) : 0;
    }
};
