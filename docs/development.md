# Development

Japanese: [development.ja.md](development.ja.md)

## Environment

- Arduino IDE or Arduino CLI
- Board: `M5ChainDualKey` (ESP32-S3)
- M5Stack ESP32 board package
- M5Chain 1.0.8
- M5Unified
- Adafruit NeoPixel 1.15.2 or later
- ESP32 USB HID libraries (`USBHIDKeyboard`, `USBHIDConsumerControl`, and `USBHIDMouse`)

## Important Board Settings

The current firmware uses native USB HID, not BLE HID. Select **USB-OTG (TinyUSB)** for USB Mode. Enable **USB CDC On Boot** when the Serial console is needed alongside HID. The upload mode may be set to **USB-OTG CDC (TinyUSB)** for uploads through the same native USB connection; use the board's download/boot procedure if the port does not reappear after flashing HID firmware.

Board menus vary with the M5Stack package version. Confirm that TinyUSB is selected before treating compile errors from the USB HID headers as firmware errors. BLE HID is planned but is not currently implemented.

## LED Implementation

The orientation with the USB-C port on the rear side is treated as the front. From this front view, the physical chain is DualKey, Encoder, then Angle from left to right. The DualKey contains two WS2812B LEDs controlled with Adafruit NeoPixel on GPIO 21; GPIO 40 enables their power. The physical left key is pixel 0 and the right key is pixel 1. Chain module LEDs use M5Chain 1.0.8 `setRGBLight()` and `setRGBValue()` with IDs found by device discovery. Brightness constants are kept near the LED state definitions, and LED writes are centralized in `updateLeds()`.

## Angle Calibration and Scrolling

Place the Angle control at its physical center before power-up or reset. Startup calibration averages 40 samples at 10 ms intervals. The resulting center is used for all thresholds until the next restart.

Important tuning constants in `M5DUALKEY-DeskCon-RGB-ToF.ino`:

| Constant | Default | Purpose |
| --- | ---: | --- |
| `ANGLE_CALIBRATION_SAMPLES` | 40 | Startup sample count |
| `ANGLE_CALIBRATION_INTERVAL_MS` | 10 ms | Delay between calibration samples |
| `ANGLE_STOP_OFFSET` | 95 | Return-to-center stop threshold |
| `ANGLE_START_OFFSET` | 135 | Start threshold |
| `ANGLE_READ_INTERVAL_MS` | 20 ms | ADC polling period |
| `SCROLL_SLOWEST_INTERVAL_MS` | 220 ms | Slowest wheel-event interval |
| `SCROLL_FASTEST_INTERVAL_MS` | 25 ms | Fastest wheel-event interval |
| `SCROLL_BASE_SPEED` | 0.35 | Initial speed beyond the start threshold |

The acceleration curve is `normalizedDistance^1.5`. Separate start and stop offsets provide hysteresis.

## Build Example

```sh
arduino-cli compile \
  --fqbn 'm5stack:esp32:m5stack_chain_dualkey:USBMode=default,CDCOnBoot=cdc,UploadMode=cdc,FlashSize=8M,PartitionScheme=default_8MB' \
  M5DUALKEY-DeskCon-RGB-ToF
```

Review the selected board package and port before uploading. Compilation does not replace verification on the physical DualKey, Encoder, and Angle chain.
