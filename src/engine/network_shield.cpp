#include "network_shield.h"
#include <fstream>
#include <sstream>
#include <iostream>

typedef BOOL (WINAPI *DnsFlushResolverCacheFn)(VOID);

static const std::string kStartTag = "# === ChromeDebloater Telemetry Shield START ===";
static const std::string kEndTag   = "# === ChromeDebloater Telemetry Shield END ===";

static const std::vector<std::string> kTelemetryDomains = {
    // ── Google Chrome Telemetry & Experimentation ─────────────────────────
    "telemetry.google.com",
    "clients2.google.com",
    "clients4.google.com",
    "safebrowsing.googleapis.com",
    "sb-ssl.google.com",
    "crashpad.google.com",
    "optimizationguide-pa.googleapis.com",
    "chrome-variations.google.com",
    "variations.google.com",
    "client-channel.google.com",
    "metrics.gstatic.com",
    "tools.google.com",
    "update.googleapis.com",
    "omahaproxy.appspot.com",
    "redirector.gvt1.com",
    "app-measurement.com",
    "firebase-settings.crashlytics.com",
    "reports.crashlytics.com",
    "suggestqueries.google.com",
    "beacons.gvt2.com",
    "beacons2.gvt2.com",
    "beacons3.gvt2.com",
    "beacons4.gvt2.com",
    "beacons5.gvt2.com",
    "beacons.gcp.gvt2.com",
    "google-analytics.com",
    "www.google-analytics.com",
    "ssl.google-analytics.com",
    "analytics.google.com",
    "doubleclick.net",
    "pagead2.googlesyndication.com",
    "adservice.google.com",

    // ── Microsoft Edge Telemetry, Activity & Shopping ─────────────────────
    "edge.microsoft.com",
    "edge-enterprise.activity.windows.com",
    "msedge.api.cdp.microsoft.com",
    "data.msn.com",
    "arc.msn.com",
    "activity.windows.com",
    "config.edge.skype.com",
    "shopping.edge.microsoft.com",
    "browser.events.data.msn.com",
    "assets.msn.com",
    "edge.activity.windows.com",

    // ── Brave Telemetry, Analytics & Sponsored Services ───────────────────
    "p3a.brave.com",
    "variations.brave.com",
    "rewards.brave.com",
    "grant.rewards.brave.com",
    "laptop-updates.brave.com",
    "analytics.brave.com"
};

static std::wstring GetHostsFilePath() {
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    return std::wstring(sysDir) + L"\\drivers\\etc\\hosts";
}

void NetworkShield::FlushDns() {
    HMODULE hDns = LoadLibraryW(L"dnsapi.dll");
    if (hDns) {
        auto pFlush = (DnsFlushResolverCacheFn)GetProcAddress(hDns, "DnsFlushResolverCache");
        if (pFlush) pFlush();
        FreeLibrary(hDns);
    }
}

static bool ExecuteHiddenProcess(const std::wstring& cmd) {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    return false;
}

static void StripBlock(std::string& content, const std::string& startTag, const std::string& endTag) {
    size_t startPos = content.find(startTag);
    if (startPos != std::string::npos) {
        size_t endPos = content.find(endTag);
        if (endPos != std::string::npos) {
            endPos += endTag.length();
            while (endPos < content.length() && (content[endPos] == '\r' || content[endPos] == '\n')) {
                endPos++;
            }
            content.erase(startPos, endPos - startPos);
        }
    }
}

bool NetworkShield::IsHostsBlockActive() {
    std::wstring hPath = GetHostsFilePath();
    std::ifstream in(hPath, std::ios::binary);
    if (!in.is_open()) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    return (content.find(kStartTag) != std::string::npos ||
            content.find("# === ChromeDebloater Google Telemetry Block START ===") != std::string::npos);
}

int NetworkShield::GetHostsBlockedDomainsCount() {
    if (!IsHostsBlockActive()) return 0;
    return (int)kTelemetryDomains.size();
}

bool NetworkShield::EnableHostsBlock(EngineLogCallback logCb) {
    std::wstring hPath = GetHostsFilePath();
    DWORD attr = GetFileAttributesW(hPath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY)) {
        SetFileAttributesW(hPath.c_str(), attr & ~FILE_ATTRIBUTE_READONLY);
    }

    std::ifstream in(hPath, std::ios::binary);
    std::string content;
    if (in.is_open()) {
        std::stringstream ss;
        ss << in.rdbuf();
        content = ss.str();
        in.close();
    }

    // Strip previous blocks (both current and legacy tags)
    StripBlock(content, kStartTag, kEndTag);
    StripBlock(content, "# === ChromeDebloater Google Telemetry Block START ===", "# === ChromeDebloater Google Telemetry Block END ===");

    // Append clean new block
    if (!content.empty() && content.back() != '\n') {
        content += "\r\n";
    }

    std::string newBlock = kStartTag + "\r\n";
    for (const auto& domain : kTelemetryDomains) {
        newBlock += "0.0.0.0 " + domain + "\r\n";
    }
    newBlock += kEndTag + "\r\n";
    content += newBlock;

    std::ofstream out(hPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        logCb(L"[!] Failed to write to hosts file (check permissions): " + hPath, L"WARNING");
        return false;
    }

    out << content;
    out.close();

    FlushDns();
    logCb(L"[✓] " + std::to_wstring(kTelemetryDomains.size()) + L" Google, Edge, and Brave telemetry domains blocked via hosts file.", L"SUCCESS");
    return true;
}

