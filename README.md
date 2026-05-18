<img width="476" height="848" alt="snake1" src="https://github.com/user-attachments/assets/0a8ec82e-0a3a-4a49-8075-1aec36271ce4" />

# 🐍 AVR Assembly Snake Game — Arduino Uno + ST7789 TFT

A fully playable Snake game running on an **Arduino Uno**, written in **AVR Assembly** for all game logic, with a thin C++ driver layer for display rendering. Controls are handled via an **analog joystick** and the game is displayed on a **240×320 ST7789 TFT LCD** over SPI.

---

## 📋 Table of Contents

- [Features](#-features)
- [Hardware Required](#-hardware-required)
- [Circuit Diagram](#-circuit-diagram)
- [Wiring Reference](#-wiring-reference)
- [Project Structure](#-project-structure)
- [File-by-File Breakdown](#-file-by-file-breakdown)
- [How the Game Works](#-how-the-game-works)
- [Memory Layout](#-memory-layout)
- [Assembly Architecture](#-assembly-architecture)
- [Building & Flashing](#-building--flashing)
- [Configuration & Tuning](#-configuration--tuning)

---

## ✨ Features

- **100% game logic in AVR Assembly** — movement, collision, food, scoring, RNG
- **Smooth rendering** — only changed cells are redrawn each frame (no full-screen refresh)
- **Full 10-bit joystick ADC** with dominant-axis selection and dead-zone
- **180° reversal prevention** — can't reverse directly into yourself
- **Self-collision detection** — game over on body hit
- **Wall collision detection** — game over on border hit
- **Galois LFSR random number generator** seeded from Timer1 entropy
- **Growing snake** — tail stays on food pickup, no teleport glitch
- **Score HUD** — live score display, updates on every food pickup
- **Auto-restart** — 3-second game-over screen then automatic reset

---

## 🛒 Hardware Required

| Component | Details |
|-----------|---------|
| Arduino Uno | ATmega328P, 16 MHz |
| ST7789 TFT LCD | 240×320 pixels, SPI interface |
| Analog Joystick Module | KY-023 or equivalent, dual-axis + button |
| Jumper Wires | Male-to-male / male-to-female |
| Breadboard (optional) | For cleaner wiring |

---

## 🔌 Circuit Diagram

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
   │  SW  ───┼──────┼──┤ D2 │ (optional)  │
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

> ⚠️ **Note:** Some ST7789 modules are **3.3 V logic only**. If your module has a voltage regulator and logic level shifter on-board it can accept 5 V directly. If not, use a logic-level converter on SCK, MOSI, CS, DC, and RST, and connect VCC to the 3.3 V pin instead of 5 V.

---

## 🔧 Wiring Reference

### Joystick → Arduino Uno

| Joystick Pin | Arduino Uno Pin | Notes |
|:---:|:---:|---|
| VCC | 5V | Power |
| GND | GND | Ground |
| VRX | A0 | X-axis analog |
| VRY | A1 | Y-axis analog |
| SW | D2 | Button (optional, not used in game logic) |

### ST7789 TFT LCD → Arduino Uno

| LCD Pin | Arduino Uno Pin | Notes |
|:---:|:---:|---|
| VCC | 5V | Power (check your module's voltage!) |
| GND | GND | Ground |
| SCK | D13 | SPI Clock (hardware SPI) |
| MOSI / SDA | D11 | SPI Data (hardware SPI) |
| CS | D10 | Chip Select |
| DC / RS | D9 | Data / Command select |
| RESET / RES | D8 | Hardware reset |

---

## 📁 Project Structure

```
SnakeGame/
├── SnakeGame.cpp      ← C++ driver: display, setup(), loop()
├── Snake_asm.h        ← Shared header: extern declarations for all
│                         variables and functions defined in .S
└── snake_logic.S      ← AVR Assembly: ALL game logic
```

All three files live in the **same sketch folder**. The Arduino IDE / `arduino-cli` automatically compiles `.S` files alongside `.cpp` files in the same directory.

---

## 📄 File-by-File Breakdown

---

### `Snake_asm.h` — Shared Interface Header

The bridge between the C++ driver and the assembly game engine.

Declares everything that the assembly `.S` file **defines** so the C++ compiler knows how to link against them:

| Declaration | Type | Purpose |
|---|---|---|
| `asm_reset_game()` | `void` function | Initialise / restart game state |
| `asm_game_tick()` | `void` function | Advance one frame |
| `asm_rand()` | `uint16_t` function | LFSR random number |
| `req_draw_cell_x/y` | `volatile uint8_t` | Coordinates for next cell draw |
| `req_draw_cell_color` | `volatile uint16_t` | RGB-565 colour for cell draw |
| `req_draw_cell` | `volatile uint8_t` | Flag: C must draw a cell |
| `req_erase_tail` | `volatile uint8_t` | Flag: C must erase old tail |
| `req_erase_tail_x/y` | `volatile uint8_t` | Tail cell coordinates to erase |
| `req_spawn_food` | `volatile uint8_t` | Flag: C must draw new food |
| `req_game_over` | `volatile uint8_t` | Flag: game ended |
| `req_show_score` | `volatile uint8_t` | Flag: C must refresh score HUD |
| `game_score` | `volatile int16_t` | Current score |
| `game_state` | `volatile uint8_t` | 0 = playing, 1 = game over |
| `snake_x[300]` | `volatile uint8_t[]` | X coords of every snake segment |
| `snake_y[300]` | `volatile uint8_t[]` | Y coords of every snake segment |
| `snake_len` | `volatile uint8_t` | Current snake length |
| `food_x`, `food_y` | `volatile uint8_t` | Food pellet grid position |
| `rand_state` | `volatile uint16_t` | LFSR internal state |

All variables are declared `volatile` because the assembly writes them asynchronously from the C++ perspective.

---

### `SnakeGame.cpp` — C++ Driver

Handles everything that requires the **Adafruit library** (hardware SPI, pixel drawing). The assembly cannot call C++ library functions, so a request-flag protocol is used: assembly sets flags, C++ services them after each tick.

#### `setup()`
1. Initialises the ST7789 display in portrait mode (240×320, no rotation)
2. Configures ADC: AVcc reference, prescaler 128 (~9.6 kHz sample rate)
3. Starts Timer1 free-running — provides entropy for the LFSR seed
4. Calls `asm_reset_game()` to initialise all game state in assembly
5. Draws the HUD label ("Score:") and initial score
6. Draws the starting 3-segment snake and first food pellet

#### `loop()`
Runs at ~8 fps (120 ms delay per frame):

```
loop() each tick:
  ├─ game_state == 1?  → drawGameOver(), delay 3s, resetAndRedraw(), return
  ├─ asm_game_tick()   ← all game logic runs here (assembly)
  ├─ req_erase_tail?   → drawCell(tail, BLACK)
  ├─ req_draw_cell?    → drawCell(head, GREEN)
  ├─ req_spawn_food?   → drawCell(food, ORANGE)
  └─ req_show_score?   → refresh score area on HUD
```

#### Helper Functions

| Function | Purpose |
|---|---|
| `drawCell(x, y, color)` | Fill one 7×7 pixel grid cell (with 1 px gap) |
| `drawBorder()` | White rectangle around the play field |
| `drawScore()` | Erase and redraw the score number on the HUD |
| `drawInitialSnake()` | Draw all segments after reset |
| `drawGameOver()` | Full-screen game-over message with final score |
| `resetAndRedraw()` | Clear screen + call `asm_reset_game()` + redraw board |

#### Grid Maths
- Screen: 240 × 320 px
- Cell size: 8 px → Grid: **30 × 40 cells**
- Cell pixel position: `x * 8 + 1`, `y * 8 + 1`, size `7 × 7`
- Border occupies row/column 0 and row/column 29/39

---

### `snake_logic.S` — AVR Assembly Game Engine

All game logic. No C library calls. Communicates with the C++ driver entirely through shared memory flags in the `.bss` section.

#### `.bss` Section — Shared Variables

All variables live in SRAM, zero-initialised at startup by the C runtime. Layout (in declaration order):

```
req_draw_cell_x      1 byte  ─┐
req_draw_cell_y      1 byte   │  Head draw request
req_draw_cell_color  2 bytes  │  (RGB-565, little-endian)
req_draw_cell        1 byte  ─┘

req_erase_tail       1 byte  ─┐
req_erase_tail_x     1 byte   │  Tail erase request
req_erase_tail_y     1 byte  ─┘

req_spawn_food       1 byte     New food draw request
req_game_over        1 byte     Game-over signal
req_show_score       1 byte     Score redraw signal

game_score           2 bytes    Unsigned 16-bit score
game_state           1 byte     0=playing, 1=game over

snake_x[300]         300 bytes  X coordinate of each segment
snake_y[300]         300 bytes  Y coordinate of each segment
snake_len            1 byte     Active length (max 298)

food_x               1 byte     Food grid X
food_y               1 byte     Food grid Y
dir_x                1 byte     X velocity: -1, 0, or +1
dir_y                1 byte     Y velocity: -1, 0, or +1
rand_state           2 bytes    16-bit LFSR state
```

Total BSS usage: **~616 bytes** out of 2048 bytes SRAM on the ATmega328P.

---

#### `asm_reset_game` — Game Initialisation

Called once from `setup()` and again after each game-over.

1. **Seeds the LFSR** by reading TCNT1L/TCNT1H (Timer1 counter). Since Timer1 starts free-running in `setup()` before this call, the timer value is different on every reset, giving unique food spawn positions each game. Falls back to `0xAC13` if the timer hasn't ticked yet.
2. **Clears all flags** — game_state, game_score, all req_* flags.
3. **Sets initial direction** — right (+X), so `dir_x = 1`, `dir_y = 0`.
4. **Places the snake** — length 3, horizontal, head at grid cell (16, 20):
   - `snake_x[0]=16, snake_x[1]=15, snake_x[2]=14`
   - `snake_y[0..2]=20` (all same row)
5. **Spawns first food** via `spawn_food_rand`.
6. **Sets `req_show_score`** so C redraws "0" on the HUD.

---

#### `asm_game_tick` — Main Game Loop (one frame)

Called every 120 ms from `loop()`. Full execution path per frame:

```
asm_game_tick:
  1. read_joystick          ← sample ADC, update dir_x/dir_y
  2. new_head = head + dir  ← compute candidate next position
  3. Wall check             ← new_head on border? → gameover
  4. Self check             ← new_head on any body segment? → gameover
  5. Food check:
     ├─ new_head == food?
     │   ├─ eat_food        ← grow, score++, shift body, spawn food
     │   └─ → draw_head
     └─ else:
         ├─ move_snake      ← erase tail, shift body
         └─ → draw_head
  6. draw_head:
     ├─ set req_draw_cell   ← tell C to draw head GREEN
     └─ write snake_x[0], snake_y[0] ← save new head position
```

---

#### `move_snake` — Normal Movement

Runs every frame when no food is eaten.

1. **Reads the last segment** (`snake_x/y[len-1]`) and stores its coordinates into `req_erase_tail_x/y`, sets `req_erase_tail = 1`. C will erase this cell BLACK.
2. **Shifts the body** forward: for `i` from `len-1` down to `1`, copies `snake_x/y[i-1]` → `snake_x/y[i]`. This moves every segment one step toward the tail.
3. The caller (`asm_game_tick`) then writes the new head into `snake_x[0]`/`snake_y[0]`.

---

#### `eat_food` — Food Consumption

Runs when the new head lands on the food cell.

1. **Saves r16/r17** (new head X/Y) onto the stack — critical because the subsequent random calls clobber those registers.
2. **Increments `snake_len`** — the tail naturally stays in place because `move_snake` is not called.
3. **Increments `game_score`** (16-bit, carries correctly), sets `req_show_score`.
4. **Shifts the body** exactly like `move_snake` but with NO tail erase — the last segment stays, making the snake visibly longer.
5. **Calls `spawn_food_rand`** to place new food.
6. **Restores r16/r17** from the stack so `draw_head` writes the correct position.

---

#### `spawn_food_rand` — Food Placement

Generates a random grid cell for the new food pellet.

1. Calls `asm_rand` → takes low byte → `mod8(value, GRID_W-2)` → adds 1 → stores in `food_x`. Result is in `[1 .. GRID_W-2]` (inside the border).
2. Same process for `food_y` using `GRID_H-2`.
3. Sets `req_spawn_food = 1` — C will draw the orange cell.

> **Note:** Food can currently spawn on a body segment. For short snakes this is rare. A future improvement would loop until a free cell is found.

---

#### `check_self_collision` — Body Hit Detection

Iterates `snake_x/y[1 .. len-1]` and compares each with the candidate new head (r16, r17). Returns `r22 = 1` on hit, `r22 = 0` otherwise. Index 0 (current head) is skipped because the head hasn't moved yet at this point.

---

#### `read_joystick` — Full 10-bit ADC Joystick

The most complex subroutine. Reads both axes via the ATmega328P's built-in ADC and converts the reading into a direction change.

**ADC Sampling:**
- Sets `ADMUX` to select channel 0 (A0) or 1 (A1) with AVcc reference
- Sets `ADSC` bit in `ADCSRA` to start a single conversion
- Polls `ADSC` until it clears (conversion complete, ~104 µs at prescaler 128)
- Reads `ADCL` first (mandatory to latch `ADCH`), then `ADCH`
- Right-shifts result twice: 10-bit value → 8-bit (0–255, centre ≈ 128)

**Thresholds (after >>2):**
```
  0 ────── 96 ─────── 128 ─────── 160 ──────── 255
  │  LEFT  │  dead zone  │  dead zone  │  RIGHT  │
           (UP/DOWN for Y axis)
```

**Dominant-axis selection:**
- Computes deflection magnitude `|value - 128|` for both X and Y
- Compares the two magnitudes — only the **more deflected axis** triggers a direction change
- Prevents diagonal pushes from firing both axes simultaneously

**180° reversal guard:**
- Left blocked if currently moving right
- Right blocked if currently moving left
- Up blocked if currently moving down
- Down blocked if currently moving up

---

#### `asm_rand` — 16-bit Galois LFSR

A maximal-length linear feedback shift register. Produces a full cycle of 65535 unique values before repeating.

```
Feedback polynomial: x¹⁶ + x¹⁴ + x¹³ + x¹¹ + 1  (0xB400)

Each call:
  1. Right-shift rand_state by 1 (lsr/ror)
  2. If the shifted-out bit was 1 → XOR state with 0xB400
  3. Store new state, return in r24:r25
```

Seeded from Timer1 in `asm_reset_game` for different sequences each game.

---

## 📐 Memory Layout

| Region | Usage | Notes |
|---|---|---|
| Flash (32 KB) | ~4–5 KB | Assembly code + C++ + Adafruit library |
| SRAM (2 KB) | ~616 B BSS + stack | Snake arrays dominate (600 B) |
| EEPROM (1 KB) | 0 B | Unused |

The snake arrays are the largest consumers: `snake_x[300]` + `snake_y[300]` = **600 bytes**, supporting a maximum snake length of 298 (the full grid minus border).

---

## ⚙️ Assembly Architecture

### Request-Flag Protocol

Assembly cannot call C++ library functions. Instead, a **shared-memory flag protocol** is used:

```
  Assembly side                    C++ side (after asm_game_tick returns)
  ─────────────                    ──────────────────────────────────────
  set req_erase_tail = 1     →     if (req_erase_tail) { drawCell(BLACK); req_erase_tail=0; }
  set req_draw_cell  = 1     →     if (req_draw_cell)  { drawCell(GREEN); req_draw_cell=0;  }
  set req_spawn_food = 1     →     if (req_spawn_food) { drawCell(ORANGE);req_spawn_food=0; }
  set req_show_score = 1     →     if (req_show_score) { drawScore();     req_show_score=0; }
  set req_game_over  = 1     →     (handled via game_state check at loop top)
```

Each flag is **cleared by C++ after servicing** — a simple producer/consumer pattern without interrupts.

### Why Two Draw Flags?

Both `req_erase_tail` and `req_draw_cell` are needed because `move_snake` wants to erase the tail **and** `asm_game_tick` wants to draw the head — in the same tick. A single flag would be overwritten. The tail erase goes into `req_erase_tail_x/y`; the head draw goes into `req_draw_cell_x/y/color`. C++ services the erase first, then the head draw, maintaining correct visual order.

---

## 🔨 Building & Flashing

### Arduino IDE

1. Place all three files (`SnakeGame.cpp`, `Snake_asm.h`, `snake_logic.S`) in a folder named `SnakeGame`
2. Open `SnakeGame.cpp` in the Arduino IDE (it will pick up the other files automatically)
3. Install libraries via **Sketch → Include Library → Manage Libraries**:
   - `Adafruit ST7735 and ST7789 Library`
   - `Adafruit GFX Library`
4. Select **Board: Arduino Uno**, correct **Port**
5. Click **Upload**

### arduino-cli

```bash
arduino-cli lib install "Adafruit ST7735 and ST7789 Library" "Adafruit GFX Library"
arduino-cli compile --fqbn arduino:avr:uno SnakeGame/
arduino-cli upload  --fqbn arduino:avr:uno --port /dev/ttyUSB0 SnakeGame/
```

---

## 🎛️ Configuration & Tuning

All tuneable constants are at the top of their respective files:

| Constant | File | Default | Effect |
|---|---|---|---|
| `GRID_SIZE` | `SnakeGame.cpp` | `8` | Pixel size of each cell |
| `delay(120)` | `SnakeGame.cpp` | `120 ms` | Frame delay — lower = faster game |
| `GRID_W` | `snake_logic.S` | `30` | Grid width in cells |
| `GRID_H` | `snake_logic.S` | `40` | Grid height in cells |
| `JOY_THRESH_LO` | `snake_logic.S` | `96` | Joystick low deflection threshold |
| `JOY_THRESH_HI` | `snake_logic.S` | `160` | Joystick high deflection threshold |
| `COLOR_FOOD` | `SnakeGame.cpp` | Orange | Food pellet colour (RGB-565) |
| `COLOR_SNAKE` | `SnakeGame.cpp` | Green | Snake body colour |

> To make the game faster as the score increases, replace the fixed `delay(120)` in `loop()` with `delay(max(40, 120 - game_score * 5))`.

---

## 📜 License

Free to use, modify, and distribute for educational and personal projects.
