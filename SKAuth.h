// ============================================================
//  Onyx Gate SDK  —  SKAuth.h
//  NO external dependencies — only Windows SDK (WinHTTP)
//  Requires: C++17, Windows 10 SDK
//  Linker:   winhttp.lib
// ============================================================
#pragma once
#pragma comment(lib, "winhttp.lib")

#include <windows.h>
#include <winhttp.h>
#include <tlhelp32.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>

class SKAuth
{
    static constexpr const wchar_t* HOST = L"auth.script-kittens.com";
    static constexpr INTERNET_PORT PORT = 443;
    static constexpr DWORD REQUEST_FLAGS = WINHTTP_FLAG_SECURE;

    std::string _appId;
    std::string _version;
    std::string _hwid;

    static std::string JsonStr(const std::string& json, const std::string& key)
    {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos) + 1;
        while (pos < json.size() && json[pos] == ' ') ++pos;
        if (pos >= json.size()) return "";
        if (json[pos] == '"')
        {
            ++pos;
            size_t end = json.find('"', pos);
            return json.substr(pos, end - pos);
        }
        size_t end = json.find_first_of(",}", pos);
        std::string val = json.substr(pos, end - pos);
        val.erase(val.find_last_not_of(" \t\r\n") + 1);
        return val;
    }

    static bool JsonBool(const std::string& json, const std::string& key)
    {
        return JsonStr(json, key) == "true";
    }

    static std::string GetHWID()
    {
        DWORD serial = 0;
        GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
        std::ostringstream ss;
        ss << std::hex << std::uppercase << serial;
        return ss.str();
    }

    static std::string HttpPost(const std::wstring& path, const std::string& body)
    {
        HINTERNET hSession = WinHttpOpen(L"OnyxGate/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "{\"ok\":false,\"message\":\"WinHTTP session failed\"}";

        HINTERNET hConnect = WinHttpConnect(hSession, HOST, PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return "{\"ok\":false,\"message\":\"Connection failed\"}"; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, REQUEST_FLAGS);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return "{\"ok\":false,\"message\":\"Request failed\"}"; }

        std::wstring headers = L"Content-Type: application/json\r\n";
        WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1L,
            (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);

        std::string response;
        if (WinHttpReceiveResponse(hRequest, nullptr))
        {
            DWORD bytesAvail = 0;
            while (WinHttpQueryDataAvailable(hRequest, &bytesAvail) && bytesAvail > 0)
            {
                std::vector<char> buf(bytesAvail + 1, 0);
                DWORD bytesRead = 0;
                WinHttpReadData(hRequest, buf.data(), bytesAvail, &bytesRead);
                response.append(buf.data(), bytesRead);
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response.empty() ? "{\"ok\":false,\"message\":\"Empty response\"}" : response;
    }

    static std::string BuildJson(std::initializer_list<std::pair<std::string,std::string>> fields)
    {
        std::string json = "{";
        bool first = true;
        for (auto& [k, v] : fields)
        {
            if (!first) json += ",";
            json += "\"" + k + "\":\"" + v + "\"";
            first = false;
        }
        return json + "}";
    }

public:
    struct AuthResult { bool ok; std::string message; };

    // Public fields populated after Login()
    std::string username, plan, expires, apiKey, sessionId, email, hwid;

    SKAuth(const std::string& appId, const std::string& version = "1.0", const std::string& secret = "")
        : _appId(appId), _version(version), _secret(secret), _hwid(GetHWID())
    {
        CheckSecurity();
        Init();
        StartAntiDllInjectionMonitor();
    }

    std::string ParseSecureResponse(const std::string& resp)
    {
        bool ok = JsonBool(resp, "ok");
        std::string encStr = JsonStr(resp, "enc");
        if (!encStr.empty())
        {
            std::string sig = JsonStr(resp, "sig");
            if (!sig.empty() && !_secret.empty())
            {
                std::string computedSig = ComputeHmacHex(encStr, _secret);
                if (sig != computedSig)
                {
                    ReportSecurityFlag("packet_tampering", "HMAC signature mismatch detected on AES payload");
                    MessageBoxW(NULL, L"Security Violation: Network packet tampering detected.", L"Onyx Gate Security", MB_OK | MB_ICONERROR);
                    TerminateProcess(GetCurrentProcess(), 0);
                    return "{\"ok\":false,\"message\":\"Packet tampering detected\"}";
                }
            }
            std::string decrypted = DecryptAes256(encStr, _secret);
            return decrypted.empty() ? resp : decrypted;
        }
        return resp;
    }

    bool Init()
    {
        auto body = BuildJson({
            {"appId", _appId},
            {"hwid", _hwid},
            {"version", _version}
        });
        auto rawResp = HttpPost(L"/sdk/init", body);
        auto resp = ParseSecureResponse(rawResp);
        bool ok = JsonBool(resp, "ok");
        if (!ok)
        {
            std::string msg = JsonStr(resp, "message");
            if (msg.empty()) msg = "Access Denied: Your HWID or IP address is blacklisted.";
            std::wstring wmsg(msg.begin(), msg.end());
            MessageBoxW(NULL, wmsg.c_str(), L"Onyx Gate Security — Access Denied", MB_OK | MB_ICONERROR);
            TerminateProcess(GetCurrentProcess(), 0);
            return false;
        }
        return true;
    }

    bool checkblack()
    {
        return Init();
    }

    bool checkban(const std::string& username = "")
    {
        auto body = BuildJson({{"appId", _appId}, {"hwid", _hwid}, {"username", username}});
        auto rawResp = HttpPost(L"/sdk/check-ban", body);
        auto resp = ParseSecureResponse(rawResp);
        bool ok = JsonBool(resp, "ok");
        if (!ok)
        {
            std::string msg = JsonStr(resp, "message");
            if (msg.empty()) msg = "Hardware or IP address is banned!";
            std::wstring wmsg(msg.begin(), msg.end());
            MessageBoxW(NULL, wmsg.c_str(), L"Onyx Gate Security — Access Denied", MB_OK | MB_ICONERROR);
            TerminateProcess(GetCurrentProcess(), 0);
            return true;
        }
        return false;
    }

    // Returns the system drive root (e.g. L"c:\\windows")
    static std::wstring GetWindowsDir()
    {
        wchar_t buf[MAX_PATH] = {};
        GetWindowsDirectoryW(buf, MAX_PATH);
        std::wstring p = buf;
        std::transform(p.begin(), p.end(), p.begin(), ::tolower);
        return p; // e.g. "c:\\windows"
    }

    // Returns the directory of the running .exe
    static std::wstring GetExeDir()
    {
        wchar_t buf[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring p = buf;
        std::transform(p.begin(), p.end(), p.begin(), ::tolower);
        size_t last = p.find_last_of(L"\\/");
        return (last != std::wstring::npos) ? p.substr(0, last) : p;
    }

    void StartAntiDllInjectionMonitor()
    {
        std::thread([this]() {
            std::set<std::wstring> allowedModules;
            HANDLE hProc = GetCurrentProcess();

            auto addModulesToAllowed = [&]() {
                HMODULE hMods[1024];
                DWORD cbNeeded = 0;
                if (EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded))
                {
                    DWORD count = cbNeeded / sizeof(HMODULE);
                    for (DWORD i = 0; i < count; i++)
                    {
                        WCHAR path[MAX_PATH] = {};
                        if (GetModuleFileNameExW(hProc, hMods[i], path, MAX_PATH))
                        {
                            std::wstring p = path;
                            std::transform(p.begin(), p.end(), p.begin(), ::towupper);
                            allowedModules.insert(p);
                        }
                    }
                }
            };

            // Warmup phase (10 ticks x 500ms = 5s): continuously expand allowed snapshot while app initializes
            for (int tick = 0; tick < 10; tick++)
            {
                addModulesToAllowed();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            // Active monitoring phase: check for unknown non-system DLLs injected after warmup
            while (true)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));

                HMODULE hMods[1024];
                DWORD cbNeeded = 0;
                if (!EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded))
                    continue;

                DWORD count = cbNeeded / sizeof(HMODULE);
                bool triggered = false;
                std::string triggeredMod;

                for (DWORD i = 0; i < count; i++)
                {
                    WCHAR path[MAX_PATH] = {};
                    if (!GetModuleFileNameExW(hProc, hMods[i], path, MAX_PATH))
                        continue;

                    std::wstring wpath(path);
                    std::wstring upper = wpath;
                    std::transform(upper.begin(), upper.end(), upper.begin(), ::towupper);

                    // Skip trusted Windows system & Program Files directories
                    if (upper.find(L"C:\\WINDOWS\\") != std::wstring::npos ||
                        upper.find(L"C:\\PROGRAM FILES\\") != std::wstring::npos ||
                        upper.find(L"C:\\PROGRAM FILES (X86)\\") != std::wstring::npos)
                    {
                        continue;
                    }

                    // Skip DLLs that were part of the initial warmup snapshot
                    if (allowedModules.find(upper) != allowedModules.end())
                        continue;

                    // Foreign/unauthorized DLL injected!
                    triggeredMod = std::string(wpath.begin(), wpath.end());
                    triggered = true;
                    break;
                }

                if (triggered)
                {
                    ReportSecurityFlag("dll_injection_detected", "Unauthorized DLL injected: " + triggeredMod);
                    std::this_thread::sleep_for(std::chrono::milliseconds(400));
                    TerminateProcess(GetCurrentProcess(), 0);
                    return;
                }
            }
        }).detach();
    }

    void ReportSecurityFlag(const std::string& flagType, const std::string& details = "")
    {
        auto body = BuildJson({
            {"appId", _appId},
            {"username", username.empty() ? "Unknown" : username},
            {"hwid", _hwid},
            {"flagType", flagType},
            {"details", details}
        });
        HttpPost(L"/sdk/security-flag", body);
    }

    bool CheckSecurity()
    {
        if (IsDebuggerPresent())
        {
            ReportSecurityFlag("debugger_detected", "Win32 IsDebuggerPresent() returned True");
            return false;
        }

        BOOL isRemote = FALSE;
        if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemote) && isRemote)
        {
            ReportSecurityFlag("debugger_detected", "Win32 CheckRemoteDebuggerPresent returned True");
            return false;
        }

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32W pe;
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(hSnap, &pe))
            {
                const std::vector<std::wstring> badProcs = { L"x64dbg.exe", L"x32dbg.exe", L"cheatengine-x86_64.exe", L"ida64.exe", L"processhacker.exe" };
                do {
                    std::wstring name = pe.szExeFile;
                    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                    for (const auto& bad : badProcs)
                    {
                        if (name.find(bad) != std::wstring::npos)
                        {
                            CloseHandle(hSnap);
                            ReportSecurityFlag("blacklisted_process", "Detected process: " + std::string(name.begin(), name.end()));
                            return false;
                        }
                    }
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }
        return true;
    }

    AuthResult Login(const std::string& user, const std::string& pass)
    {
        CheckSecurity();
        checkban(user);
        auto body = BuildJson({
            {"appId",_appId},{"username",user},{"password",pass},
            {"hwid",_hwid},{"version",_version}
        });
        auto rawResp = HttpPost(L"/sdk/login", body);
        auto resp = ParseSecureResponse(rawResp);
        bool ok = JsonBool(resp, "ok");
        if (ok)
        {
            username  = JsonStr(resp, "username");
            plan      = JsonStr(resp, "plan");
            expires   = JsonStr(resp, "expires");
            apiKey    = JsonStr(resp, "apiKey");
            sessionId = JsonStr(resp, "sessionId");
            email     = JsonStr(resp, "email");
            hwid      = JsonStr(resp, "hwid");
        }
        else
        {
            std::string msg = JsonStr(resp, "message");
            if (msg.find("banned") != std::string::npos || msg.find("Banned") != std::string::npos || msg.find("blacklisted") != std::string::npos)
            {
                std::wstring wmsg(msg.begin(), msg.end());
                MessageBoxW(NULL, wmsg.c_str(), L"Onyx Gate Security — Account Banned", MB_OK | MB_ICONERROR);
                TerminateProcess(GetCurrentProcess(), 0);
            }
        }
        return { ok, JsonStr(resp, "message") };
    }

    AuthResult Register(const std::string& user, const std::string& pass,
                        const std::string& email = "", const std::string& key = "")
    {
        auto body = BuildJson({
            {"appId",_appId},{"username",user},{"password",pass},
            {"email",email},{"licenseKey",key}
        });
        auto resp = HttpPost(L"/sdk/register", body);
        return { JsonBool(resp, "ok"), JsonStr(resp, "message") };
    }

    AuthResult Redeem(const std::string& user, const std::string& key)
    {
        auto body = BuildJson({{"appId",_appId},{"username",user},{"licenseKey",key}});
        auto resp = HttpPost(L"/sdk/redeem", body);
        return { JsonBool(resp, "ok"), JsonStr(resp, "message") };
    }

    std::string GetVar(const std::string& name)
    {
        auto body = BuildJson({{"appId",_appId},{"name",name}});
        auto resp = HttpPost(L"/sdk/variable", body);
        return JsonStr(resp, "value");
    }

    bool IsPaid() const
    {
        return !plan.empty() && plan != "free";
    }

    AuthResult Validate()
    {
        CheckSecurity();
        if (apiKey.empty()) return { false, "Not logged in" };
        auto body = BuildJson({{"appId",_appId},{"apiKey",apiKey},{"hwid",_hwid}});
        auto resp = HttpPost(L"/sdk/validate", body);
        bool ok = JsonBool(resp, "ok");
        if (ok) {
            auto p = JsonStr(resp, "plan");    if (!p.empty())    plan    = p;
            auto e = JsonStr(resp, "expires"); if (!e.empty())    expires = e;
        }
        return { ok, JsonStr(resp, "message") };
    }
};

// ── Quick start ──────────────────────────────────────────────────────────────
// SKAuth auth("6a6356f72c9481f42186ef1b", "1.0");
// auto r = auth.Login("username", "password");
// if (r.ok) { printf("[OK] Welcome %s! Plan: %s\n", auth.username.c_str(), auth.plan.c_str()); }