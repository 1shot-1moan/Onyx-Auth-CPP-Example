# Onyx Gate — C++ Example

## Requirements
- Visual Studio 2022
- Windows 10 SDK (already included with VS)
- No NuGet packages, no vcpkg — works out of the box

## Setup (2 steps)
1. Open `OnyxGate-CPP.sln` in Visual Studio
2. Open `loader.cpp` — change `APP_ID` to your App ID from the dashboard
3. Build → Release | x64 → Run

## Files
| File | Purpose |
|------|---------|
| `SKAuth.h` | SDK — drop into any C++ project |
| `loader.cpp` | Console loader example |
| `imgui_auth.h` | ImGui auth window — drop into any ImGui cheat |
| `OnyxGate-CPP.sln` | Visual Studio solution |
| `OnyxGate-CPP.vcxproj` | Project file |

## Add to your ImGui cheat (3 lines)
```cpp
// 1. In DllMain or init:
AuthSystem::Init("YOUR_APP_ID");

// 2. In your render function:
AuthSystem::Render();
if (AuthSystem::IsLoggedIn()) { YourCheatMenu(); }

// 3. Gate paid features:
if (AuthSystem::IsPaid()) { /* paid only */ }
```
