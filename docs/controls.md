# Controls and LED Behavior

Japanese: [controls.ja.md](controls.ja.md)

The orientation with the USB-C port on the rear side is treated as the front. From this front view, the physical layout from left to right is DualKey, Encoder, then Angle. All left/right controls and LED positions below are described from this viewing direction.

## DualKey

| Control | HID shortcut | Expected Raycast action |
| --- | --- | --- |
| Left key | `Ctrl + Cmd + 1` | Select ORA4 |
| Right key | `Ctrl + Cmd + 2` | Select Studio Display |
| Both keys | `Ctrl + Option + S` | Toggle ORA4 / Studio Display |

The two-key chord has priority. A short chord window prevents a near-simultaneous press from producing a single-key action first.

These are the only controls that use Raycast.

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

## LED Status

| Internal state | LED display |
| --- | --- |
| Audio output unknown | Both DualKey LEDs off |
| ORA4 selected | Left DualKey LED bright red (`255, 40, 40`) |
| Studio Display selected | Right DualKey LED bright yellow (`255, 220, 0`) |
| Locally tracked mute on | Encoder LED bright purple (`170, 40, 255`) |
| Locally tracked mute off | Encoder LED off |
| Angle centered and stopped | Angle LED off |
| Angle active | Angle LED bright blue (`40, 140, 255`), with brightness following the operation amount |

The selected output and mute LEDs use a 1/f-like breathing animation with fixed hues. The Angle LED does not breathe: it responds directly to the normalized operation amount and fades out over approximately 150 ms after returning to center and stopping.

At startup, the LEDs run in this order: left DualKey red, right DualKey yellow, Encoder purple, and Angle blue. All four then light together near maximum brightness for approximately 250 ms as the READY indication and fade out together while preserving their four colors.

At startup, audio output is explicitly `UNKNOWN`; no output is assumed and both DualKey LEDs remain off. A direct left or right selection establishes the internal output state. A both-key toggle reverses a known internal state. If the first audio action after startup is the toggle shortcut, the result cannot be inferred, so the state remains unknown and both LEDs stay off until a direct selection.

The local mute tracker starts off and toggles whenever the Encoder mute command is sent, so the first press lights the purple LED. Neither output nor mute state is read back from macOS. Operations performed outside this controller can therefore make the LEDs disagree with macOS; use a direct output key to resynchronize the output indicator, and treat the mute LED as a local operation indicator.
