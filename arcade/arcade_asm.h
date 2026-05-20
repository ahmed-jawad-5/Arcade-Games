#pragma once
/*
 * Snake_asm.h
 * Declarations for variables and functions defined in snake_logic.S
 * All BSS variables are shared between C++ (.ino) and AVR assembly (.S).
 */

#ifdef __cplusplus
extern "C" {
#endif

// ── Assembly functions ─────────────────────────────────────────
void asm_reset_game(void);
void asm_game_tick(void);
uint16_t asm_rand(void);

// ── Request flags (assembly → C) ──────────────────────────────
// Set to 1 by assembly; cleared to 0 by C after servicing.

/** X coordinate of cell to draw (req_draw_cell request) */
extern volatile uint8_t  req_draw_cell_x;
/** Y coordinate of cell to draw */
extern volatile uint8_t  req_draw_cell_y;
/** RGB-565 colour for req_draw_cell */
extern volatile uint16_t req_draw_cell_color;
/** Non-zero → draw cell at (req_draw_cell_x, req_draw_cell_y) */
extern volatile uint8_t  req_draw_cell;

/**
 * Tail-erase request.
 * Set by move_snake before the head draw request overwrites req_draw_cell.
 * C must erase this cell before processing req_draw_cell.
 */
extern volatile uint8_t  req_erase_tail;
extern volatile uint8_t  req_erase_tail_x;
extern volatile uint8_t  req_erase_tail_y;

/** Non-zero → draw food at (food_x, food_y) */
extern volatile uint8_t  req_spawn_food;
/** Non-zero → game is over */
extern volatile uint8_t  req_game_over;
/** Non-zero → redraw score from game_score */
extern volatile uint8_t  req_show_score;

// ── Game state ────────────────────────────────────────────────
/** 16-bit score (little-endian in BSS) */
extern volatile int16_t  game_score;
/** 0 = playing, 1 = game over */
extern volatile uint8_t  game_state;

// ── Snake data ────────────────────────────────────────────────
extern volatile uint8_t  snake_x[300];
extern volatile uint8_t  snake_y[300];
extern volatile uint8_t  snake_len;

// ── Food position ─────────────────────────────────────────────
extern volatile uint8_t  food_x;
extern volatile uint8_t  food_y;

// ── RNG state (seeded in asm_reset_game) ──────────────────────
extern volatile uint16_t rand_state;

#ifdef __cplusplus
}
#endif