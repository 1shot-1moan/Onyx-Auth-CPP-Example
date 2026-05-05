# Onyx Gate â€” C++ Example

> Official C++ SDK and loader example for [Onyx Gate Auth](https://auth.script-kittens.com) â€” the authentication platform built for cheat developers.

![C++](https://img.shields.io/badge/C++-17-f34b7d?style=flat&logo=cplusplus&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Windows-0078d4?style=flat&logo=windows&logoColor=white)
![Dependencies](https://img.shields.io/badge/Dependencies-None-28a745?style=flat)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat)

---

## What is Onyx Gate?

Onyx Gate is a KeyAuth-style authentication system built by Script Kittens. It gives your cheat or tool:

- **HWID Lock** â€” bind each user to one machine
- **License Keys** â€” generate, sell, and track keys from the dashboard
- **Live Sessions** â€” see who's online right now, kick them instantly
- **Blacklist** â€” ban HWIDs, IPs, or usernames with one click
- **Variables** â€” push values to your app at runtime without recompiling
- **Plan Gating** â€” free vs paid feature separation built in

---

## Files

| File | Purpose |
|---|---|
| `SKAuth.h` | Core SDK â€” drop into **any** C++ project |
| `skStr.h` | Compile-time XOR string obfuscation â€” hides App ID from scanners |
| `storage.hpp` | Save / load credentials for auto-login |
| `imgui_auth.h` | Drop-in ImGui login window for internal cheats |
| `loader.cpp` | Full console loader example |
| `OnyxGate-CPP.sln` | Visual Studio solution â€” open this |

---

## Requirements

- Visual Studio 2022
- Windows 10 SDK (included with VS)
- C++17 or later
- **No NuGet packages. No vcpkg. No external libraries.**

WinHTTP is built into Windows â€” that's all we use.

---

## Quick Start

**1. Open the solution**
```
OnyxGate-CPP.sln â†’ Visual Studio 2022
```

**2. Set your App ID** â€” open `loader.cpp` and change line 10:
```cpp
#define APP_ID_RAW "YOUR_APP_ID_HERE"
```
Get your App ID from [auth.script-kittens.com](https://auth.script-kittens.com) â†’ Manage Apps â†’ Credentials.

**3. Build**
```
Build â†’ Release | x64 â†’ Build Solution
```

**4. Run** â€” the loader opens with Login / Register / Upgrade options.

---

## Integrate into your existing ImGui cheat

Copy `SKAuth.h`, `skStr.h`, and `imgui_auth.h` into your project, then add **3 lines**:

```cpp
// 1. In DllMain or your init thread:
AuthSystem::Init("YOUR_APP_ID");

// 2. In your render function:
AuthSystem::Render();                    // draws login window
if (AuthSystem::IsLoggedIn()) {
    YourCheatMenu();                     // only shown after auth
}

// 3. Gate paid features:
if (AuthSystem::IsPaid()) {
    // aimbot, esp, etc.
}
```

Add `winhttp.lib` to your linker dependencies and you're done.

---

## SKAuth.h â€” API Reference

```cpp
SKAuth auth("YOUR_APP_ID", "1.0");

// Login
auto r = auth.Login("username", "password");
if (r.ok) { /* auth.username, auth.plan, auth.expires */ }

// Register (new customer â€” needs license key)
auto r = auth.Register("username", "password", "email", "SK-XXXX-XXXX-XXXX-XXXX");

// Upgrade (redeem key on existing account)
auto r = auth.Redeem("username", "SK-XXXX-XXXX-XXXX-XXXX");

// Server-side variable (set in dashboard, read at runtime)
std::string val = auth.GetVar("variable_name");

// Plan check
if (auth.IsPaid()) { /* paid/vip/lifetime */ }
```

---

## Dashboard

Manage your users, keys, sessions, and blacklist at:
**[auth.script-kittens.com](https://auth.script-kittens.com)**

Buy keys or upgrade plans on our Discord:
**[discord.gg/tWwUSPh5GT](https://discord.gg/tWwUSPh5GT)**

---

## Other SDKs

| Language | Repo |
|---|---|
| C# WinForms | [Onyx-Auth-CSharp-Example](https://github.com/1shot-1moan/Onyx-Auth-CSharp-Example) |
| Python | [Onyx-Auth-Python-Example](https://github.com/1shot-1moan/Onyx-Auth-Python-Example) |
