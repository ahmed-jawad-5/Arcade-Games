.section .bss

.global req_draw_cell_x
.global req_draw_cell_y
.global req_draw_cell_color
.global req_draw_cell
.global req_erase_tail
.global req_erase_tail_x
.global req_erase_tail_y
.global req_spawn_food
.global req_game_over
.global req_show_score
.global game_score
.global game_state
.global snake_x
.global snake_y
.global snake_len
.global food_x
.global food_y

req_draw_cell_x:     .space 1
req_draw_cell_y:     .space 1
req_draw_cell_color: .space 2
req_draw_cell:       .space 1
req_erase_tail:      .space 1
req_erase_tail_x:    .space 1
req_erase_tail_y:    .space 1
req_spawn_food:      .space 1
req_game_over:       .space 1
req_show_score:      .space 1
game_score:          .space 2
game_state:          .space 1

snake_x:             .space 300
snake_y:             .space 300
snake_len:           .space 1
food_x:              .space 1
food_y:              .space 1
dir_x:               .space 1
dir_y:               .space 1
rand_state:          .space 2

.section .text

; ---------------------------------------------------------------
; Constants
; ---------------------------------------------------------------
#define GRID_W        30
#define GRID_H        40
#define STATE_PLAYING  0
#define STATE_GAME_OVER 1

; RGB565 colours — use .equ so lo8()/hi8() resolve correctly
.equ COLOR_GREEN, 0x07E0
.equ COLOR_RED,   0xF800
.equ COLOR_BLACK, 0x0000

; ADC registers (ATmega328P)
#define ADCL_REG      0x78
#define ADCH_REG      0x79
#define ADCSRA_REG    0x7A
#define ADMUX_REG     0x7C

; Joystick thresholds are defined inside read_joystick below.

; ---------------------------------------------------------------
; asm_reset_game – initialise / restart everything
; ---------------------------------------------------------------
.global asm_reset_game
asm_reset_game:
    ; Seed LFSR with a non-zero value so rand works from the start.
    ; We read TCNT1 (free-running timer) for a bit of entropy; if it
    ; is still 0 at power-on just use a fixed seed instead.
    lds  r16, 0x84          ; TCNT1L
    lds  r17, 0x85          ; TCNT1H
    tst  r16
    brne seed_ok
    tst  r17
    brne seed_ok
    ldi  r16, 0xAC          ; fallback non-zero seed
    ldi  r17, 0x13
seed_ok:
    sts  rand_state,   r16
    sts  rand_state+1, r17

    ; Clear all request / state flags
    clr  r16
    sts  game_state,    r16
    sts  game_score,    r16
    sts  game_score+1,  r16
    sts  req_game_over, r16
    sts  req_show_score,r16
    sts  req_spawn_food,r16
    sts  req_draw_cell, r16
    sts  req_erase_tail, r16

    ; Initial direction: moving right (+X)
    ldi  r16, 1
    sts  dir_x, r16
    clr  r16
    sts  dir_y, r16

    ; Snake starts with length 3, head at (16,20)
    ldi  r16, 3
    sts  snake_len, r16

    ; snake_x[0..2] = 16, 15, 14  (head is index 0)
    ldi  r28, lo8(snake_x)
    ldi  r29, hi8(snake_x)
    ldi  r16, 16
    st   Y+, r16
    ldi  r16, 15
    st   Y+, r16
    ldi  r16, 14
    st   Y+, r16

    ; snake_y[0..2] = 20, 20, 20
    ldi  r28, lo8(snake_y)
    ldi  r29, hi8(snake_y)
    ldi  r16, 20
    st   Y+, r16
    st   Y+, r16
    st   Y+, r16

    ; Spawn first food
    rcall spawn_food_rand

    ; Ask C side to redraw score = 0
    ldi  r16, 1
    sts  req_show_score, r16
    ret

