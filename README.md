# Brightless

<p align="center">
  <img src="resources/icon.png" alt="Brightless icon" width="192">
</p>

A Qt DDC/CI application for controlling external monitors on Linux and Windows.

## Features

- **Monitor controls** — Brightness, contrast, monitor-speaker volume, input selection (HDMI, DisplayPort, VGA, DVI, USB-C), and power modes (on, off, standby, suspend).
- **SDR brightness in HDR** — Per-screen SDR white-level control on KDE Plasma Wayland and on Windows 10/11 HDR displays, without requiring DDC/CI.
- **Dynamic contrast** — Link brightness and contrast globally or per monitor, with configurable ratios.
- **Schedules** — Daily time or battery-percentage triggers for DDC monitor controls, targeting one or all monitors; 12- or 24-hour time display.
- **Night Light** — Native KDE Plasma scheduling on Linux and full-desktop color filtering on Windows, with sunset/sunrise, custom times, always-on mode, day/night temperatures, and smooth transitions.
- **Monitor detection** — Asynchronous discovery, automatic refresh on screen changes, and manual rescanning.
- **Mouse wheel** — Configurable 1–10% slider steps; Linux also supports tray scrolling.
- **Global shortcuts and OSD** — Configurable shortcuts with brightness/volume feedback through Plasma's OSD on Linux or Qt's OSD on Windows.
- **Themes and settings** — System (desktop colors, e.g. Breeze), Light, Dark, AMOLED Black, Light/Dark Green, Light/Dark Blue, Purple, Red, Orange, Grey, Brown, or Yellow mode; adaptive monitor cards, scalable fonts, optional control visibility, custom VCP codes, and adjustable DDC delay.
- **Tray and autostart** — Optional close-to-tray behavior and per-user login startup, including starting minimized to the tray.
- **Saved preferences** — Atomic settings and window-size persistence, with frequent slider updates debounced.
- **Single instance** — Launching Brightless again restores the existing window.
- **Translations** — Automatically follows the system language: Chinese, English, French, German, Italian, Japanese, Korean, Polish, Portuguese, Russian, Spanish, or Turkish.

## Requirements

For monitor controls, enable **DDC/CI** in the external monitor's menu. Compatibility depends on the monitor, dock, adapter, and graphics driver. Laptop backlight control is not supported, and volume controls the monitor's speakers—not system audio.

- **Linux:** Qt/KDE and libddcutil runtime packages, plus permission to access I²C devices. SDR brightness requires KDE Plasma 6 Wayland with HDR enabled; battery triggers require UPower.
- **Windows:** Windows 10/11 x64. SDR brightness requires HDR to be enabled in Windows display settings.

## Building

See [BUILD.md](BUILD.md) for dependencies, build and test commands, installation, and release packaging.

## Usage

- **Monitor cards:** drag sliders or use the mouse wheel; dropdowns select input and power modes. HDR cards accept SDR brightness from 50–10,000 nits on Linux and the Windows SDR slider range of 80–480 nits on Windows. Tray scrolling and shortcuts still control DDC brightness.
- **Settings:** open the gear button to configure appearance, monitor controls, schedules, and system integration. **Appearance → Theme** switches System, Light/Dark, AMOLED Black, and colored palettes immediately and saves the choice. System follows desktop colors; Fusion styling keeps colors consistent across platforms.
- **Shortcuts:** in **Settings → System**, enable **global shortcuts**, choose an action, and record a key combination. Change or remove assignments in their rounded cards. Conflicts are reported, and disabling shortcuts preserves saved assignments.
- **Tray:** enable **Close to tray icon** to keep running after closing the window. Click the tray icon or select **Show Brightless** to restore it.
- **Refresh:** sliders and OSD show requested values, not hardware readback. Use refresh to reread monitors; errors appear in the main window.

**SDR brightness on Windows:** Windows exposes one SDR white level for all SDR content shown on an HDR display. Brightless lists only displays with HDR currently enabled and adjusts that level from 80 to 480 nits, matching the range of Windows' own SDR content brightness slider. Slider moves are debounced, and normal shutdown flushes the newest pending changes. Refresh rereads the current values, including changes made from Windows settings. Failed writes stay reported until the same display is successfully changed again; refreshing or changing another display does not dismiss them. This control is separate from DDC brightness and from Night Light. It uses the undocumented `DisplayConfigSetDeviceInfo` packet type `-18`, which may change in a Windows update; if that happens, Brightless reports the failure in the main window rather than writing. Brightless does not restore the previous white level on exit. Remote sessions, some mixed-HDR/multi-monitor arrangements, and other color-management tools may prevent a display from appearing or may override its value.

