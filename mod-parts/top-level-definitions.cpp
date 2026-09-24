#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <memory>
#include <mutex>
#include <vector>

struct TaskbarChildStyleCache {
  uint64_t generation{0};
  uint64_t signature{0};
  bool valid{false};
};

struct TaskbarState {
  std::recursive_mutex mutex;
  std::chrono::steady_clock::time_point lastApplyStyleTime{};
  struct Data {
    int childrenCount;
    int rightMostEdge;
    unsigned int childrenWidth;
  } lastTaskbarData{};
  unsigned int lastChildrenWidthTaskbar{0};
  unsigned int lastTrayFrameWidth{0};
  float lastTargetWidth{0};
  float lastTargetOffsetX{0};
  float lastTargetOffsetY{0};
  float initOffsetX{-1};
  bool wasOverflowing{false};
  // Fork addition: false once ApplyStyle has clipped the taskbar window to the
  // island (UpdateTaskbarWindowRegion), so turning the clip off clears it once.
  // lastRegionCorner is the clip's corner radius in DIPs, which the window's
  // region bounds cannot show a change of.
  bool lastRegionClear{true};
  float lastRegionCorner{-1.0f};
  uintptr_t lastOverflowButtonIdentity{0};
  bool overflowButtonSuppressionKnown{false};
  bool overflowButtonSuppressed{false};
  float lastStartButtonXCalculated=0.0f;
  float lastStartButtonXActual=0.0f;
  float lastStartButtonAnchorLeft{0.0f};
  float lastStartButtonAnchorTop{0.0f};
  float lastStartButtonAnchorWidth{0.0f};
  float lastStartButtonAnchorHeight{0.0f};
  bool hasLastStartButtonAnchorRect{false};
  float stableStartButtonAnchorLeft{0.0f};
  float stableStartButtonAnchorTop{0.0f};
  float stableStartButtonAnchorWidth{0.0f};
  float stableStartButtonAnchorHeight{0.0f};
  bool hasStableStartButtonAnchorRect{false};
  int startButtonAnchorStablePasses{0};
  float lastRootWidth=0.0f;
  float lastTargetTaskFrameOffsetX=0.0f;
  bool hasLastTargetTaskFrameOffsetX{false};
  float lastTargetTaskbarIslandScale{1.0f};
  float lastTaskbarIslandScaleCenterX{0.0f};
  bool hasLastTargetTaskbarIslandScale{false};
  float lastObservedRootWidth{0.0f};
  float lastObservedRootHeight{0.0f};
  float lastObservedRasterizationScale{0.0f};
  float lastObservedTaskFrameWidth{0.0f};
  float lastObservedTaskFrameHeight{0.0f};
  bool hasLastDisplayGeometrySignature{false};
  float lastTargetTrayOffsetX{0.0f};
  bool hasLastTargetTrayOffsetX{false};
  float lastTargetWidgetOffsetX{0.0f};
  float lastTargetWidgetOffsetY{0.0f};
  bool hasLastTargetWidgetOffset{false};
  float lastLeftMostEdgeTray{0};
  int lastRightMostEdgeTray{0};
  float lastBackgroundShapeTargetWidth{0.0f};
  float lastBackgroundShapeTargetHeight{0.0f};
  float lastBackgroundShapeTargetOffsetX{0.0f};
  float lastBackgroundShapeTargetOffsetY{0.0f};
  float backgroundAnimationFromWidth{0.0f};
  float backgroundAnimationToWidth{0.0f};
  float backgroundAnimationFromOffsetX{0.0f};
  float backgroundAnimationToOffsetX{0.0f};
  float backgroundAnimationFromOffsetY{0.0f};
  float backgroundAnimationToOffsetY{0.0f};
  int64_t backgroundAnimationStartMs{0};
  uintptr_t backgroundFillIdentity{0};
  uint64_t lastBackgroundStyleGeneration{0};
  bool hasCustomTaskbarBackgroundVisuals{false};
  uint64_t lastDimensionInvalidationGeneration{0};
  TaskbarChildStyleCache taskbarChildStyleCache;
  TaskbarChildStyleCache trayChildStyleCache;
};