; ---------------------------------------------------------------
; asm_game_tick – called once per frame from loop()
; ---------------------------------------------------------------
.global asm_game_tick
asm_game_tick:
    push r16
    push r17
    push r18
    push r19
    push r20
    push r21
    push r22
    push r23
    push r28
    push r29

    ; Read joystick and update direction registers
    rcall read_joystick

    ; Compute new head position
    lds  r16, snake_x       ; r16 = new_head_x
    lds  r17, snake_y       ; r17 = new_head_y
    lds  r18, dir_x
    lds  r19, dir_y
    add  r16, r18
    add  r17, r19

    ; --- Wall collision (grid is 1-based: valid x in [1,GRID_W-2]) ---
    ; Using unsigned comparison: value == 0 (underflow) or >= GRID_W-1
    tst  r16
    breq gameover
    cpi  r16, GRID_W-1
    brsh gameover
    tst  r17
    breq gameover
    cpi  r17, GRID_H-1
    brsh gameover

    ; --- Self-collision check ---
    rcall check_self_collision
    tst  r22                ; r22 = 1 if collision found
    brne gameover

    ; --- Food check ---
    lds  r20, food_x
    lds  r21, food_y
    cp   r16, r20
    brne do_normal_move
    cp   r17, r21
    brne do_normal_move

    ; Ate food: grow snake, update score, spawn new food
    rcall eat_food
    rjmp draw_head

do_normal_move:
    rcall move_snake        ; shift body, erase tail

draw_head:
    ; Draw new head in GREEN
    sts  req_draw_cell_x, r16
    sts  req_draw_cell_y, r17
    ldi  r18, lo8(COLOR_GREEN)
    ldi  r19, hi8(COLOR_GREEN)
    sts  req_draw_cell_color,   r18
    sts  req_draw_cell_color+1, r19
    ldi  r18, 1
    sts  req_draw_cell, r18

    ; Save new head into snake arrays
    sts  snake_x, r16
    sts  snake_y, r17

    rjmp tick_done

gameover:
    ldi  r16, STATE_GAME_OVER
    sts  game_state, r16
    ldi  r16, 1
    sts  req_game_over, r16

tick_done:
    pop  r29
    pop  r28
    pop  r23
    pop  r22
    pop  r21
    pop  r20
    pop  r19
    pop  r18
    pop  r17
    pop  r16
    ret

; ---------------------------------------------------------------
; move_snake – erase tail cell, shift body one step forward,
;              write new head (caller writes snake_x/Y[0] after)
; Preconditions: r16=new_head_x  r17=new_head_y
; Clobbers: r18..r23, r28/r29
; ---------------------------------------------------------------
move_snake:
    lds  r22, snake_len

    ; --- Erase the last tail segment ---
    mov  r23, r22
    dec  r23                ; index of last segment

    ldi  r28, lo8(snake_x)
    ldi  r29, hi8(snake_x)
    add  r28, r23
    adc  r29, r1
    ld   r20, Y             ; r20 = tail_x

    ldi  r28, lo8(snake_y)
    ldi  r29, hi8(snake_y)
    add  r28, r23
    adc  r29, r1
    ld   r21, Y             ; r21 = tail_y

    sts  req_erase_tail_x, r20
    sts  req_erase_tail_y, r21
    ldi  r18, 1
    sts  req_erase_tail, r18

    ; --- Shift body: snake_x[i] = snake_x[i-1] for i = len-1 down to 1 ---
    mov  r18, r22
    dec  r18                ; r18 = snake_len - 1
shift_loop:
    tst  r18
    breq shift_done
    mov  r19, r18
    dec  r19                ; r19 = r18 - 1  (source index)

    ; snake_x[r18] = snake_x[r19]
    ldi  r28, lo8(snake_x)
    ldi  r29, hi8(snake_x)
    add  r28, r19
    adc  r29, r1
    ld   r20, Y

    ldi  r28, lo8(snake_x)
    ldi  r29, hi8(snake_x)
    add  r28, r18
    adc  r29, r1
    st   Y, r20

    ; snake_y[r18] = snake_y[r19]
    ldi  r28, lo8(snake_y)
    ldi  r29, hi8(snake_y)
    add  r28, r19
    adc  r29, r1
    ld   r20, Y

    ldi  r28, lo8(snake_y)
    ldi  r29, hi8(snake_y)
    add  r28, r18
    adc  r29, r1
    st   Y, r20

    dec  r18
    rjmp shift_loop
