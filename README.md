# kdedimmer

A click-through screen dimmer overlay for KDE Plasma Wayland. Adds a transparent dark layer on top of everything (including panels and fullscreen apps) with adjustable opacity.

## Features

- Covers all monitors with dark overlay
- Fully click-through (doesn't intercept mouse/keyboard)
- Always on top (above panels, fullscreen apps, everything)
- System tray icon with slider control
- D-Bus interface for CLI control
- Systemd user service for autostart

## Dependencies

- Qt 6 (Widgets, Gui, DBus, WaylandClient)
- layer-shell-qt
- wayland-client

Arch Linux:
```bash
sudo pacman -S qt6-base qt6-wayland layer-shell-qt wayland
```

## Installation

### From AUR (Arch Linux)

```bash
yay -S kdedimmer
# or
paru -S kdedimmer
```

Then enable the service:
```bash
systemctl --user enable --now kdedimmer
```

### From Source

```bash
./install.sh
```

This will:
1. Build the project
2. Install binary to `~/.local/bin/`
3. Install systemd user service
4. Enable and start the service

## Usage

### System Tray
- Right-click tray icon for slider (0-90% opacity)
- Left/middle-click to toggle on/off

### CLI Commands

```bash
kdedimmer              # Start daemon (if not using systemd)
kdedimmer +5           # Increase opacity by 5%
kdedimmer -5           # Decrease opacity by 5%
kdedimmer set 50       # Set opacity to 50%
kdedimmer toggle       # Toggle on/off
kdedimmer on           # Enable overlay
kdedimmer off          # Disable overlay
kdedimmer get          # Print current opacity
kdedimmer status       # Print running status
```

### Keyboard Shortcuts

Add custom shortcuts in KDE Settings → Shortcuts → Custom Shortcuts:

| Action | Command |
|--------|---------|
| Dim more | `kdedimmer +5` |
| Dim less | `kdedimmer -5` |
| Toggle dimmer | `kdedimmer toggle` |

## Service Management

```bash
systemctl --user status kdedimmer   # Check status
systemctl --user restart kdedimmer  # Restart
systemctl --user stop kdedimmer     # Stop
systemctl --user disable kdedimmer  # Disable autostart
```

## Uninstall

```bash
systemctl --user disable --now kdedimmer
rm ~/.local/bin/kdedimmer
rm ~/.config/systemd/user/kdedimmer.service
systemctl --user daemon-reload
```

## Technical Details

- Uses `layer-shell-qt` with `LayerOverlay` to render above all windows
- Empty Wayland input region for complete click-through
- D-Bus service: `org.kde.kdedimmer` at path `/Dimmer`

## License

GPL-3.0-or-later
