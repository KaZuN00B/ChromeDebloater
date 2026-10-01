#pragma once
#include <windows.h>
#include <string>
#include <functional>

class SQLiteHelper {
private:
    typedef int (*sqlite3_open_v2_fn)(const char *filename, void **ppDb, int flags, const char *zVfs);
    typedef int (*sqlite3_exec_fn)(void *db, const char *sql, int (*callback)(void*,int,char**,char**), void *arg, char **errmsg);
    typedef int (*sqlite3_close_fn)(void *db);

    HMODULE m_hDll = nullptr;
    sqlite3_open_v2_fn m_pOpen = nullptr;
    sqlite3_exec_fn m_pExec = nullptr;
    sqlite3_close_fn m_pClose = nullptr;

public:
    SQLiteHelper() {
        m_hDll = LoadLibraryW(L"winsqlite3.dll");
        if (m_hDll) {
            m_pOpen = (sqlite3_open_v2_fn)GetProcAddress(m_hDll, "sqlite3_open_v2");
            m_pExec = (sqlite3_exec_fn)GetProcAddress(m_hDll, "sqlite3_exec");
            m_pClose = (sqlite3_close_fn)GetProcAddress(m_hDll, "sqlite3_close");
        }
    }

    ~SQLiteHelper() {
        if (m_hDll) {
            FreeLibrary(m_hDll);
            m_hDll = nullptr;
        }
    }

    bool IsAvailable() const {
        return (m_hDll && m_pOpen && m_pExec && m_pClose);
    }

    bool VacuumAndReindex(const std::wstring& dbPathW) {
        if (!IsAvailable()) return false;

        // Convert path to UTF-8
        int len = WideCharToMultiByte(CP_UTF8, 0, dbPathW.c_str(), -1, NULL, 0, NULL, NULL);
        if (len <= 0) return false;
        std::string dbPathA(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, dbPathW.c_str(), -1, &dbPathA[0], len, NULL, NULL);

        void* db = nullptr;
        // SQLITE_OPEN_READWRITE (2)
        int rc = m_pOpen(dbPathA.c_str(), &db, 2, nullptr);
        if (rc != 0 || !db) {
            return false;
        }

        char* errmsg = nullptr;
        m_pExec(db, "VACUUM;", nullptr, nullptr, &errmsg);
        m_pExec(db, "REINDEX;", nullptr, nullptr, &errmsg);
        m_pClose(db);
        return true;
    }
};