shift_done:
    ret

; ---------------------------------------------------------------
; eat_food – grow snake, update score, request food respawn,
;            shift body (head NOT written here; caller does it)
; Preconditions: r16=new_head_x  r17=new_head_y
; Clobbers: r18..r23, r28/r29
; ---------------------------------------------------------------
eat_food:
    ; r16=new_head_x, r17=new_head_y — preserve across rand/mod calls
    push r16
    push r17

    ; Grow: increment snake_len
    lds  r22, snake_len
    inc  r22
    sts  snake_len, r22

    ; Increment 16-bit score
    lds  r20, game_score
    lds  r21, game_score+1
    inc  r20
    brcc score_no_carry
    inc  r21
score_no_carry:
    sts  game_score,   r20
    sts  game_score+1, r21

    ; Request score redraw
    ldi  r20, 1
    sts  req_show_score, r20

    ; Shift existing body indices 1..len-1 to 2..len
    lds  r18, snake_len
    dec  r18            ; r18 = new len - 1  (= old len)
eat_shift_loop:
    tst  r18
    breq eat_shift_done
    mov  r19, r18
    dec  r19

    ldi  r28, lo8(snake_x)
    ldi  r29, hi8(snake_x)
    add  r28, r19
    adc  r29, r1
    ld   r20, Y

    ldi  r28, lo8(snake_x)
    ldi  r29, hi8(snake_x)
    add  r28, r18
    adc  r29, r1
    st   Y, r20

    ldi  r28, lo8(snake_y)
    ldi  r29, hi8(snake_y)
    add  r28, r19
    adc  r29, r1
    ld   r20, Y

    ldi  r28, lo8(snake_y)
    ldi  r29, hi8(snake_y)
    add  r28, r18
    adc  r29, r1
    st   Y, r20

    dec  r18
    rjmp eat_shift_loop
eat_shift_done:
    ; Spawn new food at a random free location
    rcall spawn_food_rand
    pop  r17                ; restore new_head_y
    pop  r16                ; restore new_head_x
    ret

; ---------------------------------------------------------------
; spawn_food_rand – pick a random cell NOT occupied by the snake.
;
; FIX (v2): Two bugs corrected from the original:
;   1. food_x candidate was being clobbered before it was saved,
;      because asm_rand/mod8 for Y both write into r16.  Now we
;      push food_x onto the stack before generating Y, then pop
;      it back, so both axes come from independent rand calls.
;   2. No body-overlap check existed.  A retry loop now walks
;      every snake segment and restarts if the candidate cell
;      matches any of them, guaranteeing food never spawns
;      inside the snake.
;
; Clobbers: r16..r22, r24/r25, r28/r29
; Result:   food_x, food_y updated; req_spawn_food = 1.
; ---------------------------------------------------------------
spawn_food_rand:
sfr_retry:
    ; ── Generate candidate X in [1 .. GRID_W-2] ──────────────
    rcall asm_rand          ; random value → r24
    ldi   r18, GRID_W-2     ; modulo range = 28
    mov   r16, r24
    rcall mod8              ; r16 = r16 % 28  →  [0..27]
    inc   r16               ; r16 = food_x candidate  [1..28]

    ; Save food_x on the stack before asm_rand clobbers r16
    push  r16

    ; ── Generate candidate Y in [1 .. GRID_H-2] ──────────────
    rcall asm_rand          ; new random value → r24
    ldi   r18, GRID_H-2     ; modulo range = 38
    mov   r16, r24
    rcall mod8              ; r16 = r16 % 38  →  [0..37]
    inc   r16               ; r16 = food_y candidate  [1..38]
    mov   r17, r16          ; r17 holds food_y candidate

    pop   r16               ; r16 restored = food_x candidate

    ; ── Check candidate against every snake body segment ──────
    ; If (r16, r17) matches any (snake_x[i], snake_y[i]) → retry.
    lds   r22, snake_len    ; r22 = number of segments to check
    ldi   r18, 0            ; r18 = segment index, start at 0
