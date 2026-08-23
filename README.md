# M5DUALKEY-DeskCon-RGB-ToF

Japanese: [README.ja.md](README.ja.md)

<p align="center">
  <img src="assets/images/deskconsole-front.jpg" width="700" alt="Front view of the complete M5DUALKEY DeskConsole with DualKey, Encoder, Angle, RGB, and ToF modules">
</p>
<p align="center"><em>Complete five-module DeskConsole</em></p>

<p align="center">
  <img src="assets/images/deskconsole-lego-mount.jpg" width="700" alt="Underside view of the M5DUALKEY DeskConsole showing the LEGO Technic mounting structure">
</p>
<p align="center"><em>Underside view showing the LEGO Technic mounting structure</em></p>

M5DUALKEY-DeskCon-RGB-ToF is a compact USB HID controller for macOS, built from M5Stack Chain DualKey, Encoder, Angle, RGB, and ToF modules.

It sends standard keyboard, consumer-control, and mouse-wheel events. DualKey audio-output selection and its custom chord are delegated to Raycast (or another macOS automation tool), keeping operating-system-specific work outside the firmware.

## Features

- One-touch audio-output selection plus a separate custom two-key shortcut
- Hardware volume and mute control
- Spring-centered auto-scroll with startup calibration and hysteresis
- Low-brightness RGB status indicators for audio output and mute
- Continuously measured ToF proximity shaping the ambient light, with a time-limited 85% brightness boost and no ToF HID output
- Low-brightness 8 x 8 ambient animation and action-specific RGB feedback
- Automatic discovery and validation of the four Chain modules

## Hardware

The orientation with the USB-C port on the rear side is treated as the front in this project. All left/right references use this viewing direction. From left to right, the fixed physical layout is DualKey, Encoder, Angle, Chain RGB, then Chain ToF.

```text
Front view (USB-C on rear side)

+-----------+-----------+-----------+-----------+-----------+
| DualKey   | Encoder   | Angle     | RGB       | ToF       |
+-----------+-----------+-----------+-----------+-----------+
```

- M5Stack Chain DualKey
- M5Stack Chain Encoder
- M5Stack Chain Angle
- M5Stack Chain RGB
- M5Stack Chain ToF
- USB connection to macOS

## Quick Controls

| Module | Control | Action |
| --- | --- | --- |
| DualKey | Left | Select ORA4 (`Ctrl + Cmd + 1`), red LED |
| DualKey | Right | Select Studio Display (`Ctrl + Cmd + 2`), yellow LED |
| DualKey | Both | Custom action (`Ctrl + Option + E`) |
| Encoder | Turn / press | Volume Up, Volume Down, or Mute; purple LED while muted |
| Angle | Left / center / right | Scroll Up, stop, or Scroll Down; blue activity LED |
| Chain ToF | Continuous distance | Gently shape Matrix ambient brightness, breathing, spread, and shimmer; no HID output |
| Chain RGB | Matrix display | Proximity-aware ambient glow plus non-blocking feedback for DualKey, Encoder, and Angle actions |

DualKey keyboard shortcuts use Raycast. Encoder volume/mute and Angle scrolling are sent directly to macOS as USB HID events. ToF never sends USB HID events.

The LEDs represent firmware-maintained state, not state read back from macOS. See [Controls](docs/controls.md) for startup behavior and limitations.

## Detailed Documentation

- [Architecture](docs/architecture.md)
- [Controls and LED behavior](docs/controls.md)
- [Raycast integration](docs/raycast.md)
- [Development and build settings](docs/development.md)

## Project Status

USB HID keyboard, consumer control, Angle auto-scroll, startup calibration, continuous ToF SINGLE proximity input, RGB status indicators, and non-blocking Matrix animations are implemented. Matrix master brightness stays at 50% for idle and control actions; only an armed ToF proximity ambient window can raise it to 85% for at most 15 seconds. Full-buffer transfers are limited to 12.5 fps. BLE HID is planned.

## License

MIT License. See [LICENSE](LICENSE).

## Maintainer

omiya-bonsai
