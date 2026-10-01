#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "browser_target.h"

typedef std::function<void(const std::wstring& msg, const std::wstring& level)> EngineLogCallback;

struct NetworkShieldStatus {
    bool hostsBlockActive = false;
    int hostsBlockedDomainsCount = 0;
    bool firewallBlockActive = false;
    int firewallActiveRulesCount = 0;
    std::wstring statusSummary;
};

class NetworkShield {
public:
    static NetworkShieldStatus GetStatus(const BrowserTarget& browser);

    // Hosts File Management
    static bool IsHostsBlockActive();
    static int GetHostsBlockedDomainsCount();
    static bool EnableHostsBlock(EngineLogCallback logCb);
    static bool DisableHostsBlock(EngineLogCallback logCb);

    // Windows Defender Firewall Management
    static bool IsFirewallBlockActive();
    static int GetActiveFirewallRulesCount();
    static bool EnableFirewallBlock(const BrowserTarget& browser, EngineLogCallback logCb);
    static bool DisableFirewallBlock(EngineLogCallback logCb);

    // Combined 1-Click Operations
    static bool EnableAll(const BrowserTarget& browser, EngineLogCallback logCb);
    static bool DisableAll(EngineLogCallback logCb);

    // Flushes Windows DNS cache
    static void FlushDns();
};