struct TaskbarFlyoutStateSnapshot {
  float lastStartButtonXCalculated{0.0f};
  float lastRootWidth{0.0f};
  float lastTargetWidth{0.0f};
  float lastLeftMostEdgeTray{0.0f};
  int lastRightMostEdgeTray{0};
};

static std::mutex g_taskbarStatesMutex;
static std::unordered_map<std::wstring, std::shared_ptr<TaskbarState>> g_taskbarStates;
static std::atomic<uint64_t> g_dimensionInvalidationGeneration{1};
static std::atomic<uint64_t> g_taskbarChildStyleGeneration{1};
static std::atomic<uintptr_t> g_recentTaskbarInvocationMonitor{0};
static std::atomic<ULONGLONG> g_recentTaskbarInvocationTime{0};

bool IsTaskbarWindowClassTai(HWND window) {
  if (!window) {
    return false;
  }

  wchar_t className[64]{};
  if (!GetClassNameW(window, className, ARRAYSIZE(className))) {
    return false;
  }

  return _wcsicmp(className, L"Shell_TrayWnd") == 0 ||
         _wcsicmp(className, L"Shell_SecondaryTrayWnd") == 0;
}

// Fork addition: the monitor a taskbar belongs to. Explorer records it on every
// taskbar window as the "TaskbarMonitor" property, which ApplySettingsTBIconSize
// already reads for the DPI. MonitorFromWindow is no substitute: an auto-hidden
// taskbar is parked almost entirely off its monitor, so with another monitor
// below it (a laptop under an external screen, upstream issue #32) it names that
// neighbour, and two taskbars get styled, and clipped, as one. The property can
// outlive its monitor during a display change, hence the fallback.
HMONITOR GetTaskbarMonitorTai(HWND taskbarWindow) {
  if (!taskbarWindow) {
    return nullptr;
  }
  HMONITOR monitor =
      reinterpret_cast<HMONITOR>(GetPropW(taskbarWindow, L"TaskbarMonitor"));
  MONITORINFO monitorInfo{.cbSize = sizeof(MONITORINFO)};
  if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
    return monitor;
  }
  return MonitorFromWindow(taskbarWindow, MONITOR_DEFAULTTONEAREST);
}

// Fork addition: the taskbar window ApplyStyle is styling, set around the call
// by ApplySettingsFromTaskbarThread. ApplyStyle is only handed the taskbar's
// XAML, and UpdateTaskbarWindowRegion needs the window it belongs to.
thread_local HWND g_applyStyleTaskbarWindowTai = nullptr;