bool NetworkShield::DisableHostsBlock(EngineLogCallback logCb) {
    std::wstring hPath = GetHostsFilePath();
    DWORD attr = GetFileAttributesW(hPath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY)) {
        SetFileAttributesW(hPath.c_str(), attr & ~FILE_ATTRIBUTE_READONLY);
    }

    std::ifstream in(hPath, std::ios::binary);
    if (!in.is_open()) return false;

    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    in.close();

    bool found = (content.find(kStartTag) != std::string::npos ||
                  content.find("# === ChromeDebloater Google Telemetry Block START ===") != std::string::npos);

    if (found) {
        StripBlock(content, kStartTag, kEndTag);
        StripBlock(content, "# === ChromeDebloater Google Telemetry Block START ===", "# === ChromeDebloater Google Telemetry Block END ===");

        std::ofstream out(hPath, std::ios::binary | std::ios::trunc);
        if (out.is_open()) {
            out << content;
            out.close();
            FlushDns();
            logCb(L"[✓] Hosts file telemetry blocklist removed and DNS cache flushed.", L"SUCCESS");
            return true;
        }
    }
    logCb(L"[i] No ChromeDebloater hosts blocklist was present.", L"INFO");
    return true;
}

bool NetworkShield::IsFirewallBlockActive() {
    std::wstring cmd = L"netsh advfirewall firewall show rule name=\"ChromeDebloater - Block Google Update (x64)\"";
    
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead, hWrite;
    CreatePipe(&hRead, &hWrite, &sa, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    bool found = false;
    if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWrite);
        char buf[256];
        DWORD bytesRead;
        std::string out;
        while (ReadFile(hRead, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buf[bytesRead] = 0;
            out += buf;
        }
        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hRead);

        if (out.find("ChromeDebloater") != std::string::npos) {
            found = true;
        }
    } else {
        CloseHandle(hRead);
        CloseHandle(hWrite);
    }
    return found;
}

int NetworkShield::GetActiveFirewallRulesCount() {
    return IsFirewallBlockActive() ? 12 : 0;
}

bool NetworkShield::EnableFirewallBlock(const BrowserTarget& browser, EngineLogCallback logCb) {
    logCb(L"Configuring Windows Defender Firewall outbound block rules for Chrome, Brave, and Edge...", L"ACTION");

    // Remove old rules first to ensure clean idempotent addition
    DisableFirewallBlock([](const std::wstring&, const std::wstring&) {});

    std::wstring pf = GetPF();
    std::wstring pf86 = GetPFX86();
    std::wstring localApp = GetKnownFolderLocalApp();

    struct RuleDef {
        std::wstring name;
        std::wstring exePath;
    } rules[] = {
        { L"ChromeDebloater - Block Google Update (x64)", pf + L"\\Google\\Update\\GoogleUpdate.exe" },
        { L"ChromeDebloater - Block Google Update (x86)", pf86 + L"\\Google\\Update\\GoogleUpdate.exe" },
        { L"ChromeDebloater - Block Google Update (User)", localApp + L"\\Google\\Update\\GoogleUpdate.exe" },
        { L"ChromeDebloater - Block Google Updater (System)", pf + L"\\Google\\GoogleUpdater\\updater.exe" },
        { L"ChromeDebloater - Block Google Updater (User)", localApp + L"\\Google\\GoogleUpdater\\updater.exe" },
        { L"ChromeDebloater - Block Microsoft Edge Update (x86)", pf86 + L"\\Microsoft\\EdgeUpdate\\MicrosoftEdgeUpdate.exe" },
        { L"ChromeDebloater - Block Microsoft Edge Update (x64)", pf + L"\\Microsoft\\EdgeUpdate\\MicrosoftEdgeUpdate.exe" },
        { L"ChromeDebloater - Block Microsoft Edge Update (User)", localApp + L"\\Microsoft\\EdgeUpdate\\MicrosoftEdgeUpdate.exe" },
        { L"ChromeDebloater - Block Brave Software Update (x86)", pf86 + L"\\BraveSoftware\\Update\\BraveUpdate.exe" },
        { L"ChromeDebloater - Block Brave Software Update (x64)", pf + L"\\BraveSoftware\\Update\\BraveUpdate.exe" },
        { L"ChromeDebloater - Block Brave Software Update (User)", localApp + L"\\BraveSoftware\\Update\\BraveUpdate.exe" },
        { L"ChromeDebloater - Block Chrome Crashpad Handler", browser.userDataDir + L"\\Crashpad\\crashpad_handler.exe" },
        { L"ChromeDebloater - Block Edge Crashpad Handler", localApp + L"\\Microsoft\\Edge\\User Data\\Crashpad\\crashpad_handler.exe" },
        { L"ChromeDebloater - Block Brave Crashpad Handler", localApp + L"\\BraveSoftware\\Brave-Browser\\User Data\\Crashpad\\crashpad_handler.exe" },
        { L"ChromeDebloater - Block Edge Identity Helper", pf86 + L"\\Microsoft\\Edge\\Application\\identity_helper.exe" },
        { L"ChromeDebloater - Block Edge Notification Helper", pf86 + L"\\Microsoft\\Edge\\Application\\notification_helper.exe" }
    };

    int added = 0;
    for (const auto& r : rules) {
        std::wstring cmd = L"netsh advfirewall firewall add rule name=\"" + r.name +
            L"\" dir=out action=block program=\"" + r.exePath + L"\" enable=yes";
        if (ExecuteHiddenProcess(cmd)) {
            added++;
        }
    }

    logCb(L"[✓] Windows Defender Firewall rules created: Outbound updaters & crashpads blocked (" + std::to_wstring(added) + L" rules enforced).", L"SUCCESS");
    return true;
}