sfr_check_loop:
    cp    r18, r22
    brsh  sfr_accept        ; all segments checked → candidate is free

    ; Load snake_x[r18]
    ldi   r28, lo8(snake_x)
    ldi   r29, hi8(snake_x)
    add   r28, r18
    adc   r29, r1
    ld    r19, Y            ; r19 = snake_x[i]

    cp    r16, r19
    brne  sfr_next          ; X differs → no collision on this segment

    ; X matched — now check Y
    ldi   r28, lo8(snake_y)
    ldi   r29, hi8(snake_y)
    add   r28, r18
    adc   r29, r1
    ld    r19, Y            ; r19 = snake_y[i]

    cp    r17, r19
    breq  sfr_retry         ; both X and Y match → pick new random coords

sfr_next:
    inc   r18
    rjmp  sfr_check_loop

sfr_accept:
    ; ── Commit the free position ──────────────────────────────
    sts   food_x, r16
    sts   food_y, r17
    ldi   r16, 1
    sts   req_spawn_food, r16
    ret

; ---------------------------------------------------------------
; mod8 – r16 = r16 % r18  (unsigned 8-bit, r18 != 0)
; Clobbers: r16, r17
; ---------------------------------------------------------------
mod8:
    tst  r18
    breq mod8_done          ; safety: avoid div by zero
mod8_loop:
    cp   r16, r18
    brlo mod8_done
    sub  r16, r18
    rjmp mod8_loop
mod8_done:
    ret

; ---------------------------------------------------------------
; check_self_collision – check if (r16,r17) hits any body segment
; Returns: r22 = 0 (no collision)  or  1 (collision)
; Clobbers: r18..r22, r28/r29
; ---------------------------------------------------------------
check_self_collision:
    lds  r22, snake_len
    ; Skip index 0 (current head, not yet written) and check 1..len-1
    ldi  r18, 1
csc_loop:
    cp   r18, r22
    brsh csc_no_hit

    ldi  r28, lo8(snake_x)
    ldi  r29, hi8(snake_x)
    add  r28, r18
    adc  r29, r1
    ld   r19, Y

    cp   r16, r19
    brne csc_next

    ldi  r28, lo8(snake_y)
    ldi  r29, hi8(snake_y)
    add  r28, r18
    adc  r29, r1
    ld   r19, Y

    cp   r17, r19
    brne csc_next

    ; Hit
    ldi  r22, 1
    ret
csc_next:
    inc  r18
    rjmp csc_loop
csc_no_hit:
    ldi  r22, 0
    ret

; ---------------------------------------------------------------
; read_joystick  –  full 10-bit ADC read on both axes, dominant-
;                   axis selection, 180° reversal prevention.
; ---------------------------------------------------------------

#define JOY_THRESH_LO  96
#define JOY_THRESH_HI  160

read_joystick:
    push r16
    push r17
    push r18
    push r19
    push r20
    push r21
    push r22
    push r24
    push r25

    ; ── Sample X axis (A0) ───────────────────────────────────
    ldi  r16, (1<<6)            ; REFS0=1, MUX=0 (A0)
    sts  ADMUX_REG, r16
    lds  r16, ADCSRA_REG
    ori  r16, (1<<6)            ; ADSC start
    sts  ADCSRA_REG, r16
rj_wait_x:
    lds  r16, ADCSRA_REG
    sbrc r16, 6
    rjmp rj_wait_x
    lds  r16, ADCL_REG          ; latch: ADCL must be read first
    lds  r17, ADCH_REG          ; r17:r16 = 10-bit X

    ; ── Sample Y axis (A1) ───────────────────────────────────
    ldi  r18, (1<<6)|(1<<0)     ; REFS0=1, MUX=1 (A1)
    sts  ADMUX_REG, r18
    lds  r18, ADCSRA_REG
    ori  r18, (1<<6)
    sts  ADCSRA_REG, r18
