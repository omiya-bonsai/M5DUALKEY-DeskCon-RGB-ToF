# Architecture

Japanese: [architecture.ja.md](architecture.ja.md)

## System Overview

The orientation with the USB-C port on the rear side is treated as the front in this project. From that front view, the fixed physical layout from left to right is the ESP32-S3-based Chain DualKey, Encoder, Angle, Chain RGB, then Chain ToF. The four peripheral modules form an M5Chain UART daisy chain on the DualKey right-side port (`RX=GPIO5`, `TX=GPIO6`).

```text
Front view (USB-C on rear side)

+-----------+-----------+-----------+-----------+-----------+
| DualKey   | Encoder   | Angle     | RGB       | ToF       |
+-----------+-----------+-----------+-----------+-----------+

DualKey (ESP32-S3) -> Encoder -> Angle -> RGB -> ToF
                  M5Chain UART daisy chain
```

The firmware discovers the required peripheral modules by device type rather than assigning fixed Chain IDs. Chain enumeration succeeds when the device count and list are read successfully and Encoder, Angle, RGB, and ToF are all found; additional enumerated devices do not invalidate those discovered IDs. A later device-specific setup failure is logged without discarding unrelated device IDs. The DualKey buttons are read directly by the ESP32-S3. Encoder rotation and its button produce USB HID Consumer Control events, while Angle produces USB HID mouse-wheel events.

The three HID paths have distinct responsibilities:

```text
M5DUALKEY-DeskCon-RGB-ToF
        |
        +--> USB HID Keyboard
        |       |
        |       v
        |    Raycast
        |       |
        |       v
        |   Audio Output Switching
        |
        +--> USB HID Consumer Control
        |       |
        |       v
        |   Volume / Mute
        |
        +--> USB HID Mouse
                |
                v
             Scroll
```

Only the DualKey keyboard shortcuts use Raycast. The ESP32 does not address ORA4 or Studio Display directly; Raycast and its configured macOS script perform the actual switch. Encoder volume/mute events and Angle mouse-wheel events go directly from USB HID to macOS without Raycast.

The sensor-display path is independent of those HID paths:

```text
ToF SINGLE START
      |
      v
STOP + COMPLETE -> distance -> 75/25 smoothing -> valid/fresh check
                                                     |
                                                     v
                                      centered RGB Matrix square
```

ToF uses 33 ms SINGLE measurements. A completed distance read explicitly starts the next measurement. A stale or invalid distance turns the Matrix off; ToF does not participate in the boot animation as an LED endpoint.

## Responsibilities

- **Input processing:** debounces DualKey and Encoder input, recognizes the DualKey chord, and interprets Angle position.
- **HID output:** sends DualKey keyboard shortcuts through Raycast, while Consumer Control and mouse-wheel events go directly to macOS.
- **Angle control:** calibrates center at startup, applies hysteresis, and calculates scroll timing.
- **Chain validation:** validates that Encoder, Angle, RGB, and ToF types are present, then isolates later setup failures to the affected device.
- **Distance display:** performs bounded ToF SINGLE polling, smooths valid distance values, and updates the RGB Matrix at a bounded rate.
- **LED state:** stores the last output selected from DualKey plus the locally toggled mute state, then renders them through a dedicated LED update function.
- **macOS automation:** maps only the three DualKey audio-output shortcuts to scripts outside the firmware.

## LED State Boundary

The fixed LED mapping is: left DualKey = ORA4 in bright red, right DualKey = Studio Display in bright yellow, Encoder = mute in bright purple, and Angle = scroll activity in bright blue. The selected output and mute indicators use 1/f-like breathing, while Angle brightness follows its normalized operation amount and fades out after stopping.

LED status is maintained on the M5Stack controller. There is no feedback channel from macOS, so it is not authoritative system state. Changing audio output or mute through macOS, another keyboard, or another application can make the LED display differ from the actual macOS state. A future BLE or host-feedback feature can be added around the isolated state and LED-update layer without mixing LED commands into input handling.