HMONITOR GetTaskbarMonitorFromPointTai(POINT point) {
  for (HWND window = WindowFromPoint(point); window;
       window = GetParent(window)) {
    if (IsTaskbarWindowClassTai(window)) {
      return GetTaskbarMonitorTai(window);
    }
  }

  struct EnumContext {
    POINT point;
    HMONITOR monitor;
  } context{point, nullptr};

  EnumWindows(
      [](HWND window, LPARAM lParam) -> BOOL {
        auto* context = reinterpret_cast<EnumContext*>(lParam);
        if (!context || !IsTaskbarWindowClassTai(window)) {
          return TRUE;
        }

        RECT rect{};
        if (GetWindowRect(window, &rect) &&
            PtInRect(&rect, context->point)) {
          context->monitor = GetTaskbarMonitorTai(window);
          return FALSE;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&context));

  return context.monitor;
}

bool IsTaskbarInvocationInputMessageTai(UINT message) {
  switch (message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_POINTERDOWN:
    case WM_POINTERUP:
    case WM_TOUCH:
      return true;
    default:
      return false;
  }
}

POINT GetCurrentMessagePointTai() {
  const DWORD messagePosition = GetMessagePos();
  return {
      static_cast<short>(LOWORD(messagePosition)),
      static_cast<short>(HIWORD(messagePosition)),
  };
}

void RecordTaskbarInvocationMonitorTai(HWND taskbarWindow, UINT message) {
  if (!IsTaskbarInvocationInputMessageTai(message)) {
    return;
  }

  HMONITOR monitor =
      GetTaskbarMonitorFromPointTai(GetCurrentMessagePointTai());
  if (!monitor && taskbarWindow) {
    monitor = GetTaskbarMonitorTai(taskbarWindow);
  }
  if (!monitor) {
    return;
  }

  g_recentTaskbarInvocationMonitor.store(
      reinterpret_cast<uintptr_t>(monitor), std::memory_order_release);
  g_recentTaskbarInvocationTime.store(GetTickCount64(),
                                      std::memory_order_release);
}

HMONITOR ResolveFlyoutMonitorTai(HWND flyoutWindow) {
  constexpr DWORD kInvocationMessageTtlMs = 2500;
  const DWORD messageTime = static_cast<DWORD>(GetMessageTime());
  if (messageTime &&
      GetTickCount() - messageTime <= kInvocationMessageTtlMs) {
    if (HMONITOR monitor =
            GetTaskbarMonitorFromPointTai(GetCurrentMessagePointTai())) {
      return monitor;
    }
  }

  constexpr ULONGLONG kInvocationMonitorTtlMs = 2500;
  const ULONGLONG invocationTime =
      g_recentTaskbarInvocationTime.load(std::memory_order_acquire);
  const ULONGLONG now = GetTickCount64();
  if (invocationTime && now >= invocationTime &&
      now - invocationTime <= kInvocationMonitorTtlMs) {
    HMONITOR monitor = reinterpret_cast<HMONITOR>(
        g_recentTaskbarInvocationMonitor.load(std::memory_order_acquire));
    MONITORINFO monitorInfo{.cbSize = sizeof(MONITORINFO)};
    if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
      return monitor;
    }
  }

  return flyoutWindow
             ? MonitorFromWindow(flyoutWindow, MONITOR_DEFAULTTONEAREST)
             : nullptr;
}

std::shared_ptr<TaskbarState> GetOrCreateTaskbarState(const std::wstring& monitorName) {
  std::lock_guard<std::mutex> lock(g_taskbarStatesMutex);
  auto& state = g_taskbarStates[monitorName];
  if (!state) {
    state = std::make_shared<TaskbarState>();
  }
  return state;
}

std::vector<std::shared_ptr<TaskbarState>> GetTaskbarStatesSnapshot() {
  std::lock_guard<std::mutex> lock(g_taskbarStatesMutex);
  std::vector<std::shared_ptr<TaskbarState>> states;
  states.reserve(g_taskbarStates.size());
  for (const auto& [monitorName, state] : g_taskbarStates) {
    if (state) {
      states.push_back(state);
    }
  }
  return states;
}

bool TryGetTaskbarFlyoutStateSnapshot(
    const std::wstring& monitorName,
    TaskbarFlyoutStateSnapshot* snapshot) {
  if (!snapshot) {
    return false;
  }

  std::shared_ptr<TaskbarState> state;
  {
    std::lock_guard<std::mutex> lock(g_taskbarStatesMutex);
    auto it = g_taskbarStates.find(monitorName);
    if (it == g_taskbarStates.end() || !it->second) {
      return false;
    }
    state = it->second;
  }

  std::lock_guard<std::recursive_mutex> lock(state->mutex);
  snapshot->lastStartButtonXCalculated = state->lastStartButtonXCalculated;
  snapshot->lastRootWidth = state->lastRootWidth;
  snapshot->lastTargetWidth = state->lastTargetWidth;
  snapshot->lastLeftMostEdgeTray = state->lastLeftMostEdgeTray;
  snapshot->lastRightMostEdgeTray = state->lastRightMostEdgeTray;
  return true;
}

