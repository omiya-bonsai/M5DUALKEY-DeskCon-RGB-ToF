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

The three HID paths have distinct responsibilities. ToF is outside all three paths and cannot emit HID input:

```text
DualKey direct selection --------+
DualKey both-key custom action --+--> USB HID Keyboard --> Raycast / macOS automation
                                             +--> Audio output or configured custom action

Encoder turn / press -------------> USB HID Consumer Control --> Volume / Mute
Angle position -------------------> USB HID Mouse -------------> Scroll
ToF distance ---------------------> no HID output
```

DualKey keyboard shortcuts use Raycast. The ESP32 does not address ORA4 or Studio Display directly; Raycast and its configured macOS script perform the actual switch. Encoder volume/mute events and Angle mouse-wheel events go directly from USB HID to macOS without Raycast. ToF distance is display input only.

The sensor-display path is independent of those HID paths:

```text
ToF SINGLE START
      |
      v
STOP + COMPLETE -> valid distance -> 75/25 smoothing
                                      |
                                      v
                         100-500 mm mapping
                         quantize + smooth
                                      |
                         proximity ambient parameter
                                      |
                                      v
                         brightness / breathing /
                           spread / subtle shimmer

DualKey / Encoder / Angle action -> MatrixAnimation state --+
                                                            +-> Action-priority renderer
ToF proximity ambient --------------------------------------+             |
                                                                          v
                                                               80 ms frame limiter
                                                                          |
                                                             changed frame + API success
                                                                          |
                                                                    cached frame
```

ToF keeps the proven 33 ms SINGLE sequence. A completed distance read explicitly starts the next measurement. Valid distance is smoothed 75/25, expires after 500 ms, and becomes a bounded proximity parameter for ambient rendering. It does not call Keyboard, Consumer Control, or Mouse. ToF does not participate in the boot animation as an LED endpoint.

The Matrix does not display absolute distance as discrete steps. With no action it renders a dim center glow whose brightness, breathing speed, spread, and subtle shimmer follow proximity. The newest DualKey, Encoder, or Angle action temporarily replaces that ambient frame with a ripple, contraction, expansion, pulse, or directional sweep. Rendering is `millis()`-based and does not block input, HID, or ToF work.

## Responsibilities

- **Input processing:** debounces DualKey and Encoder input, recognizes the DualKey chord, and interprets Angle position.
- **HID output:** sends DualKey keyboard shortcuts through Raycast, while Consumer Control and mouse-wheel events go directly to macOS; ToF has no HID path.
- **Angle control:** calibrates center at startup, applies hysteresis, and calculates scroll timing.
- **Chain validation:** validates that Encoder, Angle, RGB, and ToF types are present, then isolates later setup failures to the affected device.
- **Proximity input:** converts valid, smoothed ToF SINGLE distance into a quantized and smoothed ambient parameter.
- **Matrix feedback:** gives the latest control action priority over proximity ambient through a small non-blocking state machine.
- **LED state:** stores the last output selected from DualKey plus the locally toggled mute state, then renders them through a dedicated LED update function.
- **macOS automation:** maps DualKey keyboard shortcuts to scripts outside the firmware.

## LED State Boundary

The fixed LED mapping is: left DualKey = ORA4 in bright red, right DualKey = Studio Display in bright yellow, Encoder = mute in bright purple, and Angle = scroll activity in bright blue. The selected output and mute indicators use 1/f-like breathing, while Angle brightness follows its normalized operation amount and returns to standby after stopping.

LED status is maintained on the M5Stack controller. There is no feedback channel from macOS, so it is not authoritative system state. Changing audio output or mute through macOS, another keyboard, or another application can make the LED display differ from the actual macOS state. The Matrix master brightness has a compile-time 50% ceiling; base ambient frames use 5-12% RGB levels, proximity adds at most 8% plus subtle shimmer, and feedback is short-lived. A future BLE or host-feedback feature can be added around the isolated state and LED-update layer without mixing LED commands into input handling.
