![Screenshot](https://github.com/vitaliiorlov/windhawk-taskbar-centered-condensed/raw/main/screenshot.png)

# TAI (Taskbar as (an) island) for Windows 11

TAI lets you transform your Windows 11 taskbar into a smooth floating dock without losing any of the original taskbar functionality!


# Bug reports

This is a fork. File issues with **this fork** here: https://github.com/vitaliiorlov/windhawk-taskbar-centered-condensed/issues

For problems that also reproduce on upstream TAI, please report them upstream
instead: https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed/issues

---

> ## 🍴 This is a personal fork
>
> Upstream: [DarkionAvey/windhawk-taskbar-centered-condensed](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed).
> This fork ([vitaliiorlov/windhawk-taskbar-centered-condensed](https://github.com/vitaliiorlov/windhawk-taskbar-centered-condensed))
> tracks upstream closely and adds three things on top.
>
> ### What's different from upstream
>
> 1. **The clickable taskbar area matches the visible island.**
>    Upstream condenses the taskbar visually, but `Shell_TrayWnd` itself stays
>    full monitor width, so right-clicks (and the "Task Manager / Taskbar
>    settings" menu) land on the empty strips either side. This fork uses
>    `SetWindowRgn` so the OS only routes mouse input to pixels inside the
>    island. The clip is driven by the post-scale island bounds, so it tracks
>    the island when upstream shrinks it on overflow. Also proposed upstream as
>    [PR #19](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed/pull/19).
>
> 2. **`NotificationCenterPrimaryOnly` setting.**
>    Limits Notification Center repositioning to the primary monitor; on
>    secondary monitors Windows' native placement is used instead. Upstream has
>    an on/off toggle for the Notification Center but nothing that scopes it by
>    monitor.
>
> 3. **Flyout placement is mirrored to a log file.**
>    One line per flyout open, appended to `windhawk_popup_log.txt` under
>    `%TEMP%`, so multi-monitor and mixed-DPI placement can be diagnosed
>    without attaching DebugView.
>
> Everything else — the island auto-scaling, the WindhawkBlur engine, flyout
> monitor resolution, the Y-above-taskbar clamp and Notification-Center
> detection — is upstream's code, used as-is. Earlier versions of this fork
> carried their own popup-placement hook; it was dropped in favour of
> upstream's, which handles the same cases more robustly.
>
> ### Building
>
> ```powershell
> python assemble-mod.py
> ```
>
> Requires `requests` and `art`. The pipeline fetches the latest
> `taskbar-icon-size` and `taskbar-start-button-position` from ramensoftware on
> every run and re-applies the patches, so a build can start failing when those
> upstream sources change. That is deliberate: `dependencies/cpp_patcher.py`
> raises rather than silently emitting a broken mod. Fork changes belong in
> `mod-parts/`, `dependencies/patches/`, or the processors — never in
> `dependencies/modified-dependencies/`, which is regenerated output.

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

---

## 🛠 Source Code

The actual mod code is split into files under [
`mod-parts/`](https://github.com/vitaliiorlov/windhawk-taskbar-centered-condensed/blob/main/mod-parts/), which are later merged together using a Python script.

❗ **Do not edit `assembled-mod.cpp` manually**, as any changes will be overwritten in the next build cycle. Instead,
modify the source files in the `mod-parts` directory.

---

## 🙌 Credits

Huge thanks to these awesome developers who made this mod possible -- your contributions to modding Windows are truly appreciated!:

- [`Michael Maltsev (m417z)`](https://github.com/m417z)
- [`Valentin Radu (valinet)`](https://github.com/valinet)
- [`TranslucentTB team`](https://github.com/translucenttb/translucenttb)

---

## 🔥 Recommended Mods
- [Taskbar Fluent Media Player](https://windhawk.net/mods/taskbar-fluent-media-player) \[Set player position to Tray\]
- [Smart Auto Hide for Taskbar](https://windhawk.net/mods/taskbar-auto-hide-when-maximized)
- [Taskbar Auto-Hide Speed/Frame Rate](https://windhawk.net/mods/taskbar-auto-hide-speed)
- [Show All Tray Icons](https://windhawk.net/mods/taskbar-notification-icons-show-all)

---

# Options

| Property | Name | Description | Accepted values |
| --- | --- | --- | --- |
| `TaskbarHeight` | Taskbar height | Set the height of the taskbar. Default is 74 | Non-negative integer |
| `TaskbarIconSize` | Taskbar icon size | Set the width and height of taskbar icons. Values below 8 are clamped to 8; values above the current taskbar/button size are clamped to fit. Default is 42 | Non-negative integer |
| `TaskbarButtonSize` | Taskbar button size | Set the size (width and height) of taskbar buttons. Default is 72 | Non-negative integer |
| `TaskbarOffsetY` | Taskbar vertical offset | Move the taskbar up or down. Padding of the same value is applied to the top. Default is 6 | Non-negative integer |
| `TrayTaskGap` | Tray task gap | Adjust the space between the task area and the tray area. Default is 10 | Non-negative integer |
| `TaskbarBackgroundHorizontalPadding` | Taskbar background horizontal padding | Set the horizontal padding on both sides of the taskbar background. Default is 2 | Non-negative integer |
| `FullWidthTaskbarBackground` | Full-width taskbar background | When enabled, the taskbar background fills the entire width of the screen, similar to the default Windows behavior. Default is off | Boolean (true/false) |
| `IgnoreShowDesktopButton` | Ignore "Show Desktop" button | When enabled, the "Show Desktop" button is ignored in width calculations. Default is off | Boolean (true/false) |
| `TaskbarCornerRadius` | Taskbar corner radius | Controls how rounded the taskbar corners appear. Default is 22 | Non-negative integer |
| `TaskButtonCornerRadius` | Task button corner radius | Controls how rounded the corners of individual task buttons are. Default is 18 | Non-negative integer |
| `FlatTaskbarBottomCorners` | Flat bottom corners | When enabled, the bottom corners of the taskbar will be squared and the taskbar will dock to the screen edge. This overrides the taskbar offset; this is always on with the full-width taskbar background option. Default is off | Boolean (true/false) |
| `CustomizeTaskbarBackground` | Stylize the taskbar background | When enabled, this mod applies its taskbar background visuals. When disabled, this mod skips all taskbar background changes so other Windhawk mods can provide their own background. Default is on | Boolean (true/false) |
| `DisableCustomBlurBackground` | Disable custom blur background | When enabled, the WindhawkBlur brush is skipped and only the fallback color is used, producing a solid background. Background blur, tint, tint color, luminosity, saturation, and inversion settings are ignored. This also activates automatically when background tint is 100, background blur amount is 0, or background inversion is 100. Default is off | Boolean (true/false) |
| `TaskbarBackgroundOpacity` | Background opacity | Adjust the opacity of the taskbar background. 0 = fully transparent, 100 = fully opaque. Default is 100 | Non-negative integer |
| `TaskbarBackgroundTint` | Background tint | Modify the taskbar tint level. Higher values = more tint. Range 0-100. Default is 0 | Non-negative integer |
| `TaskbarBackgroundLuminosity` | Background luminosity | Adjust luminosity of the taskbar background. Higher values = more opaque, lower values = more glass-like. Range 0-100. Default is 20 | Non-negative integer |
| `TaskbarBackgroundBlurAmount` | Background blur amount | WindhawkBlur Gaussian blur amount. Higher values make the taskbar glass blur stronger. Must be non-negative. Default is 30 | Non-negative integer |
| `TaskbarBackgroundTintColor` | Background tint color | WindhawkBlur tint color. Accepts `#RRGGBB`, `#AARRGGBB`, or {ThemeResource Name}. Default is {ThemeResource CardStrokeColorDefaultSolid}. Use {ThemeResource SystemBaseHighColor} if you want white color when Windows is in dark mode, or black in light mode. | Text |
| `TaskbarBackgroundTintSaturation` | Background saturation | WindhawkBlur saturation applied before tint. 0 = grayscale, 100 = normal, 200-500 = boosted saturation. Must be non-negative. Default is 200. Max is 500 | Non-negative integer |
| `TaskbarBackgroundInversion` | Background inversion | Inverts the blurred background behind the taskbar to enhance contrast. 0 = off, 100 = fully inverted. Default is 10 | Non-negative integer |
| `TaskbarBackgroundFallbackColor` | Background fallback color | Color used when transparency effects or energy saver disable blur, or when the custom blur background is disabled. Accepts `#RRGGBB`, `#AARRGGBB`, or {ThemeResource Name}. Default is {ThemeResource CardStrokeColorDefaultSolid} | Text |
| `TaskbarBorderOpacity` | Border opacity | Set the opacity of the taskbar border, as well as the app dividers. Range 0-100. Default is 10 | Non-negative integer |
| `TaskbarBorderColorHex` | Border color (HEX) | Set the color of the taskbar border and app dividers, Hex color as `#RRGGBB`. Default is `#ffffff` | string hex color |
| `TaskbarBorderThickness` | Taskbar border thickness scale (%) | Set the scale of the taskbar border. Range 0-100. Default is 8 | unsigned int percentage |
| `AppsDividerThickness` | Apps divider thickness scale (%) | Set the thickness scale of the taskbar dividers. Range 0-100. Default is 8 | unsigned int percentage |
| `AppsDividerVerticalScale` | Apps divider vertical scale (%) | Set the vertical scale of the taskbar dividers. Range 0-100. Default is 40 | unsigned int percentage |
| `AppsDividerAlignment` | Choose the side on which the app dividers should appear |  |  |
| `DividedAppNames` | App names for divider placement | Type partial app names where you'd like a divider to appear. Use ; to separate multiple entries (e.g., Steam; Notepad\+\+; Settings). Case-insensitive and supports regex. | string regex |
| `TrayAreaDivider` | Tray area divider | When enabled, the tray area will be separated by a divider. Default is off | Boolean (true/false) |
| `StyleTrayArea` | Modify the tray area appearance | When enabled, the options for tray icon size will take effect. Default is off | Boolean (true/false) |
| `TrayIconSize` | Tray icon size | Set the width and height of tray icons. Minimum is 15. Default is 15 | Non-negative integer |
| `TrayButtonSize` | Tray button size | Set the size (width and height) of tray buttons. Minimum is 20. Default is 30 | Non-negative integer |
| `MoveFlyoutStartMenu` | Move Start Menu with Taskbar | When enabled, the Start and Search menus are moved to align with taskbar size and location. Default is on. | Boolean (true/false) |
| `MoveFlyoutControlCenter` | Move Control Center with Taskbar | When enabled, the Control Center is moved to align with taskbar size and location. Default is on. | Boolean (true/false) |
| `MoveFlyoutNotificationCenter` | Move Notification Center with Taskbar | When enabled, the Notification Center is moved to align with taskbar size and location. Default is on. | Boolean (true/false) |
| `AlignFlyoutInner` | Align flyout windows to the inside of the taskbar | When enabled, the flyout windows will be aligned within the bounds of the taskbar. When off, they will be 50% inside the taskbar bounds. Default is on. | Boolean (true/false) |
| `NotificationCenterPrimaryOnly` | Notification Center on primary monitor only | When enabled, the Notification Center (the clock/calendar popup) is only repositioned when opened on the primary monitor. On secondary monitors it appears at Windows' native default position. Only affects the Notification Center - Start Menu, Search and Control Center are unaffected. Default is off. | Boolean (true/false) |