void ClearTaskbarStates() {
  std::lock_guard<std::mutex> lock(g_taskbarStatesMutex);
  g_taskbarStates.clear();
}

void RequestTaskbarDimensionInvalidation() {
  g_dimensionInvalidationGeneration.fetch_add(1, std::memory_order_acq_rel);
}

void RequestTaskbarChildStyleRefresh() {
  g_taskbarChildStyleGeneration.fetch_add(1, std::memory_order_acq_rel);
}


void ApplySettingsDebounced(int delayMs);
void ApplySettingsDebounced();
void ApplySettingsFromTaskbarThreadIfRequired();
void ApplySettingsFromTaskbarThreadImmediately();
void ApplySettingsFromTaskbarThreadGeometryChanged();
extern std::atomic<int> g_high_priority_dispatch_passes;
void RequestTaskbarButtonSizeRelayout();
void ArmInitialExplorerStyleApplyDelay();
void ScheduleInitialExplorerStyleApply();
int g_lastRecordedStartMenuWidth=0;
std::atomic<bool> g_already_requested_debounce_initializing = false;
STDAPI GetDpiForMonitor(HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, UINT* dpiX, UINT* dpiY);

#include <Windows.h>
bool IsStartMenuOrbLeftAligned() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                     LR"(Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced)",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"TaskbarAl", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return value == 0;
        }
        RegCloseKey(hKey);
    }
    return false;
}


int GetFlyoutTaskbarBottomGapPx(float dpiScaleY) {
    std::lock_guard<std::recursive_mutex> lock(g_settingsMutex);
    if (g_unloading || g_settings.userDefinedFlatTaskbarBottomCorners ||
        g_settings.userDefinedFullWidthTaskbarBackground) {
        return 0;
    }
    int offsetY = static_cast<int>(g_settings.userDefinedTaskbarOffsetY);
    if (offsetY >= 0) {
        return 0;
    }
    return static_cast<int>((-offsetY * dpiScaleY) + 0.5f);
}

