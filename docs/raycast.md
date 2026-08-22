# Raycast Integration

Japanese: [raycast.ja.md](raycast.ja.md)

Raycast is used only for the three DualKey audio-output actions. The firmware does not directly control ORA4 or Studio Display; it sends keyboard shortcuts, and Raycast maps those shortcuts to commands that run on macOS.

## Shortcut Mapping

| Shortcut received by macOS | Raycast command |
| --- | --- |
| `Ctrl + Cmd + 1` | Select ORA4 |
| `Ctrl + Cmd + 2` | Select Studio Display |
| `Ctrl + Option + S` | Toggle ORA4 / Studio Display |

Create Raycast commands that execute a Shell Script, AppleScript, or another local automation appropriate to your Mac, then assign the shortcuts above. Script contents, device names, and the audio-switching utility are user-specific and are intentionally not embedded in this repository.

Encoder Volume Up, Volume Down, and Mute bypass Raycast. They are sent directly to macOS as standard USB HID Consumer Control events. Angle scrolling also bypasses Raycast and is sent directly to macOS as USB HID mouse-wheel events.

The same keyboard-shortcut architecture can be implemented with BetterTouchTool, Keyboard Maestro, Hammerspoon, Karabiner-Elements, or another macOS automation tool. If mappings change, keep the firmware shortcuts and automation configuration consistent.
