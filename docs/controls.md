# Controls and LED Behavior

Japanese: [controls.ja.md](controls.ja.md)

The orientation with the USB-C port on the rear side is treated as the front. From this front view, the physical layout from left to right is DualKey, Encoder, Angle, Chain RGB, then Chain ToF. All left/right controls and LED positions below are described from this viewing direction.

## DualKey

| Control | HID shortcut | Expected Raycast action |
| --- | --- | --- |
| Left key | `Ctrl + Cmd + 1` | Select ORA4 |
| Right key | `Ctrl + Cmd + 2` | Select Studio Display |
| Both keys | `Ctrl + Option + E` | Run the configured custom action |

The two-key chord has priority. A short chord window prevents a near-simultaneous press from producing a single-key action first.

These keyboard shortcuts use Raycast or equivalent macOS automation. The both-key action is a separate custom command.

## Encoder

| Control | USB HID Consumer Control |
| --- | --- |
| Clockwise | Volume Up |
| Counter-clockwise | Volume Down |
| Press | Mute / Unmute |

Encoder events are sent directly to macOS as USB HID Consumer Control and do not pass through Raycast.

## Angle

| Position | Action |
| --- | --- |
| Left | Auto-scroll Up |
| Center | Stop scrolling |
| Right | Auto-scroll Down |

Angle scrolling is sent directly to macOS as USB HID mouse-wheel events and does not pass through Raycast.

The Angle center is calibrated during startup. Hysteresis uses separate start and stop thresholds so small movements around center do not repeatedly start scrolling. Beyond the start threshold, scroll speed follows an x^1.5 curve and increases with distance from center.

Keep the Angle physically centered while the controller starts.

## ToF Proximity Input

Chain ToF runs in SINGLE mode with a 33 ms measurement time. Firmware waits for both `STOP` status and the completion flag, reads the distance, then explicitly starts the next SINGLE measurement. CONTINUOUS mode is not used.

Valid distance is smoothed with 75% of the previous value and 25% of the new value, then mapped from 500 mm (far) to 100 mm (near) into a quantized, additionally smoothed proximity value from 0 to 1. Invalid data or data older than 500 ms makes the ambient value ease back toward zero.

ToF is only a continuous parameter for the Matrix ambient light. It does not generate an event and never calls USB HID Keyboard, Consumer Control, or Mouse output.

## RGB Matrix

The Matrix does not show distance as discrete steps. In ambient mode, ToF proximity gently increases brightness, breathing speed, glow spread, and a subtle shimmer. With invalid or far readings it returns to the quiet idle ambient. The latest control action temporarily replaces the ambient frame:

| Action | Matrix feedback |
| --- | --- |
| ORA4 selection | Red center-to-edge ripple |
| Studio Display selection | Yellow center-to-edge ripple |
| DualKey both | Short white flash, then contraction |
| Volume Up | Purple expansion |
| Volume Down | Purple contraction |
| Mute | Purple pulse |
| Scroll Up | Blue movement from bottom to top |
| Scroll Down | Blue movement from top to bottom |

Animations use `millis()` and never delay the main loop. Action feedback always has priority over proximity ambient; proximity continues to settle internally and becomes visible only after the action ends. Full-frame transfers are limited to one per 80 ms, unchanged frames are skipped, and the cache advances only after both the Chain API and operation status succeed. The Matrix hardware brightness is compile-time limited to 50%; the base ambient RGB level is 5-12%, proximity adds at most 8% plus subtle shimmer, and 50% pixel peaks are brief.

## LED Status

| Internal state | LED display |
| --- | --- |
| Audio output unknown | Both DualKey LEDs remain at dim standby |
| ORA4 selected | Left DualKey LED breathes red (`255, 40, 40`); right stays at standby |
| Studio Display selected | Right DualKey LED breathes yellow (`255, 220, 0`); left stays at standby |
| Locally tracked mute on | Encoder LED bright purple (`170, 40, 255`) |
| Locally tracked mute off | Encoder LED remains at dim purple standby |
| Angle centered and stopped | Angle LED remains at dim blue standby |
| Angle active | Angle LED bright blue (`40, 140, 255`), with brightness following the operation amount |

The selected output and mute LEDs use a 1/f-like breathing animation with fixed hues. The Angle LED does not breathe: it responds directly to the normalized operation amount and returns to standby after reaching center and stopping.

At startup, the LEDs run in this order: left DualKey red, right DualKey yellow, Encoder purple, Angle blue, then Chain RGB. Runtime state and ambient rendering begin after that sequence. The Chain ToF LED does not participate.

At startup, audio output is explicitly `UNKNOWN`; no output is assumed and both DualKey LEDs remain at standby. A direct left or right selection establishes the internal output state. The both-key custom action and ToF proximity do not change the tracked audio output.

The local mute tracker starts off and toggles whenever the Encoder mute command is sent, so the first press changes the purple LED from standby to breathing state. Neither output nor mute state is read back from macOS. Operations performed outside this controller can therefore make the LEDs disagree with macOS; use a direct output key to resynchronize the output indicator, and treat the mute LED as a local operation indicator.
