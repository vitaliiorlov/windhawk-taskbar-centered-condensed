![Screenshot](https://raw.githubusercontent.com/vitaliiorlov/windhawk-taskbar-centered-condensed/main/screenshot.png)

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
> tracks upstream closely; everything it adds or fixes on top is listed in
> [What's different from upstream](https://github.com/vitaliiorlov/windhawk-taskbar-centered-condensed/blob/main/FORK-CHANGES.md).
>
> ### ⚠️ Own mod ID since 1.5.271
>
> This fork used to share the mod ID `taskbar-dock-like` with the original
> TAI. It now has its own, `taskbar-dock-like-vo`, so Windhawk keeps the two
> apart. **If TAI is already installed** (the original or an earlier build of
> this fork), update it the usual way: open it in Windhawk, click **Edit**,
> paste the new code over it and compile. Windhawk moves it to the new ID and
> keeps your settings. Don't install it with **Create a new mod** while the
> old one is there: you would get two copies, and this one stays off until
> the old one is removed or disabled. See *How to Install* below.
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
>
> ### Symbol cache
>
> `.github/workflows/symbol-cache.yml` runs `tools/update-symbol-cache.py`
> every hour and whenever the mod changes on `main`. The script reads the
> ported symbols that windhawk-mod-symbol-cache has for the DLLs the mod
> hooks and writes the mod's symbols from them to the `symbol-cache` branch,
> in the format of Windhawk's online symbol cache, and the mod points Windhawk
> at `taskbar-dock-like-vo/` there. A symbol that a ported file doesn't have
> is left out, so Windhawk looks for it in Microsoft's symbol file, and a
> cache file is removed once Microsoft has published that file. The script
> needs Python 3.14 or later and nothing else; to run it by hand, give
> `--output` a checkout of the `symbol-cache` branch.

---

## 🚀 How to Install (Development Build)

⚠️ **Note:** Please disable any mods that affect taskbar height or taskbar icons—this mod already includes those
features.
1. [Install Windhawk](https://windhawk.net/) if you haven't already.
2. Copy the contents of [
   `assembled-mod.cpp`](https://raw.githubusercontent.com/vitaliiorlov/windhawk-taskbar-centered-condensed/main/assembled-mod.cpp)
   to your clipboard.
3. Open **WindHawk** and navigate to: `Explore` → `Create a new mod`.
   **Already have TAI** (the original or an earlier build of this fork)? Open
   it in Windhawk and click **Edit** instead: pasting over it moves it to this
   fork's mod ID and keeps your settings.
4. Press `Ctrl+A` to select all, then `Ctrl+V` to paste.
5. Click **Compile Mod** button on the top left corner.
6. Change the mod's settings to fit your preference.

If the mod doesn't start after an update (for example, when a Windows update
broke the previous version), turn it off and on again in Windhawk; if that
doesn't help at once, wait a minute and repeat. Restarting Explorer won't
help: Windhawk doesn't retry a mod that failed to load there for up to 4 hours.

The first time Explorer starts after a Windows update, Windhawk looks up
this mod's symbols for the updated Windows files. While Microsoft hasn't
published them yet, they come from this repository's symbol cache (see
*Symbol cache* above); if the mod started before they got there, turn it
off and on again later. Otherwise Windhawk downloads them
from Microsoft. After a large update that can take a few minutes (one in
October 2026 needed about 185 MB). Until it finishes, Explorer can take a
little longer to appear and the taskbar keeps Windows' own sizes; it is
restyled once the download is done. Restarting Explorer meanwhile only
starts the download over.

---

## 🛠 Source Code

The actual mod code is split into files under [
`mod-parts/`](https://github.com/vitaliiorlov/windhawk-taskbar-centered-condensed/blob/main/mod-parts/), which are later merged together using a Python script.

❗ **Do not edit `assembled-mod.cpp` manually**, as any changes will be overwritten in the next build cycle. Instead,
modify the source files in the `mod-parts` directory.

---

## 🙌 Credits

- [DarkionAvey](https://github.com/DarkionAvey): the original
  [TAI](https://github.com/DarkionAvey/windhawk-taskbar-centered-condensed),
  which this fork builds on.
- [Michael Maltsev (m417z)](https://github.com/m417z): every build bundles his
  mods [Taskbar height and icon size](https://windhawk.net/mods/taskbar-icon-size)
  and [Start button always on the left](https://windhawk.net/mods/taskbar-start-button-position).
- [Valentin Radu (valinet)](https://github.com/valinet) and the
  [TranslucentTB team](https://github.com/translucenttb/translucenttb), whom
  the original TAI credits.

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
| `MoveFlyoutKeyboardLayout` | Move keyboard layout flyout with Taskbar | When enabled, the keyboard layout flyout (opened by clicking the language indicator or pressing Win+Space) is centered above the language indicator on the taskbar instead of at the right edge of the screen. On a monitor whose taskbar has no language indicator it is centered above the tray instead. Default is on. | Boolean (true/false) |
| `MoveTrayContextMenus` | Move tray icon menus with Taskbar | When enabled, the right-click menus of the clock and the system tray icons (network, volume, battery, language) open at the right end of the taskbar, lined up with the Notification Center, instead of at the right edge of the screen. Default is on. | Boolean (true/false) |
| `StartMenuOnActiveMonitor` | Open Start on the monitor in use | When enabled, the Start menu opened with the Win key (or Ctrl+Esc) opens on the monitor of the window you are working in, or the one under the mouse when the desktop or a taskbar has focus, instead of always on the main display. A taskbar's Start button already opens it on that taskbar's monitor. Only monitors with a taskbar are used. Default is on. | Boolean (true/false) |
| `AutoHideShowUnderTaskbarOnly` | Show an auto-hidden taskbar only from under it | When the taskbar hides automatically, it comes back only when the mouse reaches the screen edge under the taskbar, instead of anywhere along that edge. The rest of the edge is left to the windows behind it. Default is on. | Boolean (true/false) |
| `LogFlyoutPlacement` | Log flyout placement to a file | When enabled, a line is added to windhawk_popup_log.txt in %TEMP% each time the Start menu, Search, a flyout or a taskbar menu is placed, to help diagnose placement bugs. At 1 MB the file is renamed windhawk_popup_log.old.txt and a new one is started. Default is off. | Boolean (true/false) |