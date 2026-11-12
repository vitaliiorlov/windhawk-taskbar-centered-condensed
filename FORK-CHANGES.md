# What's different from upstream

Everything this fork, [vitaliiorlov/windhawk-taskbar-centered-condensed](https://github.com/vitaliiorlov/windhawk-taskbar-centered-condensed),
adds to or fixes in upstream TAI,
[DarkionAvey/windhawk-taskbar-centered-condensed](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed).
See the [README](README.md) for installing and building.

## Added

1. **The clickable taskbar area matches the visible island.**
   Upstream condenses the taskbar visually, but `Shell_TrayWnd` itself stays
   full monitor width, so right-clicks (and the "Task Manager / Taskbar
   settings" menu) land on the empty strips either side. This fork uses
   `SetWindowRgn` so the OS only routes mouse input to pixels inside the
   island. The clip is driven by the post-scale island bounds, so it tracks
   the island when upstream shrinks it on overflow, and reaches a pixel past
   each end of the island, whose edges seldom fall on a pixel boundary, so
   the border is never cut off at any display scale. While an auto-hidden
   taskbar is off screen the clip keeps to the island's width, so only the
   edge under the island brings it back (see 7). Also proposed upstream as
   [PR #19](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed/pull/19).

2. **`NotificationCenterPrimaryOnly` setting.**
   Limits Notification Center repositioning to the primary monitor; on
   secondary monitors Windows' native placement is used instead. Upstream has
   an on/off toggle for the Notification Center but nothing that scopes it by
   monitor.

3. **Flyout placement can be mirrored to a log file.**
   With the `LogFlyoutPlacement` setting on (off by default), one line per
   flyout open is appended to `windhawk_popup_log.txt` under `%TEMP%`, so
   multi-monitor and mixed-DPI placement can be diagnosed without attaching
   DebugView. The Start menu, Search and Notification Center lines show
   where Windows put the window and where it was moved; repairs to a taskbar
   (see 11) are logged there too. At 1 MB the file is renamed
   `windhawk_popup_log.old.txt` and a new one is started.

4. **The keyboard layout flyout opens above the language indicator.**
   Clicking the language indicator (or pressing Win+Space) opens a flyout
   that Windows centres on the tray's stock position at the right edge of
   the screen, far from the island. This fork re-centres it on the language
   indicator as the flyout is positioned, on whichever monitor Windows opens
   it, clamped to that monitor's work area. Controlled by the
   `MoveFlyoutKeyboardLayout` setting (on by default).

5. **The taskbar's right-click menus open at the island.**
   Windows anchors the menus of the clock and the system icons (network,
   volume, battery, language) to the right edge of the screen and, once
   the Start menu is moved to the island, the Start button's to the left
   edge. This fork moves those anchors as far as the flyouts beside them
   move (`ContextMenuPositionHelper` in `SystemTray.dll`,
   `GetLeadingStartFlyoutPosition` in `Taskbar.View.dll`), so the menus
   open at the island's ends, and keeps one-item tray menus such as the
   battery's above the taskbar instead of over it. Tray menus follow the
   `MoveTrayContextMenus` setting (on by default), the Start button's
   follows `MoveFlyoutStartMenu`.

6. **The Win key opens Start on the monitor in use.**
   Windows opens the Start menu on the main display when it is opened with
   the Win key (or Ctrl+Esc), whichever monitor is being worked on; only a
   taskbar's Start button opens it elsewhere. This fork has Windows open it
   on the monitor of the window in focus, or the one under the mouse when
   the desktop or a taskbar has focus, the way a Start button click does
   (`ImmersiveMonitorHelper::AdjustMonitorConnectedIfNeeded` in
   `twinui.pcshell.dll`), so it is laid out there properly. Controlled by
   the `StartMenuOnActiveMonitor` setting (on by default).

7. **An auto-hidden taskbar comes back only from under the island.**
   Windows keeps a 2px sliver of an auto-hidden taskbar on the screen and
   brings the taskbar back when the mouse touches it. The taskbar window is
   as wide as the monitor, so with the island in the middle, reaching the
   bottom edge anywhere, even in a far corner, brought it up. This fork
   cuts the hidden taskbar's window region to the island, so only the edge
   under the island brings it back and the rest of the edge is left to the
   windows behind it. Windows' own clip, which keeps a hidden taskbar off a
   neighbouring monitor, is kept. The *Edge detection* option of "Taskbar
   Auto-Hide Instant Show" brings the taskbar up from anywhere on the edge
   by itself, so leave it off to use this. Controlled by the
   `AutoHideShowUnderTaskbarOnly` setting (on by default).

## Fixed

8. **The system tray lines up with the island on Windows 11 25H2.**
   On 25H2 the clock, battery, volume and language indicator sat above or
   below the app icons by exactly `TaskbarOffsetY`, and came back that way
   after every Explorer restart
   ([upstream issues #26](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed/issues/26)
   and [#30](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed/issues/30)).
   The tray frame is shorter than the offset-expanded taskbar and relied on
   XAML centring it implicitly, which 25H2 stopped doing; the fork pins its
   vertical alignment to centre on every pass.

9. **The mod loads on the September 2026 Windows update.**
   Build 26100.9549 / 26200.9550 added a parameter to
   `CTaskBand::_UpdateItemIcon` in `taskbar.dll`, and the mod, whose hook
   only knew the old signature, stopped loading
   ([upstream issue #34](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed/issues/34)).
   The hook is optional, so a future change costs one layout trigger rather
   than the whole mod. The October 2026 preview update (26100.9818 /
   26200.9818) was such a change: it renamed the parameter's element type
   from `TaskbarIcon::Variant` to `TaskbarIcon::IconMetrics`. The hook
   accepts all three signatures.

10. **The Notification Center opens beside the island from the first click.**
    Upstream moves the clock and calendar flyout as Explorer reveals it, and
    recognises it by its window title. On the first open after Explorer
    starts, the window is revealed before it has a size or that title, so it
    opened at the right edge of the screen. This fork also shifts the
    position Explorer computes for the flyout
    (`CActionCenterExperienceManager::GetViewPosition` in `twinui.pcshell.dll`),
    so every open lands where upstream's code puts the later ones. Follows
    the `MoveFlyoutNotificationCenter` and `NotificationCenterPrimaryOnly`
    settings.

11. **Taskbars on several monitors are kept apart, and survive display changes.**
    Each taskbar is matched to its monitor by the `TaskbarMonitor` property
    Explorer sets on it, not by where its window happens to be: an
    auto-hidden taskbar parked over the monitor below it was styled and
    clipped as that monitor's taskbar, cutting the island off
    ([upstream issue #32](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed/issues/32)).
    A monitor plugged back in, or taking over as the main display, starts
    from fresh taskbar state instead of the previous taskbar's. The taskbar
    lookup no longer crashes Explorer when a display change catches a
    taskbar before its frame exists. A secondary taskbar that Windows sets
    up while the primary one is auto-hidden, which Windows leaves invisible
    (`TaskbarHost::Start_System` takes the primary taskbar's state), is
    shown. And the Start menu and the Notification Center, which Windows
    lays out for the monitor it opens them on, are no longer moved to the
    monitor of whichever taskbar the cursor is over, which drew the Start
    menu part-way across the wrong monitor; Search still follows the
    taskbar whose Search button was clicked.

12. **The running-app underline sits below the icon, not behind it.**
    Windows keeps a task button's underline a fixed distance up from the
    button's bottom edge and centres the icon, so a bigger icon or a shorter
    taskbar put the underline behind the icon's bottom edge. The icon is
    drawn on top, so the short grey underline of an app in the background
    disappeared entirely. Where they would overlap, the fork moves the
    underline down to just below the icon, still inside the button's
    highlight, and makes it thinner when the room is tight; with no room at
    all (an icon about as tall as the highlight) it draws the underline over
    the icon rather than hide it. It is measured from the button's layout,
    so it follows the taskbar height, icon size and button size settings at
    any display scale. The progress bar some apps show on their button gets
    the same move.

13. **Start no longer freezes waiting on a flyout.**
    Start could freeze until Explorer was restarted, mostly on opening
    Settings from it. Just before Explorer shows the Start menu, Search or
    the Notification Center, the mod moves the window, which belongs to
    another process, and `SetWindowPos` waits, with no time limit, for that
    process to handle the move. When it didn't, for example while it was
    suspended, the Explorer thread that shows Start and launches apps was
    stuck with it. The fork now queues the move, as Windows does for these
    windows, and gives the flyout at most 250 ms to catch up before it is
    shown. Builds since September 2026 also moved each flyout as it hid, to
    where it already was, on every Start close and app launch; hiding
    flyouts are left alone again, as in upstream TAI. And the mod's
    `SetWindowPos` hook, which sees every window move in Explorer, now
    reads settings and process names only for the windows it changes.

14. **Explorer's helper processes no longer crash on exit.**
    Explorer starts short-lived helper processes, for example to open an
    app from `shell:AppsFolder` or to host folder windows, and the mod
    loaded into each one. A `std::thread` still joinable at exit ends the
    process with `std::terminate`, so every helper exit was logged as a
    crash: about 200 `Application Error` events a month for `explorer.exe`,
    module `libc++.whl`, code `0x40000015`, each with a crash dump. The
    shell itself was not affected. The mod's worker threads are now plain
    Windows threads, and a helper started with arguments while another
    Explorer owns the taskbar skips the mod altogether.

15. **Explorer no longer hangs on its first start after a Windows update.**
    When an update replaces the taskbar's DLLs, Windhawk has to download
    their symbols for this mod from Microsoft before it can hook them, since
    Windhawk's online symbol cache only covers mods from its catalog; the
    October 2026 preview update needed about 185 MB. The DLLs Explorer loads
    after the mod, `Taskbar.View.dll`, `SystemTray.dll` and
    `SearchUx.UI.dll`, were hooked on the thread loading them, mostly the
    taskbar's, so Explorer hung until the downloads were done. Restarting it
    started them over, so it could hang on every start. The fork hooks those
    DLLs on a worker thread and lets the loading thread wait for it for at
    most a second. With the symbols downloaded nothing changes; until then
    Explorer runs with Windows' own taskbar sizes, and the taskbar is
    restyled as soon as the hooks are in.

16. **Explorer no longer re-reads symbol files on every start.**
    Windhawk caches the symbols a mod looks up, one entry per DLL, but a
    lookup that misses replaces the entry with its own symbols. The mod
    looks up symbols in `taskbar.dll`, `Taskbar.View.dll`, `SystemTray.dll`
    and `twinui.pcshell.dll` from two or three places each, so the lookups
    kept replacing each other's entries, and every Explorer start parsed
    those symbol files again, the largest 50 MB. Each lookup now includes
    the symbols of the earlier lookups on its DLL, so the cache holds them
    all.

17. **The Start button's hooks are set when Explorer starts with the mod on.**
    Both bundled mods hooked `LoadLibraryExW` to catch `Taskbar.View.dll`
    loading, but Windhawk lets a mod hook a function only once and refused
    the second hook. So whenever Explorer started with the mod already
    enabled, as at every sign-in, the Start button code's `Taskbar.View.dll`
    hooks were not set, among them the placement of the Start button's
    right-click menu (see 5); they worked only after the mod was turned on
    in a running Explorer. The two now share one hook.

18. **The mod loads when Microsoft is late with a Windows update's symbols.**
    Windhawk finds the functions the mod hooks in the symbol files that
    Microsoft publishes for each Windows build. The November 2026 update
    (26100.9974 / 26200.9974) arrived before its symbol files did, so the
    mod couldn't load and left Windows' own taskbar. The mods from
    Windhawk's catalog kept working, from symbols that Windhawk's
    [symbol cache project](https://github.com/ramensoftware/windhawk-mod-symbol-cache)
    ported from the previous build, but Windhawk only looks those up for
    its catalog. A workflow in this repository now copies the ported symbols
    this mod needs to its `symbol-cache` branch, checking every hour, and
    the mod has Windhawk look there before it downloads symbols from
    Microsoft (see [Symbol cache](README.md#symbol-cache) in the README). For a Windows build that isn't
    there, the usual case, that costs one small request to GitHub.

Everything else — the island auto-scaling, the WindhawkBlur engine, the
Y-above-taskbar clamp and Notification-Center detection — is upstream's
code, used as-is. Earlier versions of this fork carried their own
popup-placement hook; it was dropped in favour of upstream's, which handles
the same cases more robustly.
