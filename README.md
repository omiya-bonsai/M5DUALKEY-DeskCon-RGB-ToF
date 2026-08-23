# M5DUALKEY-DeskCon-RGB-ToF

Japanese: [README.ja.md](README.ja.md)

<p align="center">
  <img src="assets/images/dualkey.jpg" width="700" alt="M5DUALKEY-DeskCon-RGB-ToF">
</p>

M5DUALKEY-DeskCon-RGB-ToF is a compact USB HID controller for macOS, built from M5Stack Chain DualKey, Encoder, Angle, RGB, and ToF modules.

It sends standard keyboard, consumer-control, and mouse-wheel events. Audio-output switching is delegated to Raycast (or another macOS automation tool), keeping operating-system-specific work outside the firmware.

## Features

- One-touch audio-output selection and toggle shortcuts
- Hardware volume and mute control
- Spring-centered auto-scroll with startup calibration and hysteresis
- Low-brightness RGB status indicators for audio output and mute
- ToF-responsive 8 x 8 RGB Matrix visualization
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
| DualKey | Both | Toggle output (`Ctrl + Option + S`) |
| Encoder | Turn / press | Volume Up, Volume Down, or Mute; purple LED while muted |
| Angle | Left / center / right | Scroll Up, stop, or Scroll Down; blue activity LED |
| Chain ToF | Hand distance | SINGLE measurements at 33 ms; each completed read explicitly starts the next measurement |
| Chain RGB | Matrix display | Off at 400 mm or farther; a centered square grows and brightens toward 50 mm |

Only the three DualKey audio-output actions use Raycast. Encoder volume/mute and Angle scrolling are sent directly to macOS as USB HID events.

The LEDs represent firmware-maintained state, not state read back from macOS. See [Controls](docs/controls.md) for startup behavior and limitations.

## Detailed Documentation

- [Architecture](docs/architecture.md)
- [Controls and LED behavior](docs/controls.md)
- [Raycast integration](docs/raycast.md)
- [Development and build settings](docs/development.md)

## Project Status

USB HID keyboard, consumer control, Angle auto-scroll, startup calibration, RGB status indicators, ToF SINGLE measurements, and the distance-responsive RGB Matrix are implemented. BLE HID is planned.

## License

MIT License. See [LICENSE](LICENSE).

## Maintainer

omiya-bonsai
