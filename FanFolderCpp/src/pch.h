#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <array>
#include <optional>
#include <fstream>

#define NTDDI_VERSION NTDDI_WIN10
#define _WIN32_WINNT  _WIN32_WINNT_WIN10
#define WINVER        0x0A00
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <combaseapi.h>
#include <oleidl.h>
#include <objidl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <commoncontrols.h>

// GDI+ needs min/max - pull from std:: (algorithm included above)
namespace Gdiplus {
    using std::min;
    using std::max;
}
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")
#include <mmsystem.h>
