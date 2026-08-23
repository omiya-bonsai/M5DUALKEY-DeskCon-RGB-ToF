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

The orientation with the USB-C port on the rear side is treated as the front. From this front view, the physical chain is DualKey, Encoder, Angle, Chain RGB, then Chain ToF from left to right. This fixed right-side connection uses `RX=GPIO5` and `TX=GPIO6`; UART auto-detection is not used. The DualKey contains two WS2812B LEDs controlled with Adafruit NeoPixel on GPIO 21; GPIO 40 enables their power. The physical left key is pixel 0 and the right key is pixel 1. Chain module LEDs use M5Chain 1.0.8 `setRGBLight()` and `setRGBValue()` with IDs found by device discovery. The Chain LED master brightness is 100, the API maximum. LED values are cached only after both the Chain call and device operation status report success.

## Chain Initialization and Scheduling

Chain enumeration requires a successful device-count query, a successful device-list read, and discovered IDs for Encoder, Angle, RGB, and ToF. The total count is not required to equal four. Encoder, RGB, and ToF setup still validate both the Chain return value and operation status, but a device-specific setup failure is logged without clearing the other enumerated IDs.

Encoder rotation is polled every 10 ms and its button every 20 ms. Angle retains its existing 20 ms period. ToF completion is polled every 40 ms, and the full 64-pixel Matrix buffer is limited to one update every 80 ms. These limits keep the 115200-baud Chain UART available to all four modules.

ToF is configured after the boot animation and Angle calibration. It uses `SINGLE`, a 33 ms measurement time, explicit `START`, `STOP + COMPLETE` verification, distance read, and explicit restart. A failed START is retried every 250 ms. Valid distance uses the existing 75/25 smoothing and expires after 500 ms.

## Angle Calibration and Scrolling

Place the Angle control at its physical center before power-up or reset. Startup calibration averages 40 samples at 10 ms intervals. The resulting center is used for all thresholds until the next restart.

Important tuning constants in `M5DUALKEY-DeskCon-RGB-ToF.ino`:

| Constant | Default | Purpose |
| --- | ---: | --- |
| `ANGLE_CALIBRATION_SAMPLES` | 40 | Startup sample count |
| `ANGLE_CALIBRATION_INTERVAL_MS` | 10 ms | Delay between calibration samples |
| `ANGLE_CALIBRATION_MAX_ATTEMPTS` | 80 | Maximum attempts used to obtain 40 valid samples |
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

Review the selected board package and port before uploading. Compilation does not replace verification on the physical DualKey, Encoder, Angle, RGB, and ToF chain.