### Schedules

**Night Light (native Linux builds on KDE Plasma):** configure Plasma's Night Light at the top of **Settings → Schedules**, then select **Apply Night Light**. Choose sunset-to-sunrise, custom times, or always on, and set the night temperature (1000–6500 K). Sunset-to-sunrise and custom schedules also have a **Morning temperature** slider (1000–6500 K, normally 6500 K) for the daytime color temperature. Both temperatures are saved only when you select **Apply Night Light**. For sunset-to-sunrise, use Plasma's automatic location, uncheck it to choose an approximate location on the offline world map, enter latitude/longitude in decimal degrees (negative for south/west), or choose **Detect location via ipwho.is**. Clicking the map fills the manual coordinates; arrow keys adjust the selected point by one degree. The marker also follows coordinate edits, and nothing is saved until **Apply Night Light**. The map requires no network connection. IP detection sends an explicit HTTPS request to ipwho.is, which sees your public IP; coordinates are approximate and may reflect a VPN or proxy. It fills the manual coordinates without saving them until **Apply Night Light**. No IP lookup runs at startup or in the background; repeat detection when your network/location changes. Custom times use 24-hour local time; **Smooth transition (minutes)** controls the gradual temperature change at both start times and must be shorter than both the day and the night. Plasma runs this schedule even after Brightless closes. These are the same settings used by Plasma System Settings, not a separate Brightless schedule. On Plasma 6.5+, timings are shared with other day/night features. Existing Plasma temperatures and location settings are loaded for editing; unused manual coordinates are preserved. Night Light is unavailable outside a compatible Plasma session and in the current Flatpak sandbox.

**Night Light on Windows:** use the same **Settings → Schedules → Night Light** controls and select **Apply Night Light**. Brightless uses [`MagSetFullscreenColorEffect`](https://learn.microsoft.com/en-us/windows/win32/api/magnification/nf-magnification-magsetfullscreencoloreffect) to apply an approximate color-temperature matrix to the entire desktop; it does not change monitor hardware settings, magnification/zoom, or Windows' native Night light schedule. Both morning/day and night temperatures are supported. Custom schedules use local time; sunset-to-sunrise uses the manually chosen map/coordinates or the explicit **Detect location via ipwho.is** button, with the same IP privacy considerations described above. No automatic location lookup runs. Solar times are approximate (flat horizon); polar day/night are handled, and long solar transitions are capped to avoid overlap.

Brightless must remain running on Windows, including when hidden in the tray. Saved Night Light preferences resume at application startup without opening Settings. Disabling the feature or exiting normally restores the previous fullscreen color effect, unless another application has replaced Brightless's effect in the meantime. An external filter change pauses Brightless's updates and shows an error rather than repeatedly overriding it. Disable Windows Night light, Magnifier color effects, and third-party filters while using this feature; HDR, remote sessions, protected content, and display/driver configurations may affect the result. API/save failures are shown in Schedules. If preferences save successfully but the Windows API subsequently fails, the saved settings remain available for retry. A forced termination may bypass restoration.

Add an entry under **Settings → Schedules**, choose its trigger, monitor, control, and value, then enable it. Entries start disabled; brightness follows each monitor's dynamic-contrast settings.

- **Daily time:** repeats at the chosen local time. Missed times while asleep or closed are skipped.
- **Battery percentage:** runs once when the charge is **at or below** or **at or above** a 0–100% threshold, including the first matching reading after startup or enabling. It re-arms when the condition becomes false. Readings are checked every 30 seconds during charging or discharging; unavailable readings never trigger or re-arm entries.

Brightless must remain running, but its window can be hidden. Unsupported controls and unavailable monitors are ignored; entries triggered together run in list order. Power-on actions require the monitor to accept DDC/CI while asleep or off.

### Preferences

- **Linux:** `$XDG_CONFIG_HOME/brightless/settings.json`, normally `~/.config/brightless/settings.json`.
- **Windows:** `%LOCALAPPDATA%/brightless/settings.json`; Night Light uses a separate atomically saved `%LOCALAPPDATA%/brightless/night-light.json`. Shortcut assignments are stored separately in the per-user Qt settings registry.

Linux monitor IDs include the I²C bus, so moving a monitor between connectors may require reapplying per-monitor settings.

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE).

The bundled [world map](src/qt/qml/world-map.png) is an equirectangular rendering of
[Natural Earth 5.1.2 land data](https://github.com/nvkelso/natural-earth-vector/blob/v5.1.2/geojson/ne_110m_land.geojson),
which is [public domain](https://www.naturalearthdata.com/about/terms-of-use/).
