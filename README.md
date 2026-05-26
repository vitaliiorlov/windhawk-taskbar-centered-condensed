![Screenshot](https://github.com/vitaliiorlov/windhawk-taskbar-centered-condensed/raw/main/screenshot.png)

# WinDock Windhawk Mod

Transform your Windows 11 taskbar into a smooth, MacOS-style dock — without losing any of the original functionality!

---

> ## 🍴 This is a personal fork
>
> Upstream: [DarkionAvey/windhawk-taskbar-centered-condensed](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed).
> This fork ([vitaliiorlov/windhawk-taskbar-centered-condensed](https://github.com/vitaliiorlov/windhawk-taskbar-centered-condensed))
> contains a few fixes I needed on my own machine, listed below.
>
> ### What's different from upstream
>
> 1. **Clickable taskbar area matches the visible taskbar.**
>    Upstream visually condenses the taskbar but `Shell_TrayWnd` itself
>    stays full screen width, so right-clicks (and the "Task Manager /
>    Taskbar settings" menu) hit the empty strips on either side. This
>    fork uses `SetWindowRgn` to make the OS only route mouse input to
>    pixels inside the visible taskbar. Also proposed upstream as
>    [PR #19](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed/pull/19).
>
> 2. **Start menu, Search popup and Notification Center work correctly
>    on multi-monitor + mixed-DPI setups.**
>    The popup placement in `DwmSetWindowAttribute_Hook` was rewritten
>    into a single unified design: all math in DIPs in monitor-local
>    coordinates, with explicit cursor-based monitor detection, a
>    cached "natural primary DIPs" size for Search, Y anchored just
>    above the visible taskbar, and a final safety clamp. Fixes
>    secondary-monitor positioning, mixed-DPI sizing, search popup
>    drift between launches, content cut-off on high-DPI monitors,
>    and the `Start menu doesn't appear` Int-overflow bug.
>
> 3. **Toast notifications are no longer hijacked.**
>    The Notification-Center positioning code used to also catch
>    toast notifications (same `ShellExperienceHost.exe` process) and
>    yank them to the top of the screen. The hook now discriminates
>    by popup height and lets Windows handle toasts natively.
>
> ### Building this fork
>
> ```powershell
> python assemble-mod.py
> ```
>
> The regen pipeline in `dependencies/main.py` fetches latest
> `taskbar-icon-size.wh.cpp` and `taskbar-start-button-position.wh.cpp`
> from ramensoftware on each run, then re-applies our fork-specific
> patches. The fork's unified popup-placement hook lives in
> `mod-parts/popup-placement-hook.cpp` and is injected into the
> regenerated `b_taskbar-start-button-position.wh.cpp` — so a clean
> regen never overwrites it. If you edit the hook, edit that file (not
> the file under `dependencies/modified-dependencies/`, which is
> regenerated output).
>
> ### Diagnostic logging
>
> The popup hook writes one structured line per popup-open to
> `%TEMP%\windhawk_popup_log.txt`. Useful for debugging multi-monitor
> placement issues. Disable by removing the `FILE* f = nullptr; ...
> fclose(f);` block in
> `dependencies/modified-dependencies/b_taskbar-start-button-position.wh.cpp`
> if you don't want it.
>
> ### Known limitations
>
> Hardcoded Search popup natural-size constants (858 × 890 DIPs)
> are tuned for Win11 24H2 and the typical default. The cache
> snapshots the user's actual value when Search is opened on the
> primary monitor, so other Win11 versions adapt automatically once
> Search opens there at least once.

---

## 🚀 How to Install (Development Build)

⚠️ **Note:** Please disable any mods that affect taskbar height or taskbar icons—this mod already includes those
features.
1. [Install Windhawk](https://windhawk.net/) if you haven't already.
2. Copy the contents of [
   `assembled-mod.cpp`](https://raw.githubusercontent.com/vitaliiorlov/windhawk-taskbar-centered-condensed/main/assembled-mod.cpp)
   to your clipboard.
3. Open **WindHawk** and navigate to: `Explore` → `Create a new mod`.
4. Press `Ctrl+A` to select all, then `Ctrl+V` to paste.
5. Click **Compile Mod** button on the top left corner.
6. Change the mod's settings to fit your preference.
7. If your explorer.exe is not responding, disable the mod from Windhawk, open task manager, restart explorer.exe, wait
   for the default taskbar to appear, then apply WinDock.

---

## 🛠 Source Code

The actual mod code is split into files under [
`mod-parts/`](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed/blob/main/mod-parts/), which are later merged together using a Python script.

❗ **Do not edit `assembled-mod.cpp` manually**, as any changes will be overwritten in the next build cycle. Instead,
modify the source files in the `mod-parts` directory.

---

## 🛣️ To-do

1. Proper unloading
2. Fix overflowing

---

## 🙌 Credits

Huge thanks to these awesome developers who made this mod possible -- your contributions to modding Windows are truly appreciated!:

- [`Michael Maltsev (m417z)`](https://github.com/m417z)
- [`Valentin Radu (valinet)`](https://github.com/valinet)

---

## 🔥 Recommended Mods

- [Smart Auto Hide for Taskbar](https://windhawk.net/mods/taskbar-auto-hide-when-maximized)
- [Taskbar Auto-Hide Speed/Frame Rate](https://windhawk.net/mods/taskbar-auto-hide-speed)
- [Show All Tray Icons](https://windhawk.net/mods/taskbar-notification-icons-show-all)

---

# Options

| Property | Name | Description | Accepted values |
| --- | --- | --- | --- |
| `TaskbarHeight` | Taskbar height | Set the height of the taskbar (Default is 74) | Non-negative integer |
| `TaskbarIconSize` | Taskbar icon size | Set the width and height of taskbar icons (Default is 42) | Non-negative integer |
| `TaskbarButtonSize` | Taskbar button size | Set the size (width and height) of taskbar buttons (Default is 74) | Non-negative integer |
| `TaskbarOffsetY` | Taskbar vertical offset | Move the taskbar up or down. Padding of the same value is applied to the top (Default is 6) | Non-negative integer |
| `TrayTaskGap` | Tray task gap | Adjust the space between the task area and the tray area (Default is 10) | Non-negative integer |
| `TaskbarBackgroundHorizontalPadding` | Taskbar background horizontal padding | Set the horizontal padding on both sides of the taskbar background (Default is 2) | Non-negative integer |
| `FullWidthTaskbarBackground` | Full-width taskbar background | When enabled, the taskbar background fills the entire width of the screen, similar to the default Windows behavior (Default is off) | Boolean (true/false) |
| `IgnoreShowDesktopButton` | Ignore "Show Desktop" button | When enabled, the "Show Desktop" button is ignored in width calculations (Default is off) | Boolean (true/false) |
| `TaskbarCornerRadius` | Taskbar corner radius | Controls how rounded the taskbar corners appear (Default is 22) | Non-negative integer |
| `TaskButtonCornerRadius` | Task button corner radius | Controls how rounded the corners of individual task buttons are (Default is 16) | Non-negative integer |
| `FlatTaskbarBottomCorners` | Flat bottom corners | When enabled, the bottom corners of the taskbar will be squared and the taskbar will dock to the screen edge. This overrides the taskbar offset; this is always on with the full-width taskbar background option (Default is off) | Boolean (true/false) |
| `CustomizeTaskbarBackground` | Stylize the taskbar background | When enabled, the taskbar background will be changed to acrylic blur. Disable this option if you are using other mods that change the taskbar background. You may need to restart explorer.exe to restore the default taskbar background (Default is on) | Boolean (true/false) |
| `TaskbarBackgroundOpacity` | Background opacity | Adjust the opacity of the taskbar background. 0 = fully transparent, 100 = fully opaque (Default is 100) | Non-negative integer |
| `TaskbarBackgroundTint` | Background tint | Modify the taskbar tint level. Higher values = more tint. Range 0-100 (Default is 0) | Non-negative integer |
| `TaskbarBackgroundLuminosity` | Background luminosity | Adjust luminosity of the taskbar background. Higher values = more opaque, lower values = more glass-like. Range 0-100 (Default is 30) | Non-negative integer |
| `TaskbarBorderOpacity` | Border opacity | Set the opacity of the taskbar border, as well as the app dividers. Range 0-100 (Default is 20) | Non-negative integer |
| `TaskbarBorderColorHex` | Border color (HEX) | Set the color of the taskbar border and app dividers, Hex color as `#RRGGBB` (Default is `#ffffff`) | string hex color |
| `TaskbarBorderThickness` | Taskbar border thickness scale (%) | Set the scale of the taskbar border. Range 0-100 (Default is 8) | unsigned int percentage |
| `AppsDividerThickness` | Apps divider thickness scale (%) | Set the thickness scale of the taskbar dividers. Range 0-100 (Default is 8) | unsigned int percentage |
| `AppsDividerVerticalScale` | Apps divider vertical scale (%) | Set the vertical scale of the taskbar dividers. Range 0-100 (Default is 40) | unsigned int percentage |
| `AppsDividerAlignment` | Choose the side on which the app dividers should appear |  |  |
| `DividedAppNames` | App names for divider placement | Type partial app names where you'd like a divider to appear. Use ; to separate multiple entries (e.g., Steam; Notepad\+\+; Settings). Case-insensitive and supports regex. | string regex |
| `TrayAreaDivider` | Tray area divider | When enabled, the tray area will be separated by a divider (Default is on) | Boolean (true/false) |
| `StyleTrayArea` | Modify the tray area appearance | When enabled, the options for tray icon size will take effect (Default is off) | Boolean (true/false) |
| `TrayIconSize` | Tray icon size | Set the width and height of tray icons. Minimum is 15. (Default is 15) | Non-negative integer |
| `TrayButtonSize` | Tray button size | Set the size (width and height) of tray buttons. Minimum is 20. (Default is 30) | Non-negative integer |
| `MoveFlyoutStartMenu` | Move Start Menu with Taskbar | When enabled, the Start and Search menus are moved to align with taskbar size and location (Default is on). | Boolean (true/false) |
| `MoveFlyoutControlCenter` | Move Control Center with Taskbar | When enabled, the Control Center is moved to align with taskbar size and location (Default is on). | Boolean (true/false) |
| `MoveFlyoutNotificationCenter` | Move Notification Center with Taskbar | When enabled, the Notification Center is moved to align with taskbar size and location (Default is on). | Boolean (true/false) |
| `AlignFlyoutInner` | Align flyout windows to the inside of the taskbar | When enabled, the flyout windows will be aligned within the bounds of the taskbar. When off, they will be 50% inside the taskbar bounds (Default is on). | Boolean (true/false) |