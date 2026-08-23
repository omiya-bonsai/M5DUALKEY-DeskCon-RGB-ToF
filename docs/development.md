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

## LED and Matrix Implementation

The orientation with the USB-C port on the rear side is treated as the front. From this front view, the physical chain is DualKey, Encoder, Angle, Chain RGB, then Chain ToF from left to right. This fixed right-side connection uses `RX=GPIO5`, `TX=GPIO6`, and 115200 baud; UART auto-detection is not used. The DualKey contains two WS2812B LEDs controlled with Adafruit NeoPixel on GPIO 21; GPIO 40 enables their power. The physical left key is pixel 0 and the right key is pixel 1. Encoder and Angle LEDs use M5Chain 1.0.8 `setRGBLight()` and `setRGBValue()` with IDs found by device discovery.

The 8 x 8 Matrix has a separate safety boundary. `RGB_MATRIX_MAX_BRIGHTNESS` is 50, `MATRIX_MASTER_BRIGHTNESS` is checked against it with `static_assert`, and every rendered RGB value has an additional level cap. Base ambient animation uses 5-12% RGB levels; proximity adds at most 8% plus subtle shimmer. Normal action frames use about 30%, and only short feedback may reach a 50% pixel level. No runtime path sets Matrix hardware brightness above 50%.

## Chain Initialization and Scheduling

Chain enumeration requires a successful device-count query, a successful device-list read, and discovered IDs for Encoder, Angle, RGB, and ToF. The total count is not required to equal four. Encoder, RGB, and ToF setup still validate both the Chain return value and operation status, but a device-specific setup failure is logged without clearing the other enumerated IDs.

Encoder rotation is polled every 15 ms and its button every 25 ms. Angle retains its existing 20 ms period. ToF completion is polled every 40 ms, and the full 64-pixel Matrix buffer is limited to one update every 80 ms (12.5 fps). Identical Matrix frames are not resent, and the cached frame advances only after `CHAIN_OK` and a successful operation status. Failed frames remain uncached so a later frame is attempted again. These limits keep the 115200-baud Chain UART available to all four modules.

ToF keeps the v0.1.0 communication sequence: `SINGLE`, a 33 ms measurement time, explicit `START`, `STOP + COMPLETE` verification, distance read, and explicit restart after each completed measurement. Valid distance uses the existing 75/25 smoothing and expires after 500 ms. Matrix ambient code consumes these results without changing the M5Chain initialization, enumeration, type-to-ID lookup, or measurement API order. No ToF code path calls USB HID.

## ToF Proximity Tuning

At each 80 ms Matrix update, valid smoothed distance is mapped linearly from the far and near points into 0-1. The target is quantized before another low-pass step, which limits small frame changes and avoids increasing bus traffic. Invalid or stale distance targets zero. This processing changes display state only and does not create events.

Important tuning constants:

| Constant | Default | Purpose |
| --- | ---: | --- |
| `TOF_AMBIENT_NEAR_MM` | 100 mm | Distance mapped to proximity 1 |
| `TOF_AMBIENT_FAR_MM` | 500 mm | Distance mapped to proximity 0 |
| `TOF_AMBIENT_PROXIMITY_STEPS` | 16 | Quantization steps before display smoothing |
| `TOF_AMBIENT_PROXIMITY_SMOOTHING` | 0.16 | Per-frame approach to the target proximity |
| `TOF_AMBIENT_SPEED_BOOST` | 0.60 | Maximum breathing-speed increase |
| `TOF_AMBIENT_BRIGHTNESS_BOOST` | 0.08 | Maximum ambient RGB-level increase |
| `TOF_AMBIENT_SPREAD_BOOST` | 0.35 | Maximum radial falloff reduction |
| `TOF_AMBIENT_SHIMMER_MAX` | 0.12 | Maximum subtle ripple modulation |

## Matrix Animation Scheduling

`MatrixAnimation` stores only the current animation, start time, and duration. A new DualKey, Encoder, or Angle action replaces the old animation. `updateMatrix()` computes a frame from `millis()` and never calls `delay()`. Available effects are red/yellow ripples, white flash-contraction, purple expansion/contraction/pulse, and blue vertical movement. Action rendering has priority over ToF proximity ambient. Proximity continues to settle while an action is visible, then drives the ambient brightness, breathing speed, spread, and subtle shimmer after the action ends.

## Angle Calibration and Scrolling

Place the Angle control at its physical center before power-up or reset. Startup calibration averages 40 valid samples at 10 ms intervals and allows up to 80 attempts. The resulting center is used for all thresholds until the next restart.

Important tuning constants in `M5DUALKEY-DeskCon-RGB-ToF.ino`:

| Constant | Default | Purpose |
| --- | ---: | --- |
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
