/*
 * SnakeGame.cpp  —  Snake on Arduino Uno + ST7789 240×320 TFT + analog joystick
 *
 * Wiring:
 *   TFT CS  → D10,  DC → D9,  RST → D8,  SDA → D11 (MOSI),  SCK → D13
 *   Joystick VRx → A0,  VRy → A1,  VCC → 5 V,  GND → GND
 *
 * Libraries required (install via Arduino Library Manager):
 *   Adafruit ST7789  +  Adafruit GFX
 *
 * Build note:
 *   Place this file alongside snake_logic.S and Snake_asm.h in the
 *   same sketch folder.  The Arduino IDE / arduino-cli will compile
 *   all three together automatically.
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "Snake_asm.h"

// ── TFT pin assignments ────────────────────────────────────────
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8

// ── Grid / screen dimensions ──────────────────────────────────
// Portrait 240×320,  8 px per grid cell
// GRID_W = 240/8 = 30,  GRID_H = 320/8 = 40
#define SCREEN_W   240
#define SCREEN_H   320
#define GRID_SIZE    8
#define GRID_W      30
#define GRID_H      40

// ── RGB-565 colour macros ─────────────────────────────────────
#define RGB565(r,g,b) \
    ((uint16_t)( (((uint16_t)(r) & 0xF8u) << 8) \
               | (((uint16_t)(g) & 0xFCu) << 3) \
               | (((uint16_t)(b) & 0xF8u) >> 3) ))

#define COLOR_FOOD    RGB565(255, 80,  0)   // orange food pellet
#define COLOR_SNAKE   ST77XX_GREEN          // snake body
#define COLOR_BG      ST77XX_BLACK          // background / erase colour
#define COLOR_BORDER  ST77XX_WHITE          // play-field border

// ── TFT object ────────────────────────────────────────────────
static Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// ── Forward declarations ──────────────────────────────────────
static void drawCell(uint8_t x, uint8_t y, uint16_t color);
static void drawBorder(void);
static void drawScore(void);
static void drawInitialSnake(void);
static void drawGameOver(void);
static void resetAndRedraw(void);

// =============================================================
//  Helper functions
// =============================================================

/**
 * Fill one grid cell with @color.
 * 1-pixel gap between cells (the +1 offset + GRID_SIZE-1 width).
 * Guards against drawing on the border row/column (index 0 or max).
 */
static void drawCell(uint8_t x, uint8_t y, uint16_t color)
{
    if (x == 0 || x >= GRID_W || y == 0 || y >= GRID_H) return;
    tft.fillRect(
        static_cast<int16_t>(x) * GRID_SIZE + 1,
        static_cast<int16_t>(y) * GRID_SIZE + 1,
        GRID_SIZE - 1,
        GRID_SIZE - 1,
        color
    );
}

/** Draw the white play-field border. */
static void drawBorder(void)
{
    tft.drawRect(0, 0, SCREEN_W, SCREEN_H, COLOR_BORDER);
}

/** Erase and redraw the score value from game_score. */
static void drawScore(void)
{
    tft.fillRect(50, 2, 80, 10, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(50, 2);
    tft.print(game_score);
}

/** Draw the initial 3-segment snake after a reset. */
static void drawInitialSnake(void)
{
    for (uint8_t i = 0; i < snake_len; i++) {
        drawCell(snake_x[i], snake_y[i], COLOR_SNAKE);
    }
}

/** Full-screen game-over message with score. */
static void drawGameOver(void)
{
    tft.fillScreen(COLOR_BG);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(20, 110);
    tft.print(F("GAME OVER"));

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(35, 150);
    tft.print(F("Score: "));
    tft.print(game_score);

    tft.setCursor(20, 170);
    tft.print(F("Restarting in 3 s..."));

    Serial.print(F("Game Over! Score: "));
    Serial.println(game_score);
}

/** Clear screen, reset assembly state, redraw HUD + initial board. */
static void resetAndRedraw(void)
{
    tft.fillScreen(COLOR_BG);
    drawBorder();
    asm_reset_game();

    // HUD label (drawn once; score value redrawn by drawScore)
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(2, 2);
    tft.print(F("Score:"));
    drawScore();

    drawInitialSnake();
    drawCell(food_x, food_y, COLOR_FOOD);
}

// =============================================================
//  Arduino entry points
// =============================================================

void setup(void)
{
    Serial.begin(9600);

    // ── Display init (portrait, no rotation) ──────────────────
    tft.init(SCREEN_W, SCREEN_H);
    tft.setRotation(0);          // portrait: 240 wide × 320 tall
    tft.fillScreen(COLOR_BG);
    drawBorder();

    // ── ADC for joystick (AVcc ref, prescaler 128 ≈ 9.6 kHz) ─
    ADMUX  = (1u << REFS0);
    ADCSRA = (1u << ADEN) | (1u << ADPS2) | (1u << ADPS1) | (1u << ADPS0);

    // ── Timer1 free-running — provides entropy for LFSR seed ──
    TCCR1A = 0;
    TCCR1B = (1u << CS10);       // clk/1, just counting

    // ── Initial game state ────────────────────────────────────
    asm_reset_game();

    // ── Draw HUD + board ──────────────────────────────────────
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(2, 2);
    tft.print(F("Score:"));
    drawScore();

    drawInitialSnake();
    drawCell(food_x, food_y, COLOR_FOOD);

    Serial.println(F("Snake Game Started"));
}

void loop(void)
{
    // ── Game-over state ───────────────────────────────────────
    if (game_state == 1u) {
        drawGameOver();
        delay(3000);
        resetAndRedraw();
        return;
    }

    // ── Normal game tick ──────────────────────────────────────
    asm_game_tick();

    /*
     * Draw order per tick:
     *
     *  1. req_erase_tail  — erase the vacated tail cell (set by
     *                        move_snake in the .S file).
     *  2. req_draw_cell   — draw the new head cell in GREEN
     *                        (set by draw_head in asm_game_tick).
     *  3. req_spawn_food  — draw freshly spawned food in orange.
     *  4. req_show_score  — refresh the score HUD.
     *
     * req_erase_tail and req_draw_cell are kept in separate
     * variables so neither write clobbers the other while the
     * assembly is still running.
     */

    // 1. Erase old tail cell
    if (req_erase_tail) {
        drawCell(req_erase_tail_x, req_erase_tail_y, COLOR_BG);
        req_erase_tail = 0u;
    }

    // 2. Draw new head (or any other single-cell update)
    if (req_draw_cell) {
        // Color is already packed as RGB-565 by the assembly
        drawCell(req_draw_cell_x, req_draw_cell_y,
                 static_cast<uint16_t>(req_draw_cell_color));
        req_draw_cell = 0u;
    }

    // 3. Draw newly spawned food pellet
    if (req_spawn_food) {
        drawCell(food_x, food_y, COLOR_FOOD);
        req_spawn_food = 0u;
    }

    // 4. Refresh score HUD
    if (req_show_score) {
        drawScore();
        req_show_score = 0u;
    }

    // ~8 fps — reduce delay for a faster/harder game
    delay(120);
}