# 🕹️ Arduino Arcade — Snake & Pac-Man

<img width="921" height="1302" alt="WhatsApp Image 2026-06-05 at 4 10 02 PM" src="https://github.com/user-attachments/assets/c560a428-09f9-465e-888b-604a465101c0" />

<img width="973" height="1324" alt="WhatsApp Image 2026-06-05 at 4 10 03 PM" src="https://github.com/user-attachments/assets/7a971681-825a-4fa6-b3fc-b551a59747c0" />



A dual-game arcade console running on an **Arduino Uno** with an **ST7789 240×320 TFT display** and a **KY-023 analog joystick**. Both games run from a shared boot menu and are written in C++ (with the Snake logic backed by AVR assembly for performance).

---

## 📋 Table of Contents

- [Features](#-features)
- [Hardware Requirements](#-hardware-requirements)
- [Circuit & Wiring](#-circuit--wiring)
- [Software & Libraries](#-software--libraries)
- [Project Structure](#-project-structure)
- [How to Build & Upload](#-how-to-build--upload)
- [Gameplay](#-gameplay)
  - [Boot Menu](#boot-menu)
  - [Snake](#snake)
  - [Pac-Man](#pac-man)
- [Configuration & Tuning](#-configuration--tuning)
- [Architecture Notes](#-architecture-notes)
- [Troubleshooting](#-troubleshooting)
- [License](#-license)

---

## ✨ Features

| Feature | Detail |
|---|---|
| Boot menu | Joystick UP/DOWN to highlight, button to launch |
| Snake | Assembly-backed game logic, 8-fps smooth scroll |
| Pac-Man | 28×31 tile maze, 4 coloured ghosts, power-freeze pellets |
| Mid-game exit | Press joystick button at any time to return to menu |
| Score HUD | Live score display; lives shown as yellow discs |
| Shared hardware | One joystick and one display drive both games |

---

## 🔧 Hardware Requirements

| Component | Specification | Qty |
|---|---|---|
| Arduino Uno | ATmega328P, 5 V | 1 |
| ST7789 TFT LCD | 240 × 320 px, SPI, 3.3 V logic (most modules have on-board level shifter) | 1 |
| KY-023 Joystick | Dual-axis analog + push button | 1 |
| Jumper wires | Male-to-male or Male-to-female depending on your modules | ~14 |
| Breadboard | Optional — useful for clean GND/VCC distribution | 1 |
| USB cable | Type-A to Type-B for programming the Uno | 1 |

> **Voltage note:** The ST7789 data lines run at 3.3 V logic on bare modules. Most breakout boards sold as "Arduino compatible" include an onboard 3.3 V regulator and level-shifter, allowing you to drive SPI directly from the Uno's 5 V GPIO without any extra components. Check your specific module's datasheet if you are using a bare chip.

---

## 🔌 Circuit & Wiring

### Schematic Diagram

```
                         ARDUINO UNO
                    ┌──────────────────────┐
                    │                      │
   JOYSTICK         │  DIGITAL             │
   ┌─────────┐      │  ┌────┐              │
   │  VCC ───┼──────┼──┤ 5V │              │
   │  GND ───┼──────┼──┤GND │              │
   │  VRX ───┼──────┼──┤ A0 │  ANALOG IN  │
   │  VRY ───┼──────┼──┤ A1 │              │
   │  SW  ───┼──────┼──┤ D2 │ (INPUT_PULLUP)
   └─────────┘      │  └────┘              │
                    │                      │
                    │  SPI / DIGITAL       │
                    │  ┌────┐              │
                    │  │ D13├──────────────┼──── SCK  ──┐
                    │  │ D11├──────────────┼──── MOSI ──┤
                    │  │ D10├──────────────┼──── CS   ──┤  ST7789
                    │  │  D9├──────────────┼──── DC   ──┤  TFT LCD
                    │  │  D8├──────────────┼──── RST  ──┤  240×320
                    │  │  5V├──────────────┼──── VCC  ──┤
                    │  │ GND├──────────────┼──── GND  ──┘
                    │  └────┘              │
                    └──────────────────────┘
```

### Pin-by-Pin Reference Table

#### ST7789 TFT Display

| TFT Pin | Arduino Pin | Signal | Notes |
|---|---|---|---|
| VCC | 5 V | Power | Module regulator steps down to 3.3 V |
| GND | GND | Ground | |
| SCK | D13 | SPI Clock | Hardware SPI SCK |
| MOSI / SDA | D11 | SPI Data | Hardware SPI MOSI |
| CS | D10 | Chip Select | Active LOW |
| DC / RS | D9 | Data / Command | HIGH = data, LOW = command |
| RST | D8 | Reset | Active LOW |

#### KY-023 Analog Joystick

| Joystick Pin | Arduino Pin | Signal | Notes |
|---|---|---|---|
| VCC | 5 V | Power | |
| GND | GND | Ground | |
| VRX | A0 | X-axis analog | Left < 300, Centre ≈ 512, Right > 724 |
| VRY | A1 | Y-axis analog | Up < 300, Centre ≈ 512, Down > 724 |
| SW | D2 | Button | INPUT_PULLUP — LOW when pressed |

### Wiring Tips

- Keep SPI wires (D10-D13) as short as possible to reduce noise at high clock speeds.
- The joystick button (SW) uses the Uno's internal pull-up resistor via `INPUT_PULLUP` — no external resistor is needed.
- If your display flickers, add a 100 nF ceramic decoupling capacitor between the TFT's VCC and GND pins, placed as close to the module as possible.
- If sharing a breadboard power rail, make sure GND is a single common ground across the Uno, display, and joystick.

---

## 💾 Software & Libraries

### Arduino IDE / arduino-cli

Install the following libraries via **Sketch → Include Library → Manage Libraries** (or `arduino-cli lib install`):

| Library | Author | Purpose |
|---|---|---|
| `Adafruit ST7789` | Adafruit | ST7789 TFT driver |
| `Adafruit GFX Library` | Adafruit | Graphics primitives (circles, rectangles, text) |

Both are available directly in the Arduino Library Manager. `Adafruit GFX` is installed automatically as a dependency of `Adafruit ST7789`.

### AVR-GCC (bundled with Arduino IDE)

No separate installation needed — the IDE ships with `avr-gcc` which compiles both the C++ and the `.S` assembly file.

---

## 📁 Project Structure

```
YourSketchFolder/
├── SnakeGame.cpp       ← Main source: menu, Snake, Pac-Man
├── snake_logic.S       ← AVR assembly: Snake game tick & reset
├── Snake_asm.h         ← C header exposing assembly symbols to C++
└── README.md           ← This file
```

All three source files must live in the **same sketch folder**. The Arduino IDE and `arduino-cli` automatically compile every `.cpp`, `.c`, and `.S` file found in the sketch directory.

### Key Symbols Exported by `snake_logic.S`

| Symbol | Type | Description |
|---|---|---|
| `asm_reset_game()` | Function | Initialises snake, food, score, and direction |
| `asm_game_tick()` | Function | Advances one game frame; reads joystick A0/A1 |
| `snake_x[]` / `snake_y[]` | Arrays | Ring-buffer of snake segment tile positions |
| `snake_len` | `uint8_t` | Current snake length |
| `food_x` / `food_y` | `uint8_t` | Current food tile position |
| `game_score` | `uint16_t` | Running score |
| `game_state` | `uint8_t` | `0` = playing, `1` = game over |
| `req_erase_tail` | `uint8_t` | Set by ASM when tail tile must be cleared |
| `req_draw_cell` | `uint8_t` | Set by ASM when a cell must be drawn |
| `req_spawn_food` | `uint8_t` | Set by ASM when new food has been placed |
| `req_show_score` | `uint8_t` | Set by ASM when score display must update |

---

## 🚀 How to Build & Upload

### Using Arduino IDE (1.8 or 2.x)

1. Create a new sketch and name it `SnakeGame`.
2. Copy `SnakeGame.cpp`, `snake_logic.S`, and `Snake_asm.h` into the sketch folder (shown in the IDE title bar).
3. Select **Tools → Board → Arduino Uno**.
4. Select the correct **Tools → Port** (e.g. `COM3` on Windows or `/dev/ttyUSB0` on Linux).
5. Click **Upload** (→ arrow button).

### Using arduino-cli

```bash
# Install core and libraries (first time only)
arduino-cli core install arduino:avr
arduino-cli lib install "Adafruit ST7789" "Adafruit GFX Library"

# Compile
arduino-cli compile --fqbn arduino:avr:uno YourSketchFolder/

# Upload (replace /dev/ttyUSB0 with your port)
arduino-cli upload --fqbn arduino:avr:uno --port /dev/ttyUSB0 YourSketchFolder/
```

### Expected Flash & RAM Usage (approximate)

| Resource | Used | Available (Uno) |
|---|---|---|
| Flash (program storage) | ~22 KB | 32 KB |
| SRAM | ~1.1 KB | 2 KB |
| PROGMEM (maze walls, ghost colours) | ~120 B | — |

> The maze wall bitmasks and ghost colour table are stored in flash (`PROGMEM`) to preserve the limited 2 KB SRAM on the ATmega328P.

---

## 🎮 Gameplay

### Boot Menu

When the device powers on you are greeted by the **ARCADE GAMES** boot menu.

```
  ╔══════════════════╗
  ║   ARCADE GAMES   ║
  ╠══════════════════╣
  ║  1. SNAKE        ║   ← highlighted in green
  ║  2. PAC-MAN      ║
  ╠══════════════════╣
  ║ Joystick UP/DOWN ║
  ║  Press to start  ║
  ╚══════════════════╝
```

| Action | Control |
|---|---|
| Move highlight UP | Push joystick UP (VRY < 400) |
| Move highlight DOWN | Push joystick DOWN (VRY > 624) |
| Launch selected game | Press joystick button (SW) |

Press the joystick button **at any time during a game** to return to this menu.

---

### Snake

A classic Snake game with assembly-optimised game logic running at approximately 8 fps.

#### Controls

| Action | Joystick |
|---|---|
| Move UP | Push UP |
| Move DOWN | Push DOWN |
| Move LEFT | Push LEFT |
| Move RIGHT | Push RIGHT |
| Return to menu | Press button |

#### Rules

- The snake starts at the centre of the grid moving right.
- Eating the **orange food pellet** grows the snake by one segment and adds points.
- Hitting a **wall** or the **snake's own body** ends the game.
- Score is displayed in the top-left corner.
- On game over, press the button to return to the menu.

#### Grid

| Property | Value |
|---|---|
| Display area | 240 × 320 px |
| Tile size | 8 × 8 px |
| Grid dimensions | 30 × 40 tiles |
| Frame rate | ~8 fps (120 ms delay) |

---

### Pac-Man

A faithful Pac-Man-style game with a 28-column × 31-row tile maze, four coloured ghosts, normal pellets, and power-freeze pellets.

#### Controls

| Action | Joystick |
|---|---|
| Move LEFT | Push LEFT |
| Move RIGHT | Push RIGHT |
| Move UP | Push UP |
| Move DOWN | Push DOWN |
| Return to menu | Press button |

> Pac-Man **always keeps moving** in his current direction. The joystick only queues the next turn — it is applied automatically as soon as the corridor opens, exactly like the original arcade game.

#### Maze Layout

```
  Col:  0         9  13 14         18        27
        │         │   │  │          │         │
Row 0:  ██████████████████████████████████████  ← top border
Row 1:  █ · · · · · · · · · · · · · · · · · █  ← open corridor + power pellets at corners
Row 5:  █ ·──· ·──· ·─────────────· ·──· ·──█  ← horizontal shelf
Row 8:  █ · · · · · · · · · · · · · · · · · █  ← open corridor
       ...
Row 12: █ · · · · · · · · · · · · · · · · · █  ← pen exit row (open)
Row 13: █ ·──·  ┌──────────────────┐  ·──· ·█
Row 14: █ · · · │  Ghost  Pen      │  · · · █  ← pen interior (no dots)
Row 15: █ · · · │  (open interior) │  · · · █
Row 16: █ ·──·  └──────────────────┘  ·──· ·█
Row 17: █ · · · · · · · · · · · · · · · · · █  ← open corridor
       ...
Row 23: █ · · · · ·[PAC-MAN START]· · · · · █
       ...
Row 30: ██████████████████████████████████████  ← bottom border
```

- **Vertical corridors** run at columns 1, 6, 9, 13, 14, 18, 21, 26 — open from top to bottom.
- **Horizontal corridors** (open rows) are at rows 1, 5, 8, 12, 17, 23, 26, 29 — open across the full width.
- Everything else is a **shelf wall**.

#### Scoring

| Event | Points |
|---|---|
| Eat a normal pellet | +10 |
| Eat a power-freeze pellet | +50 |
| Complete all pellets | Win! |

#### Lives

Pac-Man starts with **3 lives**, shown as small yellow discs in the top-right of the HUD. A life is lost whenever an active (non-frozen) ghost touches Pac-Man.

#### The 4 Ghosts

| Ghost | Colour | Personality |
|---|---|---|
| Blinky | 🔴 Red | Chases Pac-Man directly (shortest Manhattan path) |
| Pinky | 🩷 Pink | Same algorithm, different spawn — creates pincer with Blinky |
| Inky | 🩵 Cyan | Same algorithm, different spawn — fills mid-maze |
| Clyde | 🟠 Orange | Same algorithm, different spawn — covers right side |

All four ghosts use a **Manhattan-distance minimising** pathfinder. They cannot reverse direction (no 180° U-turns), which keeps their movement natural and predictable.

Ghost spawn positions (inside the pen):

| Ghost | Start column | Start row |
|---|---|---|
| Blinky | 11 | 14 |
| Pinky | 13 | 13 |
| Inky | 14 | 16 |
| Clyde | 16 | 14 |

#### Power-Freeze Pellet

The **4 large white pellets** are located at the four near-corner tiles `(1,1)`, `(26,1)`, `(1,29)`, `(26,29)`.

| Normal ghost behaviour | Frozen ghost behaviour |
|---|---|
| Chases Pac-Man | Stops moving completely |
| Kills Pac-Man on touch | Completely harmless — touching does nothing |
| Normal colour | Turns **cyan-blue** |
| — | Unfreezes after 40 game ticks (~8.8 seconds) |

> There is **no eat-the-ghost / return-to-base mechanic**. Frozen ghosts simply wait in place and resume chasing when the freeze expires.

#### Win / Lose

| Condition | Result |
|---|---|
| All pellets eaten | **YOU WIN!** screen — score shown |
| 0 lives remaining | **GAME OVER** screen — score shown |
| Either screen | Press button → return to menu |

---

## ⚙️ Configuration & Tuning

All tunable constants are `#define`s near the top of `SnakeGame.cpp`:

### Snake

| Constant | Default | Effect |
|---|---|---|
| `delay(120)` in `handleSnake()` | 120 ms | Game speed — lower = faster snake |

### Pac-Man

| Constant | Default | Effect |
|---|---|---|
| `PM_TICK_MS` | `220` | Milliseconds per game tick — raise to slow everything down |
| `PM_FREEZE_TICKS` | `40` | Ticks ghosts stay frozen after a power pellet |

### Joystick Dead-Zone

The analogue dead-zone is hard-coded in `pm_readJoystick()`:

```cpp
if      (vx < 300) { /* left  */ }
else if (vx > 724) { /* right */ }
else if (vy < 300) { /* up    */ }
else if (vy > 724) { /* down  */ }
```

Adjust the thresholds `300` and `724` if your joystick reads off-centre at rest (check with `Serial.println(analogRead(A0))` in `setup()`).

---

## 🏗️ Architecture Notes

### Memory Strategy

The ATmega328P has only **2 KB of SRAM**. To conserve it:

- The maze wall bitmasks (`pm_mazeWalls[28]`, 112 bytes) are stored in **flash** via `PROGMEM` and read back with `pgm_read_dword()`.
- Ghost colours (`ghostColor[4]`, 8 bytes) are also in `PROGMEM`.
- String literals use the `F()` macro (`F("GAME OVER")`) to keep them in flash instead of copying to SRAM at boot.
- Pellet state is packed into `uint32_t pm_dots[31]` — one bit per column per row (31 × 4 = 124 bytes total).

### Rendering Strategy

Rather than redrawing the full screen every frame (which would take ~150 ms on the Uno at 8 MHz SPI), only **changed tiles** are updated each tick:

1. **Erase** Pac-Man and each ghost by calling `pm_restoreTile()` — this redraws the wall outline, dot, or black background that was underneath the sprite, so dots are never accidentally erased.
2. **Move** all entities.
3. **Draw** Pac-Man and all ghosts in their new positions.

This keeps the per-frame SPI transfer count to roughly 10–20 filled rectangles instead of 868 (the full maze).

### Collision Detection

Ghost–Pac-Man collision uses a **pixel bounding-box overlap test**:

```cpp
int16_t dx = pm.px - gh[i].px;   // pixel distance X
int16_t dy = pm.py - gh[i].py;   // pixel distance Y
if (abs(dx) < PM_TILE && abs(dy) < PM_TILE) { /* hit */ }
```

This fires reliably regardless of tile-snap timing and is cheaper than any distance formula.

### Ghost AI

Each ghost evaluates all four cardinal directions every tick, discards its opposite direction (no U-turns), discards wall tiles, then picks the direction with the **minimum Manhattan distance** to Pac-Man's current tile. Frozen ghosts skip the movement step entirely but still tick their freeze counter.

---

## 🛠️ Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| Blank / white screen | SPI wiring error or wrong pin order | Double-check D8/D9/D10/D11/D13 against the wiring table |
| Display shows garbage | CS or DC pin swapped | Verify D10=CS and D9=DC |
| Joystick moves the wrong direction | Axis wiring swapped | Swap A0 and A1, or swap VRX/VRY |
| Left/right reversed | Joystick polarity | Swap the signs in `pm_readJoystick()` or physically rotate the joystick 180° |
| Button never registers | SW not wired to D2 | Check SW → D2; the code uses `INPUT_PULLUP` so no resistor needed |
| Ghosts don't move | Ghost spawn tiles are walls | Ensure `snake_logic.S` / maze bitmasks are unmodified |
| Dots not visible | `pm_initDots()` not called or SRAM full | Check Serial monitor for reset loops; reduce other globals |
| Sketch won't compile | Missing library | Install `Adafruit ST7789` and `Adafruit GFX` via Library Manager |
| Low SRAM warning | Too many global variables | Move large arrays to `PROGMEM`; avoid `String` objects |
| Game resets randomly | SRAM overflow (stack collision) | Check free RAM with `SP - __bss_end`; reduce global state |

### Checking Free RAM at Runtime

Add this helper and call it from `setup()` to print available SRAM over Serial:

```cpp
int freeRam(){
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
// In setup():
Serial.print(F("Free RAM: ")); Serial.println(freeRam());
```

---

## 📄 License

This project is released under the **MIT License**. You are free to use, modify, and distribute it for personal or commercial purposes with attribution.

```
MIT License
Copyright (c) 2025

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

---

*Built with ❤️ on an Arduino Uno. No HDMI required.*![Uploading WhatsApp Image 2026-06-05 at 4.10.02 PM.jpeg…]()
