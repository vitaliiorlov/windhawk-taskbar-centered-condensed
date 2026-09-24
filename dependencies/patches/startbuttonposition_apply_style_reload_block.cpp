// Fork addition: this runs inside an EnumThreadWindows callback, where an
// escaping exception takes Explorer down, and XAML calls throw while a taskbar
// is being torn down in a display change.
try {
const auto xamlRootContent = xamlRoot.Content().try_as<FrameworkElement>();
if (!xamlRootContent) {
    Wh_Log(L"XamlRoot content is null");
    return TRUE;
}

auto dispatcher = xamlRootContent.Dispatcher();
if (!dispatcher) {
    Wh_Log(L"XamlRoot content dispatcher is null");
    return TRUE;
}

// Fork addition: see GetTaskbarMonitorTai.
std::wstring monitorName = GetMonitorName(GetTaskbarMonitorTai(hWnd));
// Fork addition: in the middle of a display change Windows can report the
// "WinDisc" placeholder display, which is never drawn on. Styling a taskbar
// against it would only file its state under that name.
if (_wcsicmp(monitorName.c_str(), L"WinDisc") == 0) {
    Wh_Log(L"Skipping taskbar on the WinDisc placeholder display");
    return TRUE;
}
// Fork addition: see RepairSecondaryTaskbarIslandTai.
RepairSecondaryTaskbarIslandTai(hWnd, monitorName);
auto applyOnDispatcher = [xamlRootContent, monitorName, hWnd]() {
    // Fork addition: see g_applyStyleTaskbarWindowTai.
    g_applyStyleTaskbarWindowTai = hWnd;
    if (!ApplyStyle(xamlRootContent, monitorName)) {
        Wh_Log(L"ApplyStyles failed");
    }
    g_applyStyleTaskbarWindowTai = nullptr;
};

if (dispatcher.HasThreadAccess()) {
    applyOnDispatcher();
} else if (!g_unloading) {
    auto priority = winrt::Windows::UI::Core::CoreDispatcherPriority::Low;
    int highPriorityPasses = g_high_priority_dispatch_passes.load();
    while (highPriorityPasses > 0) {
        if (g_high_priority_dispatch_passes.compare_exchange_weak(
                highPriorityPasses, highPriorityPasses - 1)) {
            priority = winrt::Windows::UI::Core::CoreDispatcherPriority::High;
            break;
        }
    }
    dispatcher.TryRunAsync(priority, applyOnDispatcher);
}
} catch (...) {
    Wh_Log(L"Styling a taskbar failed: %08X", winrt::to_hresult());
}
