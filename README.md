# Online symbol cache of TAI (vo fork)

Written by `tools/update-symbol-cache.py` on the `main` branch, from the
ported symbols of [windhawk-mod-symbol-cache](https://github.com/ramensoftware/windhawk-mod-symbol-cache).
Don't edit it by hand.

When Microsoft publishes a Windows update's symbol files late, Windhawk can't
find the functions the mod hooks in the updated Windows files. The mod points
Windhawk at `taskbar-dock-like-vo/` here, and each `pdb_<fingerprint>.txt`
file holds the mod's symbols for one such file, in the format of Windhawk's
own online symbol cache. A file is removed once Microsoft has published the
symbols, which Windhawk then downloads as for any other update.