rj_wait_y:
    lds  r18, ADCSRA_REG
    sbrc r18, 6
    rjmp rj_wait_y
    lds  r18, ADCL_REG
    lds  r19, ADCH_REG          ; r19:r18 = 10-bit Y

    ; ── Compress to 8-bit by right-shifting 2 places ─────────
    lsr  r17
    ror  r16
    lsr  r17
    ror  r16                    ; r16 = X >> 2  (0..255, centre≈128)

    lsr  r19
    ror  r18
    lsr  r19
    ror  r18                    ; r18 = Y >> 2  (0..255, centre≈128)

    ; ── Compute deflection magnitudes (|val - 128|) ───────────
    ldi  r20, 128
    cp   r16, r20
    brsh rj_x_above
    sub  r20, r16
    rjmp rj_x_mag_done
rj_x_above:
    mov  r20, r16
    subi r20, 128
rj_x_mag_done:

    ldi  r21, 128
    cp   r18, r21
    brsh rj_y_above
    sub  r21, r18
    rjmp rj_y_mag_done
rj_y_above:
    mov  r21, r18
    subi r21, 128
rj_y_mag_done:

    ; ── Determine dominant axis ───────────────────────────────
    clr  r22

    cp   r20, r21
    brsh rj_x_dominant

    ; ── Y axis is dominant ────────────────────────────────────
    cpi  r18, JOY_THRESH_LO
    brlo rj_dir_up
    cpi  r18, JOY_THRESH_HI
    brsh rj_dir_down
    rjmp rj_apply

rj_dir_up:
    ldi  r22, 3
    rjmp rj_apply

rj_dir_down:
    ldi  r22, 4
    rjmp rj_apply

    ; ── X axis is dominant ────────────────────────────────────
rj_x_dominant:
    cpi  r16, JOY_THRESH_LO
    brlo rj_dir_left
    cpi  r16, JOY_THRESH_HI
    brsh rj_dir_right
    rjmp rj_apply

rj_dir_left:
    ldi  r22, 1
    rjmp rj_apply

rj_dir_right:
    ldi  r22, 2

    ; ── Apply direction (with 180° reversal guard) ────────────
rj_apply:
    tst  r22
    breq joy_exit

    lds  r24, dir_x
    lds  r25, dir_y

    cpi  r22, 1                 ; left?
    brne rj_chk_right
    cpi  r24, 1                 ; currently going right? → ignore
    breq joy_exit
    ldi  r24, 0xFF              ; dir_x = -1
    clr  r25
    rjmp rj_save

rj_chk_right:
    cpi  r22, 2                 ; right?
    brne rj_chk_up
    cpi  r24, 0xFF              ; currently going left? → ignore
    breq joy_exit
    ldi  r24, 1
    clr  r25
    rjmp rj_save

rj_chk_up:
    cpi  r22, 3                 ; up?
    brne rj_chk_down
    cpi  r25, 1                 ; currently going down? → ignore
    breq joy_exit
    ldi  r25, 0xFF              ; dir_y = -1
    clr  r24
    rjmp rj_save

rj_chk_down:
    cpi  r25, 0xFF              ; currently going up? → ignore
    breq joy_exit
    ldi  r25, 1
    clr  r24

rj_save:
    sts  dir_x, r24
    sts  dir_y, r25

joy_exit:
    pop  r25
    pop  r24
    pop  r22
    pop  r21
    pop  r20
    pop  r19
    pop  r18
    pop  r17
    pop  r16
    ret

; ---------------------------------------------------------------
; asm_rand – 16-bit Galois LFSR (taps at bits 15,13,12,10)
; Result returned in r24:r25 (also stored back to rand_state)
; ---------------------------------------------------------------
.global asm_rand
asm_rand:
    lds  r24, rand_state
    lds  r25, rand_state+1
    lsr  r25
    ror  r24
    brcc rand_no_tap
    ; Feedback polynomial 0xB400 (taps 15,13,12,10 — maximal length)
    ldi  r18, 0x00
    ldi  r19, 0xB4
    eor  r24, r18
    eor  r25, r19
rand_no_tap:
    sts  rand_state,   r24
    sts  rand_state+1, r25
    ret