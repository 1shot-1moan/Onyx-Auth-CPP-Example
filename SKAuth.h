// ============================================================
//  Onyx Gate SDK  —  SKAuth.h
//  NO external dependencies — only Windows SDK (WinHTTP)
//  Works on any Windows C++ project out of the box.
//  Requires: C++17, Windows 10 SDK
// ============================================================
#pragma once

#pragma comment(lib, "winhttp.lib")

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <sstream>
#include <vector>

class SKAuth
{
    // ── Config ───────────────────────────────────────────────────────────────
    static constexpr const wchar_t* HOST = L"auth.script-kittens.com";

    std::string _appId;
    std::string _version;
    std::string _hwid;

    // ── Simple JSON helpers (no external library needed) ─────────────────────
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
        // trim whitespace
        val.erase(val.find_last_not_of(" \t\r\n") + 1);
        return val;
    }

    static bool JsonBool(const std::string& json, const std::string& key)
    {
        return JsonStr(json, key) == "true";
    }

    // ── HWID — reads C: volume serial ────────────────────────────────────────
    static std::string GetHWID()
    {
        DWORD serial = 0;
        GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
        std::ostringstream ss;
        ss << std::hex << std::uppercase << serial;
        return ss.str();
    }

    // ── HTTP POST via WinHTTP ─────────────────────────────────────────────────
    static std::string HttpPost(const std::wstring& path, const std::string& body)
    {
        HINTERNET hSession = WinHttpOpen(
            L"OnyxGate/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return R"({"ok":false,"message":"WinHTTP session failed"})";

        HINTERNET hConnect = WinHttpConnect(
            hSession, HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect)
        {
            WinHttpCloseHandle(hSession);
            return R"({"ok":false,"message":"Connection failed"})";
        }

        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect, L"POST", path.c_str(),
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (!hRequest)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return R"({"ok":false,"message":"Request failed"})";
        }

        std::wstring headers = L"Content-Type: application/json\r\n";
        BOOL sent = WinHttpSendRequest(
            hRequest,
            headers.c_str(), (DWORD)-1L,
            (LPVOID)body.c_str(), (DWORD)body.size(),
            (DWORD)body.size(), 0);

        std::string response;
        if (sent && WinHttpReceiveResponse(hRequest, nullptr))
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

        return response.empty()
            ? R"({"ok":false,"message":"Empty response"})"
            : response;
    }

    // ── Build JSON body ───────────────────────────────────────────────────────
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
    // ── User data (populated after login) ────────────────────────────────────
    std::string username;
    std::string plan;
    std::string expires;
    std::string sessionId;
    std::string email;
    std::string hwid;

    // ── Constructor ──────────────────────────────────────────────────────────
    SKAuth(const std::string& appId, const std::string& version = "1.0")
        : _appId(appId), _version(version), _hwid(GetHWID()) {}

    // ── Login ─────────────────────────────────────────────────────────────────
    // Returns: { ok, message, user: { username, plan, expires } }
    struct AuthResult { bool ok; std::string message; };

    AuthResult Login(const std::string& user, const std::string& pass)
    {
        auto body = BuildJson({
            {"appId",    _appId},
            {"username", user},
            {"password", pass},
            {"hwid",     _hwid},
            {"version",  _version}
        });

        auto resp = HttpPost(L"/sdk/login", body);
        bool ok   = JsonBool(resp, "ok");

        if (ok)
        {
            username  = JsonStr(resp, "username");
            plan      = JsonStr(resp, "plan");
            expires   = JsonStr(resp, "expires");
            sessionId = JsonStr(resp, "sessionId");
            email     = JsonStr(resp, "email");
            hwid      = JsonStr(resp, "hwid");
        }

        return { ok, JsonStr(resp, "message") };
    }

    // ── Register (new customer — needs license key) ───────────────────────────
    AuthResult Register(const std::string& user, const std::string& pass,
                        const std::string& email = "", const std::string& key = "")
    {
        auto body = BuildJson({
            {"appId",      _appId},
            {"username",   user},
            {"password",   pass},
            {"email",      email},
            {"licenseKey", key}
        });

        auto resp = HttpPost(L"/sdk/register", body);
        return { JsonBool(resp, "ok"), JsonStr(resp, "message") };
    }

    // ── Redeem — extend/upgrade an existing account with a key ───────────────
    AuthResult Redeem(const std::string& user, const std::string& key)
    {
        auto body = BuildJson({
            {"appId",      _appId},
            {"username",   user},
            {"licenseKey", key}
        });

        auto resp = HttpPost(L"/sdk/redeem", body);
        return { JsonBool(resp, "ok"), JsonStr(resp, "message") };
    }

    // ── GetVar — fetch a server-side variable by name ─────────────────────────
    // Set variables in dashboard → they update without recompiling your cheat
    // Example: store cheat version, download URL, announcement messages
    std::string GetVar(const std::string& name)
    {
        auto body = BuildJson({
            {"appId", _appId},
            {"name",  name}
        });

        auto resp = HttpPost(L"/sdk/variable", body);
        std::string val = JsonStr(resp, "value");
        return val.empty() ? JsonStr(resp, "message") : val;
    }

    // ── Helpers ───────────────────────────────────────────────────────────────
    bool IsPaid() const
    {
        return plan == "paid" || plan == "vip" || plan == "lifetime";
    }
};