int GetFlyoutTaskbarHeightPx(float dpiScaleY) {
    std::lock_guard<std::recursive_mutex> lock(g_settingsMutex);
    int taskbarHeight = static_cast<int>(g_settings.userDefinedTaskbarHeight);
    if (taskbarHeight <= 0) {
        taskbarHeight = g_taskbarHeight > 0 ? g_taskbarHeight : kSystemMediumTaskbarButtonSize;
    }
    return std::max(1, static_cast<int>((taskbarHeight * dpiScaleY) + 0.5f));
}
int GetFlyoutInnerPaddingPx(float dpiScale) {
    std::lock_guard<std::recursive_mutex> lock(g_settingsMutex);
    if (dpiScale <= 0.0f) {
        dpiScale = 1.0f;
    }
    constexpr int kMaxFlyoutInnerPaddingDip = 32;
    const float logicalPadding =
        std::min<float>(kMaxFlyoutInnerPaddingDip,
                        static_cast<float>(g_settings.userDefinedTaskbarBackgroundHorizontalPadding) +
                            (g_settings.userDefinedTaskbarCornerRadius * 0.5f));
    return std::max(0, static_cast<int>((logicalPadding * dpiScale) + 0.5f));
}
bool IsVerticalTaskbar();
bool TryCalculateFlyoutYAboveTaskbar(const MONITORINFO& monitorInfo,
                                     int flyoutHeight,
                                     float dpiScaleY,
                                     int& y) {
    if (flyoutHeight <= 0 || dpiScaleY <= 0.0f || IsVerticalTaskbar()) {
        return false;
    }
    const int monitorTop = monitorInfo.rcMonitor.top;
    const int monitorBottom = monitorInfo.rcMonitor.bottom;
    if (monitorBottom <= monitorTop) {
        return false;
    }
    const int taskbarHeightPx = GetFlyoutTaskbarHeightPx(dpiScaleY);
    const int taskbarBottomGapPx = GetFlyoutTaskbarBottomGapPx(dpiScaleY);
    const int taskbarTop = monitorBottom - taskbarHeightPx - taskbarBottomGapPx;
    if (taskbarTop <= monitorTop) {
        return false;
    }
    y = taskbarTop - flyoutHeight;
    if (y < monitorTop) {
        y = monitorTop;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Fork addition: mirror flyout placement decisions to a file, so multi-monitor
// and mixed-DPI issues can be diagnosed without DebugView attached. One line is
// appended per flyout open to %TEMP%\windhawk_popup_log.txt. Failures are
// silent by design -- diagnostics must never affect placement behaviour.
static FILE* OpenPopupLogFileTai() {
  WCHAR logPath[MAX_PATH];
  if (!GetEnvironmentVariableW(L"TEMP", logPath, MAX_PATH)) {
    return nullptr;
  }
  if (wcscat_s(logPath, MAX_PATH, L"\\windhawk_popup_log.txt") != 0) {
    return nullptr;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, logPath, L"a, ccs=UTF-8") != 0) {
    return nullptr;
  }
  return f;
}

void LogFlyoutPlacementToFileTai(PCWSTR stage,
                                 PCWSTR monitorName,
                                 int target,
                                 UINT monitorDpiX,
                                 UINT monitorDpiY,
                                 UINT windowDpiX,
                                 UINT windowDpiY,
                                 int x,
                                 int y,
                                 int cx,
                                 int cy,
                                 float lastStartButtonXCalculated,
                                 float lastRootWidth,
                                 float lastTargetWidth) {
  FILE* f = OpenPopupLogFileTai();
  if (!f) {
    return;
  }
  SYSTEMTIME st{};
  GetLocalTime(&st);
  POINT cursorPos{};
  GetCursorPos(&cursorPos);
  fwprintf(f,
           L"%02d:%02d:%02d.%03d %s monitor=%s target=%d "
           L"monitorDpi=%ux%u windowDpi=%ux%u "
           L"setPos=(x=%d,y=%d,cx=%d,cy=%d) cursor=(%ld,%ld) "
           L"tbState{startBtnX=%.2f rootW=%.2f targetW=%.2f}\n",
           st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, stage,
           monitorName, target, monitorDpiX, monitorDpiY, windowDpiX,
           windowDpiY, x, y, cx, cy, cursorPos.x, cursorPos.y,
           lastStartButtonXCalculated, lastRootWidth, lastTargetWidth);
  fclose(f);
}

// Fork addition: the keyboard layout (input switcher) flyout is placed by
// SetWindowPos_Hook rather than the DWM-cloak path above, so it gets its own
// line. anchor is "language" (the indicator) or "tray" (the island's tray, on
// taskbars without an indicator); originalX is where Windows put the flyout.
void LogInputSwitchPlacementToFileTai(PCWSTR monitorName,
                                      PCWSTR anchor,
                                      float anchorCenterXDip,
                                      UINT monitorDpi,
                                      int originalX,
                                      int x,
                                      int y,
                                      int cx,
                                      int cy) {
  FILE* f = OpenPopupLogFileTai();
  if (!f) {
    return;
  }
  SYSTEMTIME st{};
  GetLocalTime(&st);
  POINT cursorPos{};
  GetCursorPos(&cursorPos);
  fwprintf(f,
           L"%02d:%02d:%02d.%03d InputSwitch monitor=%s monitorDpi=%u "
           L"anchor=%s anchorX=%.2f originalX=%d "
           L"setPos=(x=%d,y=%d,cx=%d,cy=%d) cursor=(%ld,%ld)\n",
           st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, monitorName,
           monitorDpi, anchor, anchorCenterXDip, originalX, x, y, cx, cy,
           cursorPos.x, cursorPos.y);
  fclose(f);
}

// Fork addition: the Notification Center is also placed through Explorer's
// GetViewPosition, the rect Explorer pushes to the flyout on every open, so
// that gets its own line too. viewRect is the rect Windows computed; placedX is
// where the hook moved its left edge. trayRight is the island tray's right edge
// in monitor-relative DIPs.
void LogNotificationCenterPlacementToFileTai(PCWSTR monitorName,
                                             UINT monitorDpi,
                                             int trayRightDip,
                                             RECT const& viewRect,
                                             int placedX) {
  FILE* f = OpenPopupLogFileTai();
  if (!f) {
    return;
  }
  SYSTEMTIME st{};
  GetLocalTime(&st);
  POINT cursorPos{};
  GetCursorPos(&cursorPos);
  fwprintf(f,
           L"%02d:%02d:%02d.%03d NotificationCenter monitor=%s monitorDpi=%u "
           L"trayRight=%d viewRect=(x=%ld,y=%ld,cx=%ld,cy=%ld) placedX=%d "
           L"cursor=(%ld,%ld)\n",
           st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, monitorName,
           monitorDpi, trayRightDip, viewRect.left, viewRect.top,
           viewRect.right - viewRect.left, viewRect.bottom - viewRect.top,
           placedX, cursorPos.x, cursorPos.y);
  fclose(f);
}

// Fork addition: the taskbar's right-click menus. menu is TrayContextMenu or
// StartButtonContextMenu; edge is the island edge the menu is placed against
// (the tray's right, the start button's left). Stage "anchor" is the point the
// menu opens at: windows is Windows' point, placed the moved one. A tray menu
// then gets "moved" or "kept" once it has opened: windows is the top-left
// corner XAML gave it, placed where it ended up. All values are root-relative
// DIPs, which on a horizontal taskbar are monitor-relative.
void LogContextMenuPlacementToFileTai(PCWSTR menu,
                                      PCWSTR stage,
                                      PCWSTR monitorName,
                                      int edgeDip,
                                      float rootWidthDip,
                                      float marginDip,
                                      float menuWidthDip,
                                      float menuHeightDip,
                                      float windowsX,
                                      float windowsY,
                                      float placedX,
                                      float placedY) {
  FILE* f = OpenPopupLogFileTai();
  if (!f) {
    return;
  }
  SYSTEMTIME st{};
  GetLocalTime(&st);
  POINT cursorPos{};
  GetCursorPos(&cursorPos);
  fwprintf(f,
           L"%02d:%02d:%02d.%03d %s %s monitor=%s edge=%d rootW=%.2f "
           L"margin=%.2f menu=%.2fx%.2f windows=(%.2f,%.2f) "
           L"placed=(%.2f,%.2f) cursor=(%ld,%ld)\n",
           st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, menu, stage,
           monitorName, edgeDip, rootWidthDip, marginDip, menuWidthDip,
           menuHeightDip, windowsX, windowsY, placedX, placedY, cursorPos.x,
           cursorPos.y);
  fclose(f);
}

// Fork addition: things the mod did to a taskbar window as a whole rather than
// to a flyout: event names what happened, detail the specifics.
void LogTaskbarEventToFileTai(PCWSTR event, PCWSTR detail) {
  FILE* f = OpenPopupLogFileTai();
  if (!f) {
    return;
  }
  SYSTEMTIME st{};
  GetLocalTime(&st);
  fwprintf(f, L"%02d:%02d:%02d.%03d %s %s\n", st.wHour, st.wMinute,
           st.wSecond, st.wMilliseconds, event, detail);
  fclose(f);
}

// Fork addition: defined in win-dock-mod.cpp, called from the dependencies'
// HookSystemTraySymbols and HookTaskbarViewDllSymbolsStartButtonPosition.
bool HookTrayContextMenuPositionTai(HMODULE systemTrayModule);
bool HookStartButtonContextMenuPositionTai(HMODULE taskbarViewModule);
// Fork addition: defined in win-dock-mod.cpp, called from the dependencies'
// ApplySettingsFromTaskbarThread.
void RepairSecondaryTaskbarIslandTai(HWND taskbarWindow,
                                     std::wstring const& monitorName);
