// Custom DwmSetWindowAttribute_Hook for WinDock.
//
// Replaces the upstream taskbar-start-button-position.wh.cpp hook with a
// unified per-monitor popup-placement design that handles Start Menu,
// Search, and Notification Center across multi-monitor + mixed-DPI setups.
// See README of the fork for context.
//
// This file is injected by dependencies/main.py into the regenerated
// b_taskbar-start-button-position.wh.cpp, replacing upstream's smaller
// hook. Keeping the customization here (not in modified-dependencies/)
// means a clean `python assemble-mod.py` run pulls in latest upstream
// and re-injects our logic on top.
using DwmSetWindowAttribute_t = decltype(&DwmSetWindowAttribute);
DwmSetWindowAttribute_t DwmSetWindowAttribute_Original;
HRESULT WINAPI DwmSetWindowAttribute_Hook(HWND hwnd,
                                          DWORD dwAttribute,
                                          LPCVOID pvAttribute,
                                          DWORD cbAttribute) {
    auto original = [=]() {
        return DwmSetWindowAttribute_Original(hwnd, dwAttribute, pvAttribute,
                                              cbAttribute);
    };
    if (dwAttribute != DWMWA_CLOAK || cbAttribute != sizeof(BOOL)) {
        return original();
    }
    BOOL cloak = *(BOOL*)pvAttribute;
    if (cloak) {
        return original();
    }
    Wh_Log(L"> %08X", (DWORD)(DWORD_PTR)hwnd);
    DWORD processId = 0;
    if (!hwnd || !GetWindowThreadProcessId(hwnd, &processId)) {
        return original();
    }
    TCHAR className[256];GetClassName(hwnd, className, 256);std::wstring windowClassName(className);
std::wstring processFileName = GetProcessFileName(processId);
Wh_Log(L"process: %s, windowClassName: %s",processFileName.c_str(),windowClassName.c_str());
    // ===== DIAGNOSTIC: log EVERY DwmSetWindowAttribute_Hook entry =====
    // Before any filtering, so we can prove the hook is even being called
    // for unfamiliar popups (language switcher, IME flyout, etc).
    {
        POINT diagCursor0 = {-1, -1};
        GetCursorPos(&diagCursor0);
        RECT diagRect0 = {};
        GetWindowRect(hwnd, &diagRect0);
        FILE* f = nullptr;
        WCHAR logPath[MAX_PATH];
        if (GetEnvironmentVariableW(L"TEMP", logPath, MAX_PATH)) {
            wcscat_s(logPath, MAX_PATH, L"\\windhawk_popup_log.txt");
            _wfopen_s(&f, logPath, L"a, ccs=UTF-8");
            if (f) {
                SYSTEMTIME st;
                GetLocalTime(&st);
                fwprintf(f,
                         L"%02d:%02d:%02d.%03d HOOK_ENTRY proc=%s class=%s "
                         L"hwnd=%p rect=(%ld,%ld,cx=%ld,cy=%ld) cursor=(%ld,%ld)\n",
                         st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                         processFileName.c_str(),
                         windowClassName.c_str(),
                         hwnd,
                         diagRect0.left, diagRect0.top,
                         diagRect0.right - diagRect0.left,
                         diagRect0.bottom - diagRect0.top,
                         diagCursor0.x, diagCursor0.y);
                fclose(f);
            }
        }
    }
    enum class Target {
        StartMenu,
        SearchHost,ShellExperienceHost,
    };
    Target target;
    if (_wcsicmp(processFileName.c_str(), L"StartMenuExperienceHost.exe") ==
        0) {
        target = Target::StartMenu;
    } else if (_wcsicmp(processFileName.c_str(), L"SearchHost.exe") == 0) {
        target = Target::SearchHost;
    }else if (_wcsicmp(processFileName.c_str(), L"ShellExperienceHost.exe") == 0) {
        target = Target::ShellExperienceHost;
    }  else {
        // ===== DIAGNOSTIC: log unrecognized popups too =====
        // Helps identify which process owns popups we don't currently
        // re-position (language switcher, IME flyout, etc.). One line per
        // popup-open into %TEMP%\windhawk_popup_log.txt with the process
        // name, window class, initial rect, and cursor position. Safe to
        // remove once we've identified everything we want to handle.
        {
            POINT diagCursor = {-1, -1};
            GetCursorPos(&diagCursor);
            RECT diagRect = {};
            GetWindowRect(hwnd, &diagRect);
            FILE* f = nullptr;
            WCHAR logPath[MAX_PATH];
            if (GetEnvironmentVariableW(L"TEMP", logPath, MAX_PATH)) {
                wcscat_s(logPath, MAX_PATH, L"\\windhawk_popup_log.txt");
                _wfopen_s(&f, logPath, L"a, ccs=UTF-8");
                if (f) {
                    SYSTEMTIME st;
                    GetLocalTime(&st);
                    fwprintf(f,
                             L"%02d:%02d:%02d.%03d UNHANDLED proc=%s class=%s "
                             L"hwnd=%p rect=(%ld,%ld,cx=%ld,cy=%ld) cursor=(%ld,%ld)\n",
                             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                             processFileName.c_str(),
                             windowClassName.c_str(),
                             hwnd,
                             diagRect.left, diagRect.top,
                             diagRect.right - diagRect.left,
                             diagRect.bottom - diagRect.top,
                             diagCursor.x, diagCursor.y);
                    fclose(f);
                }
            }
        }
        return original();
    }
    // Determine which monitor the popup belongs to. MonitorFromWindow returns
    // the popup's *current* monitor, but Windows often places the popup on the
    // primary monitor before our hook fires, even when the user clicked on a
    // secondary taskbar. The cursor position at the moment of the click is a
    // much better proxy for "which taskbar invoked this".
    HMONITOR monitor = nullptr;
    POINT cursorPos;
    if (GetCursorPos(&cursorPos)) {
        monitor = MonitorFromPoint(cursorPos, MONITOR_DEFAULTTONEAREST);
    }
    if (!monitor) {
        monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    }
    UINT monitorDpiX = 96;
    UINT monitorDpiY = 96;
    GetDpiForMonitor(monitor, MDT_DEFAULT, &monitorDpiX, &monitorDpiY);
    MONITORINFO monitorInfo{
        .cbSize = sizeof(MONITORINFO),
    };
    GetMonitorInfo(monitor, &monitorInfo);
    auto monitorName = GetMonitorName(monitor);
    auto iterationTbStates = g_taskbarStates.find(monitorName);
    if (iterationTbStates == g_taskbarStates.end()) {
      return original();
    }
    TaskbarState& taskbarState = iterationTbStates->second;
    RECT targetRect;
    if (!GetWindowRect(hwnd, &targetRect)) {
        Wh_Log(L"[POPUP-DBG] GetWindowRect failed for hwnd=%p", hwnd);
        return original();
    }
    int x = targetRect.left;
    int y = targetRect.top;
    int cx = targetRect.right - targetRect.left;
    int cy = targetRect.bottom - targetRect.top;
    // ====================================================================
    // === Unified popup placement (refactored, single code path) =========
    // ====================================================================
    // Replaces the previous per-target patched logic with a single design:
    //
    //   * All math in DIPs in monitor-local coordinates, converted to
    //     virtual-screen physical pixels once at the very end.
    //   * No state read from the popup hwnd (cx/cy/x/y from GetWindowRect
    //     are unreliable across monitors and across mod reloads — Windows
    //     reports stale physical-pixel sizes from previous SetWindowPos
    //     calls, which is what poisoned every previous attempt).
    //   * Per-target placement strategy is a static table, not branching
    //     formulas that drift apart over time.
    //
    //   Target              X                          Y                       Size
    //   ----------------- + -------------------------- + ---------------------- + --------------------------
    //   StartMenu           monitor.left               monitor.top              keep Windows' cx/cy (Win11
    //                                                                           sizes Start menu to the
    //                                                                           destination monitor itself)
    //   SearchHost          centered under taskbar     bottom just above the    cached natural DIPs scaled
    //                                                  visible taskbar          to dest monitor's DPI
    //   ShellExperienceHost right-aligned to taskbar   monitor.top              keep Windows' cx/cy (NC
    //                       right edge                                          is a full-height sidebar)

    // Skip everything if mod is unloading or the relevant setting is off —
    // let Windows handle positioning natively in those cases.
    HMONITOR primaryMonitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    bool moveStartSetting = g_settings_startbuttonposition.startMenuOnTheLeft;
    bool moveNCSetting    = g_settings_startbuttonposition.MoveFlyoutNotificationCenter;
    // Notification-Center-specific scope: when the user opts into "primary
    // monitor only" via NotificationCenterPrimaryOnly, NC popups on a
    // secondary monitor fall through to original() and Windows places them
    // at the native default position there. Start Menu / Search are
    // intentionally NOT affected by this gate.
    bool onPrimary         = (monitor == primaryMonitor);
    bool ncPrimaryOnlyMode = g_settings.userDefinedNotificationCenterPrimaryOnly;
    bool ncWantPlace       = moveNCSetting && (!ncPrimaryOnlyMode || onPrimary);
    bool wantPlace =
        !g_unloading &&
        ((target == Target::StartMenu          && moveStartSetting) ||
         (target == Target::SearchHost         && moveStartSetting) ||
         (target == Target::ShellExperienceHost && ncWantPlace));

    if (wantPlace) {
        // Cache the Search popup's natural size in DIPs. Windows sizes popups
        // intrinsically for the primary monitor's DPI; on any other monitor
        // the reported physical size is whatever someone last set, which is
        // not a reliable source of truth (Windows migrates the hwnd to a new
        // monitor without resizing it). So we only snapshot the cache when:
        //   - target is Search (other popups don't need DIP-based sizing), AND
        //   - the popup hwnd is currently on the primary monitor, AND
        //   - our hook fired for the primary monitor (cursor was there), AND
        //   - the observed size in primary-DPI DIPs is in the natural range.
        // This guarantees we never adopt a stale cross-monitor size as the
        // "natural" baseline.
        static int g_naturalSearchCxDIPs = 0;
        static int g_naturalSearchCyDIPs = 0;
        {
            const int kDefaultSearchCxDIPs = 858;  // Win11 24H2 fallback
            const int kDefaultSearchCyDIPs = 890;
            const int kMinNaturalDIPs      = 700;
            const int kMaxNaturalDIPs      = 950;
            HMONITOR popupMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            // primaryMonitor was already computed above for the wantPlace gate.
            if (target == Target::SearchHost &&
                popupMon == primaryMonitor &&
                monitor == primaryMonitor) {
                UINT primaryDpiX = 96, primaryDpiY = 96;
                GetDpiForMonitor(primaryMonitor, MDT_DEFAULT, &primaryDpiX, &primaryDpiY);
                if (primaryDpiX > 0) {
                    int observedCxDIPs = MulDiv(cx, 96, primaryDpiX);
                    if (observedCxDIPs >= kMinNaturalDIPs && observedCxDIPs <= kMaxNaturalDIPs) {
                        g_naturalSearchCxDIPs = observedCxDIPs;
                        g_naturalSearchCyDIPs = MulDiv(cy, 96, primaryDpiY);
                    }
                }
            }
            if (g_naturalSearchCxDIPs == 0) {
                g_naturalSearchCxDIPs = kDefaultSearchCxDIPs;
                g_naturalSearchCyDIPs = kDefaultSearchCyDIPs;
            }
        }

        // Taskbar geometry in DIPs (from taskbarState, populated by ApplyStyle
        // in explorer.exe; stabilized via floor() so no float drift).
        int tbarLeftDIPs   = (int)taskbarState.lastStartButtonXCalculated;
        int tbarWidthDIPs  = (int)taskbarState.lastTargetWidth;
        int tbarCenterDIPs = tbarLeftDIPs + tbarWidthDIPs / 2;
        int tbarRightDIPs  = tbarLeftDIPs + tbarWidthDIPs;

        // Visual taskbar height in DIPs (mod setting; includes Y offset).
        int tbarVisualHDIPs =
            (int)g_settings.userDefinedTaskbarHeight +
            std::abs((int)g_settings.userDefinedTaskbarOffsetY);

        int monW = monitorInfo.rcMonitor.right  - monitorInfo.rcMonitor.left;
        int monH = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

        if (target == Target::StartMenu) {
            // Start menu: monitor-sized hwnd at destination's top-left.
            // Win11 manages visible content inside via XAML Canvas, and
            // it already resizes the hwnd to match destination monitor
            // before our hook fires — so we keep cx/cy as-is.
            x = monitorInfo.rcMonitor.left;
            y = monitorInfo.rcMonitor.top;
        } else if (target == Target::SearchHost) {
            // Derive popup size from cached natural DIPs (idempotent across
            // any number of re-launches; never compounds).
            cx = MulDiv(g_naturalSearchCxDIPs, monitorDpiX, 96);
            cy = MulDiv(g_naturalSearchCyDIPs, monitorDpiY, 96);
            // X: centered under visible taskbar.
            int xLocal = MulDiv(tbarCenterDIPs, monitorDpiX, 96) - cx / 2;
            // Y: popup bottom = monitor bottom - visible-taskbar height - margin.
            int tbarVisualHPx = MulDiv(tbarVisualHDIPs, monitorDpiY, 96);
            int marginPx      = MulDiv(6, monitorDpiY, 96);
            int yLocal = monH - tbarVisualHPx - marginPx - cy;
            // Clamp inside monitor with 10 px breathing room.
            if (xLocal < 10) xLocal = 10;
            if (xLocal + cx > monW - 10) xLocal = monW - cx - 10;
            if (yLocal < 10) yLocal = 10;
            x = monitorInfo.rcMonitor.left + xLocal;
            y = monitorInfo.rcMonitor.top  + yLocal;
            g_searchMenuWnd = hwnd;
            g_searchMenuOriginalX = targetRect.left;
        } else /* Target::ShellExperienceHost */ {
            // ShellExperienceHost.exe owns BOTH the Notification Center
            // (full-monitor-height sidebar on the right) AND toast
            // notifications (small popups at bottom-right). They come
            // through this same hook. We discriminate by height:
            //   Notification Center: cy ≈ monitor height (1500-2100 px)
            //   Toast notification:  cy ≈ 100-300 px (well under half)
            // For toasts, Windows already places them correctly at the
            // bottom-right corner — we must NOT move them. Returning
            // original() skips both our SetWindowPos and the file log.
            if (cy < monH / 2) {
                return original();  // toast — leave Windows' positioning alone
            }
            // Notification Center: right-aligned with taskbar, top-anchored.
            int xLocal = MulDiv(tbarRightDIPs, monitorDpiX, 96) - cx;
            if (xLocal < 10) xLocal = 10;
            if (xLocal + cx > monW - 10) xLocal = monW - cx - 10;
            x = monitorInfo.rcMonitor.left + xLocal;
            y = monitorInfo.rcMonitor.top;  // top-anchor; cy already monitor-sized
        }
    }

    // === Final safety clamp =============================================
    // Guarantee popup is at least mostly visible on the destination monitor,
    // regardless of how x/y were computed. Catches any future bug that
    // would push the popup off-screen.
    {
        int safeMinX = monitorInfo.rcMonitor.left   - cx + 100;
        int safeMaxX = monitorInfo.rcMonitor.right  - 100;
        int safeMinY = monitorInfo.rcMonitor.top    - cy + 100;
        int safeMaxY = monitorInfo.rcMonitor.bottom - 100;
        if (x < safeMinX) x = safeMinX;
        if (x > safeMaxX) x = safeMaxX;
        if (y < safeMinY) y = safeMinY;
        if (y > safeMaxY) y = safeMaxY;
    }
    // === DIAGNOSTIC LOG (popup positioning) ===
    // Also write to %TEMP%\windhawk_popup_log.txt so the user can read logs
    // without needing DebugView. Each popup open appends one line.
    Wh_Log(L"[POPUP-DBG] target=%d proc=%s monitor=%s mDPI=%u rcMon=(%ld,%ld)-(%ld,%ld) rcWork=(%ld,%ld)-(%ld,%ld) cursor=(%ld,%ld) winRect.in=(%ld,%ld,cx=%d,cy=%d) finalSetPos=(x=%d,y=%d,cx=%d,cy=%d) tbState{startBtnX=%.2f rootW=%.2f targetW=%.2f}",
           (int)target,
           processFileName.c_str(),
           monitorName.c_str(),
           monitorDpiX,
           monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
           monitorInfo.rcMonitor.right, monitorInfo.rcMonitor.bottom,
           monitorInfo.rcWork.left, monitorInfo.rcWork.top,
           monitorInfo.rcWork.right, monitorInfo.rcWork.bottom,
           cursorPos.x, cursorPos.y,
           targetRect.left, targetRect.top,
           targetRect.right - targetRect.left, targetRect.bottom - targetRect.top,
           x, y, cx, cy,
           taskbarState.lastStartButtonXCalculated,
           taskbarState.lastRootWidth,
           taskbarState.lastTargetWidth);
    {
      FILE* f = nullptr;
      WCHAR logPath[MAX_PATH];
      if (GetEnvironmentVariableW(L"TEMP", logPath, MAX_PATH)) {
        wcscat_s(logPath, MAX_PATH, L"\\windhawk_popup_log.txt");
        _wfopen_s(&f, logPath, L"a, ccs=UTF-8");
        if (f) {
          SYSTEMTIME st;
          GetLocalTime(&st);
          fwprintf(f, L"%02d:%02d:%02d.%03d target=%d proc=%s monitor=%s mDPI=%u rcMon=(%ld,%ld)-(%ld,%ld) rcWork=(%ld,%ld)-(%ld,%ld) cursor=(%ld,%ld) winRect.in=(%ld,%ld,cx=%d,cy=%d) finalSetPos=(x=%d,y=%d,cx=%d,cy=%d) tbState{startBtnX=%.2f rootW=%.2f targetW=%.2f} popupMon%s\n",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            (int)target,
            processFileName.c_str(),
            monitorName.c_str(),
            monitorDpiX,
            monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right, monitorInfo.rcMonitor.bottom,
            monitorInfo.rcWork.left, monitorInfo.rcWork.top,
            monitorInfo.rcWork.right, monitorInfo.rcWork.bottom,
            cursorPos.x, cursorPos.y,
            targetRect.left, targetRect.top,
            targetRect.right - targetRect.left, targetRect.bottom - targetRect.top,
            x, y, cx, cy,
            taskbarState.lastStartButtonXCalculated,
            taskbarState.lastRootWidth,
            taskbarState.lastTargetWidth,
            (MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) == monitor) ? L"=cursor" : L"!=cursor");
          fclose(f);
        }
      }
    }
    SetWindowPos(hwnd, nullptr, x, y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);
    return original();
}