bool NetworkShield::DisableFirewallBlock(EngineLogCallback logCb) {
    const wchar_t* ruleNames[] = {
        L"ChromeDebloater - Block Google Update (x64)",
        L"ChromeDebloater - Block Google Update (x86)",
        L"ChromeDebloater - Block Google Update (User)",
        L"ChromeDebloater - Block Google Updater (System)",
        L"ChromeDebloater - Block Google Updater (User)",
        L"ChromeDebloater - Block Microsoft Edge Update (x86)",
        L"ChromeDebloater - Block Microsoft Edge Update (x64)",
        L"ChromeDebloater - Block Microsoft Edge Update (User)",
        L"ChromeDebloater - Block Microsoft Edge Update",
        L"ChromeDebloater - Block Brave Software Update (x86)",
        L"ChromeDebloater - Block Brave Software Update (x64)",
        L"ChromeDebloater - Block Brave Software Update (User)",
        L"ChromeDebloater - Block Brave Software Update",
        L"ChromeDebloater - Block Chrome Crashpad Handler",
        L"ChromeDebloater - Block Edge Crashpad Handler",
        L"ChromeDebloater - Block Brave Crashpad Handler",
        L"ChromeDebloater - Block Edge Identity Helper",
        L"ChromeDebloater - Block Edge Notification Helper"
    };

    for (const auto* r : ruleNames) {
        std::wstring cmd = L"netsh advfirewall firewall delete rule name=\"" + std::wstring(r) + L"\"";
        ExecuteHiddenProcess(cmd);
    }

    logCb(L"[✓] ChromeDebloater firewall rules removed.", L"SUCCESS");
    return true;
}

bool NetworkShield::EnableAll(const BrowserTarget& browser, EngineLogCallback logCb) {
    bool ok1 = EnableHostsBlock(logCb);
    bool ok2 = EnableFirewallBlock(browser, logCb);
    return (ok1 && ok2);
}

bool NetworkShield::DisableAll(EngineLogCallback logCb) {
    bool ok1 = DisableHostsBlock(logCb);
    bool ok2 = DisableFirewallBlock(logCb);
    return (ok1 && ok2);
}

NetworkShieldStatus NetworkShield::GetStatus(const BrowserTarget& browser) {
    NetworkShieldStatus st;
    st.hostsBlockActive = IsHostsBlockActive();
    st.hostsBlockedDomainsCount = GetHostsBlockedDomainsCount();
    st.firewallBlockActive = IsFirewallBlockActive();
    st.firewallActiveRulesCount = GetActiveFirewallRulesCount();

    if (st.hostsBlockActive && st.firewallBlockActive) {
        st.statusSummary = L"Active & Fully Protected (Firewall Rules + " + std::to_wstring(st.hostsBlockedDomainsCount) + L" Hosts Entries)";
    } else if (st.hostsBlockActive) {
        st.statusSummary = L"Hosts Shield Active (" + std::to_wstring(st.hostsBlockedDomainsCount) + L" Domains Blocked · Firewall Inactive)";
    } else if (st.firewallBlockActive) {
        st.statusSummary = L"Firewall Shield Active (Outbound Updaters Blocked · Hosts Inactive)";
    } else {
        st.statusSummary = L"Shield Inactive (Telemetry and domains are unblocked)";
    }

    return st;
}
