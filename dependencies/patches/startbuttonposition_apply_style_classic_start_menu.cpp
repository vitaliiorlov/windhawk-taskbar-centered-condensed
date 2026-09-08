void ApplyStyleClassicStartMenu(FrameworkElement content, HMONITOR monitor) {
    // Must be explicitly global-qualified: this file is injected into
    // namespace StartMenuUI, which declares its own `void ApplyStyle();`.
    // Unqualified lookup stops at that inner declaration and never reaches
    // the global two-argument overload defined in win-dock-mod.cpp.
    ::ApplyStyle(content, GetMonitorName(monitor));
}
