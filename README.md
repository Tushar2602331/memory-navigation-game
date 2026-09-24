# Memory + Navigation Game (Arduino, 8x8 LED Matrix, Joystick)

A hardware memory game for Arduino Uno. A random target cell flashes briefly
on an 8x8 LED matrix; it then disappears and you must navigate a "ball" LED
from the top-left corner to the remembered target using a joystick, then
submit your guess with the joystick's click button. Correct guesses get
3 beeps; wrong guesses show a capital **L**.

▶️ **[Run the simulation on Wokwi](#running-the-simulation-on-wokwi)** — no
hardware required.

## How it works

1. A random `(targetX, targetY)` cell lights up and a beep plays. It stays
   lit for ~4 seconds (memorization phase).
2. The matrix clears. A ball LED appears at `(0,0)`.
3. Move the ball with the joystick — one push = one cell, in any of the 4
   directions. The ball can't leave the 8x8 grid.
4. Click the joystick to submit your guess.
5. **Correct** → 3 distinct beeps. **Wrong** → the matrix shows `L`.
6. After a short pause, a new round starts automatically.

Coordinate system: `(0,0)` is the top-left LED. X increases to the right,
Y increases downward.

## Repository contents

| File | Purpose |
|---|---|
| `sketch.ino` | Complete Arduino sketch (game logic, matrix multiplexing, joystick + buzzer handling) |
| `diagram.json` | Wokwi circuit: Arduino Uno + a hand-wired 8x8 LED matrix (64 individual LEDs + 8 row resistors, wired exactly like a real raw matrix) + joystick + buzzer |
| `wokwi.toml` | Optional config for running the simulation from the command line via `wokwi-cli` / the Wokwi VS Code extension |

## Running the simulation on Wokwi

The easiest way (no install):

1. Go to [wokwi.com](https://wokwi.com) and start a **New Project → Arduino Uno**.
2. Open the project's `sketch.ino` tab and replace its contents with this
   repo's [`sketch.ino`](./sketch.ino).
3. Open the `diagram.json` tab (⋮ menu → *diagram.json*, or the file list on
   the left) and replace its contents with this repo's
   [`diagram.json`](./diagram.json).
4. Click the green ▶ **Play** button.
5. Click the joystick to focus it, then use your keyboard arrow keys to move
   the ball and the space bar to click/submit.

Alternatively, if you have the [Wokwi CLI](https://docs.wokwi.com/wokwi-ci/cli-usage)
or the [Wokwi for VS Code](https://marketplace.visualstudio.com/items?itemName=Wokwi.wokwi-vscode)
extension installed, you can open this folder directly and run the
simulation from your editor (see `wokwi.toml`).

## Real hardware notes

The simulation's `diagram.json` wires 64 individual LEDs exactly the way a
real raw (driver-less) 8x8 matrix is wired internally — 8 row lines (each
through a 220Ω current-limiting resistor) and 8 column lines — so the same
multiplexing code in `sketch.ino` behaves the same way on real hardware.

If you're building this on a breadboard:

- **Confirm your matrix's actual pinout first.** Raw 8x8 matrices don't
  share one universal 16-pin layout across manufacturers — see the pin
  identification method in the "Testing procedure" section below.
- **Add 8 current-limiting resistors** (~220–330Ω), one per row (or
  column) line, to protect the LEDs and the Arduino's pins.
- A raw matrix + analog joystick uses all 20 of the Uno's I/O pins,
  including D0/D1 (normally the USB serial pins) for the joystick button
  and the buzzer. This can occasionally interfere with re-uploading
  sketches — if so, briefly disconnect those two wires, upload, then
  reconnect.
- `ROW_ACTIVE_HIGH` / `COL_ACTIVE_LOW` at the top of `sketch.ino` assume
  rows are anodes and columns are cathodes. Flip either constant if your
  matrix is wired the opposite way.

### Real-hardware pin map

| Component | Pin | Arduino Pin |
|---|---|---|
| Matrix Row 0 (top) … Row 7 | R0…R7 | D2, D3, D4, D5, D6, D7, D8, D9 |
| Matrix Col 0 (left) … Col 7 | C0…C7 | D10, D11, D12, D13, A0, A1, A2, A3 |
| Joystick VRx | VRx | A4 |
| Joystick VRy | VRy | A5 |
| Joystick SW | SW | D0 (RX) |
| Buzzer + | — | D1 (TX) |
| Buzzer − | — | GND |

### Testing procedure

1. **Map the matrix pins** — use a multimeter (diode mode) or a coin cell +
   resistor to find which physical pin is which row/column before wiring.
2. **Test one LED at a time** to confirm `(0,0)` really lights the top-left
   LED, not a mirrored/rotated position.
3. **Test the buzzer alone** with `tone(1, 1000, 500);`.
4. **Test joystick readings** (temporarily free D0/D1 to use `Serial`) to
   confirm the center reading and full-deflection range.
5. **Test target generation**, memorization timing, and that the matrix
   clears completely afterward.
6. **Test movement** in all 4 directions and edge clamping.
7. **Test a correct guess** (3 beeps) and **a wrong guess** (recognizable
   `L`, non-mirrored).

### Troubleshooting

| Problem | Likely cause / fix |
|---|---|
| Matrix reversed / mirrored / rotated | `rowPins[]`/`colPins[]` order doesn't match your physical wiring — reorder them |
| Nothing lights, or lights randomly | `ROW_ACTIVE_HIGH`/`COL_ACTIVE_LOW` guess is backwards — flip one or both |
| Joystick moves multiple cells per push | Increase `JOY_DEAD_ZONE` / `MOVE_COOLDOWN` |
| Ball drifts on its own | Re-check joystick center calibration, or increase `JOY_DEAD_ZONE` |
| Button triggers multiple times | Increase `BUTTON_DEBOUNCE` |
| Buzzer silent | Check wiring/polarity, test in isolation |
| Sketch won't upload | Disconnect D0/D1 wires, upload, then reconnect |

## Possible improvements

- Drive the matrix through a 74HC595 shift register to free up D0/D1 for
  normal serial debugging.
- Add a score/streak counter.
- Scale difficulty (shorter memorization time, multiple targets).
- Replace the auto-restart with "click to start next round."

## License

MIT — see [LICENSE](./LICENSE).
