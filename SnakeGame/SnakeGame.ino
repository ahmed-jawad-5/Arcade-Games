/*
 * SnakeGame.cpp — Snake + Pac-Man  (Arduino Uno + ST7789 240×320 + KY-023 joystick)
 *
 * Wiring:
 *   TFT CS→D10  DC→D9  RST→D8  MOSI→D11  SCK→D13
 *   Joystick VRx→A0  VRy→A1  SW→D2 (INPUT_PULLUP)
 *
 * Libraries: Adafruit ST7789 + Adafruit GFX
 * Also needs: snake_logic.S  +  Snake_asm.h  in the same sketch folder.
 *
 * PAC-MAN changes in this revision
 * ──────────────────────────────────
 *  • 4 ghosts, all with distinct start positions inside the open pen
 *  • Pellets placed ONLY on open non-pen, non-border tiles → dots always visible
 *  • Ghost sprite erase now restores underlying dot/wall instead of black-filling
 *  • Collision uses pixel-overlap test (not just tile match) → reliable kills
 *  • Power pellet = FREEZE: ghosts freeze in place, touching them is harmless;
 *    no "eaten" / return-to-base logic at all
 *  • Win condition counts only actual pellet tiles
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "Snake_asm.h"

// ═══════════════════════════════════════════════════════════════
//  HARDWARE
// ═══════════════════════════════════════════════════════════════
#define TFT_CS  10
#define TFT_DC   9
#define TFT_RST  8
#define JOY_SW   2

#define SCREEN_W 240
#define SCREEN_H 320
#define GRID_SIZE 8
#define GRID_W   30
#define GRID_H   40

#define RGB565(r,g,b) ((uint16_t)((((uint16_t)(r)&0xF8u)<<8)|(((uint16_t)(g)&0xFCu)<<3)|(((uint16_t)(b)&0xF8u)>>3)))

#define COLOR_FOOD   RGB565(255,80,0)
#define COLOR_SNAKE  ST77XX_GREEN
#define COLOR_BG     ST77XX_BLACK
#define COLOR_BORDER ST77XX_WHITE

static Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// ═══════════════════════════════════════════════════════════════
//  GAME MODE
// ═══════════════════════════════════════════════════════════════
enum GameMode : uint8_t { MODE_MENU=0, MODE_SNAKE, MODE_PACMAN };
static GameMode gameMode     = MODE_MENU;
static uint8_t  menuSelection = 0;

static uint32_t lastBtnTime = 0;
#define BTN_DEBOUNCE_MS 200ul
static bool buttonPressed(void){
    if(digitalRead(JOY_SW)==LOW){
        uint32_t n=millis();
        if(n-lastBtnTime>BTN_DEBOUNCE_MS){lastBtnTime=n;return true;}
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════
//  FORWARD DECLS
// ═══════════════════════════════════════════════════════════════
static void drawMenu(void);       static void handleMenu(void);
static void drawCell(uint8_t,uint8_t,uint16_t);
static void drawBorder(void);     static void drawSnakeScore(void);
static void drawInitialSnake(void);
static void drawSnakeGameOver(void);
static void resetAndRedrawSnake(void); static void handleSnake(void);
static void pacmanInit(void);     static void handlePacMan(void);

// ═══════════════════════════════════════════════════════════════
//  MENU
// ═══════════════════════════════════════════════════════════════
static void drawMenu(void){
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2); tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(30,40);  tft.print(F("ARCADE"));
    tft.setCursor(42,62);  tft.print(F("GAMES"));
    tft.drawFastHLine(10,90,220,ST77XX_WHITE);
    tft.setTextColor(menuSelection==0?ST77XX_GREEN:ST77XX_WHITE);
    tft.setCursor(40,130); tft.print(F("1. SNAKE"));
    tft.setTextColor(menuSelection==1?RGB565(255,255,0):ST77XX_WHITE);
    tft.setCursor(28,175); tft.print(F("2. PAC-MAN"));
    tft.drawFastHLine(10,220,220,ST77XX_WHITE);
    tft.setTextSize(1); tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(18,240); tft.print(F("Joystick UP/DOWN to select"));
    tft.setCursor(28,256); tft.print(F("Press button to start"));
}
static void handleMenu(void){
    int vy=analogRead(A1);
    static uint32_t lm=0;
    if(millis()-lm>300ul){
        if(vy<400&&menuSelection!=0){menuSelection=0;drawMenu();lm=millis();}
        else if(vy>624&&menuSelection!=1){menuSelection=1;drawMenu();lm=millis();}
    }
    if(buttonPressed()){
        if(menuSelection==0){gameMode=MODE_SNAKE; resetAndRedrawSnake();}
        else               {gameMode=MODE_PACMAN;pacmanInit();}
    }
}

// ═══════════════════════════════════════════════════════════════
//  SNAKE
// ═══════════════════════════════════════════════════════════════
static void drawCell(uint8_t x,uint8_t y,uint16_t color){
    if(x==0||x>=GRID_W||y==0||y>=GRID_H)return;
    tft.fillRect((int16_t)x*GRID_SIZE+1,(int16_t)y*GRID_SIZE+1,GRID_SIZE-1,GRID_SIZE-1,color);
}
static void drawBorder(void){tft.drawRect(0,0,SCREEN_W,SCREEN_H,COLOR_BORDER);}
static void drawSnakeScore(void){
    tft.fillRect(50,2,80,10,COLOR_BG);
    tft.setTextSize(1);tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(50,2);tft.print(game_score);
}
static void drawInitialSnake(void){
    for(uint8_t i=0;i<snake_len;i++)drawCell(snake_x[i],snake_y[i],COLOR_SNAKE);
}
static void drawSnakeGameOver(void){
    tft.fillScreen(COLOR_BG);
    tft.setTextSize(2);tft.setTextColor(ST77XX_RED);
    tft.setCursor(20,110);tft.print(F("GAME OVER"));
    tft.setTextSize(1);tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(35,150);tft.print(F("Score: "));tft.print(game_score);
    tft.setCursor(15,170);tft.print(F("Press button for menu"));
}
static void resetAndRedrawSnake(void){
    tft.fillScreen(COLOR_BG);drawBorder();asm_reset_game();
    tft.setTextSize(1);tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(2,2);tft.print(F("Score:"));
    drawSnakeScore();drawInitialSnake();drawCell(food_x,food_y,COLOR_FOOD);
}
static void handleSnake(void){
    if(game_state==1u){
        drawSnakeGameOver();
        while(!buttonPressed()){delay(50);}
        gameMode=MODE_MENU;menuSelection=0;drawMenu();return;
    }
    asm_game_tick();
    if(req_erase_tail){drawCell(req_erase_tail_x,req_erase_tail_y,COLOR_BG);req_erase_tail=0u;}
    if(req_draw_cell){drawCell(req_draw_cell_x,req_draw_cell_y,(uint16_t)req_draw_cell_color);req_draw_cell=0u;}
    if(req_spawn_food){drawCell(food_x,food_y,COLOR_FOOD);req_spawn_food=0u;}
    if(req_show_score){drawSnakeScore();req_show_score=0u;}
    if(buttonPressed()){gameMode=MODE_MENU;menuSelection=0;drawMenu();return;}
    delay(120);
}

// ═══════════════════════════════════════════════════════════════
//  PAC-MAN CONSTANTS
// ═══════════════════════════════════════════════════════════════
#define PM_TILE  8
#define PM_COLS 28
#define PM_ROWS 31
#define PM_OX    8    // pixel x of column 0
#define PM_OY   16    // pixel y of row 0 (below HUD)

#define PM_WALL_C   RGB565(33,33,222)
#define PM_DOT_C    RGB565(255,222,176)
#define PM_PWR_C    ST77XX_WHITE
#define PM_PM_C     RGB565(255,255,0)
#define PM_FREEZE_C RGB565(0,180,255)   // frozen ghost colour
#define PM_BG_C     ST77XX_BLACK

static const uint16_t ghostColor[4] PROGMEM = {
    RGB565(255,0,0),
    RGB565(255,184,255),
    RGB565(0,255,255),
    RGB565(255,184,82),
};

// ═══════════════════════════════════════════════════════════════
//  MAZE
// ═══════════════════════════════════════════════════════════════
//
// Column bitmask: bit r = 1 → wall at (col, row).
//
// Open horizontal corridor rows (pass through every non-border col):
//   1, 5, 8, 12, 17, 23, 26, 29
//
// Vertical corridor cols (open everywhere except rows 0 and 30):
//   1, 6, 9, 13, 14, 18, 21, 26
//
// Ghost pen: cols 10-17, rows 13-16 are OPEN interior.
//   Only cols 10 and 17 get the pen-side wall bits on rows 13-16.
//   This gives ghosts a closed room they can move inside, with exits
//   at row 12 (corridor) and row 17 (corridor).
//
// PELLET ZONE: dots are placed only on open tiles that are NOT in the
//   pen interior (rows 13-16, cols 10-17) and NOT row 0 or 30.
//   This is handled in pm_initDots() by the pm_isPenInterior() check.

#define _HCORR ((1ul<<1)|(1ul<<5)|(1ul<<8)|(1ul<<12)|(1ul<<17)|(1ul<<23)|(1ul<<26)|(1ul<<29))
#define _SHELF  (0x7FFFFFFFul & ~_HCORR)
#define _PSIDE  ((1ul<<13)|(1ul<<14)|(1ul<<15)|(1ul<<16))

static const uint32_t pm_mazeWalls[PM_COLS] PROGMEM = {
    /*0 */ 0x7FFFFFFFul,
    /*1 */ (1ul<<0)|(1ul<<30),
    /*2 */ _SHELF, /*3*/_SHELF, /*4*/_SHELF, /*5*/_SHELF,
    /*6 */ (1ul<<0)|(1ul<<30),
    /*7 */ _SHELF, /*8*/_SHELF,
    /*9 */ (1ul<<0)|(1ul<<30),
    /*10*/ (_SHELF|_PSIDE),
    /*11*/ _SHELF, /*12*/_SHELF,
    /*13*/ (1ul<<0)|(1ul<<30),
    /*14*/ (1ul<<0)|(1ul<<30),
    /*15*/ _SHELF, /*16*/_SHELF,
    /*17*/ (_SHELF|_PSIDE),
    /*18*/ (1ul<<0)|(1ul<<30),
    /*19*/ _SHELF, /*20*/_SHELF,
    /*21*/ (1ul<<0)|(1ul<<30),
    /*22*/ _SHELF,/*23*/_SHELF,/*24*/_SHELF,/*25*/_SHELF,
    /*26*/ (1ul<<0)|(1ul<<30),
    /*27*/ 0x7FFFFFFFul,
};
#undef _HCORR
#undef _SHELF
#undef _PSIDE

static inline bool pm_isWall(uint8_t col, uint8_t row){
    if(col>=PM_COLS||row>=PM_ROWS)return true;
    return (pgm_read_dword(&pm_mazeWalls[col])>>row)&1u;
}

// Returns true for the pen interior cells (where ghosts live but no dots go)
static inline bool pm_isPenInterior(uint8_t col, uint8_t row){
    return (col>=10&&col<=17&&row>=13&&row<=16);
}

// ═══════════════════════════════════════════════════════════════
//  ENTITY STRUCTS + GLOBALS
// ═══════════════════════════════════════════════════════════════
struct PacMan { int16_t px,py; int8_t dx,dy,ndx,ndy; uint8_t animTick; };
struct Ghost  {
    int16_t px,py;
    int8_t  dx,dy;
    bool    frozen;       // true = power pellet active, ghost stops
    uint8_t freezeTimer;  // ticks remaining
};

static PacMan pm;
static Ghost  gh[4];

static int16_t  pm_score;
static uint8_t  pm_lives;
static uint16_t pm_totalDots;
static uint16_t pm_dotsEaten;
static bool     pm_gameOver;
static bool     pm_win;
static uint32_t pm_lastTick;
static uint8_t  pm_ghostSlowCtr;

// Pellet bitmask: bit c in pm_dots[r] = dot present at (c,r)
static uint32_t pm_dots[PM_ROWS];
static uint8_t  pm_pwrCol[4];
static uint8_t  pm_pwrRow[4];

#define PM_TICK_MS     220u
#define PM_FREEZE_TICKS 40u   // how long ghosts stay frozen

// Ghost spawn positions — all inside the open pen interior
// (cols 11-16, rows 13-16 are open, not wall, not pen-side)
static const uint8_t ghostStartCol[4] = {12, 13, 14, 15};
static const uint8_t ghostStartRow[4] = {14, 14, 14, 14};
// ═══════════════════════════════════════════════════════════════
//  PIXEL ↔ TILE
// ═══════════════════════════════════════════════════════════════
static inline uint8_t  px2col(int16_t px){return(uint8_t)((px-PM_OX)/PM_TILE);}
static inline uint8_t  py2row(int16_t py){return(uint8_t)((py-PM_OY)/PM_TILE);}
static inline int16_t  col2px(uint8_t c) {return PM_OX+(int16_t)c*PM_TILE;}
static inline int16_t  row2py(uint8_t r) {return PM_OY+(int16_t)r*PM_TILE;}

// ═══════════════════════════════════════════════════════════════
//  TILE RENDERERS
// ═══════════════════════════════════════════════════════════════
static void pm_fillTile(uint8_t col,uint8_t row,uint16_t color){
    tft.fillRect(PM_OX+col*PM_TILE, PM_OY+row*PM_TILE, PM_TILE, PM_TILE, color);
}
static void pm_drawWallTile(uint8_t col,uint8_t row){
    int16_t x=PM_OX+col*PM_TILE, y=PM_OY+row*PM_TILE;
    tft.fillRect(x,y,PM_TILE,PM_TILE,PM_BG_C);
    tft.drawRect(x,y,PM_TILE,PM_TILE,PM_WALL_C);
}
static void pm_drawPellet(uint8_t col,uint8_t row){
    tft.fillCircle(PM_OX+col*PM_TILE+PM_TILE/2,
                   PM_OY+row*PM_TILE+PM_TILE/2, 1, PM_DOT_C);
}
static void pm_drawPowerPellet(uint8_t col,uint8_t row){
    tft.fillCircle(PM_OX+col*PM_TILE+PM_TILE/2,
                   PM_OY+row*PM_TILE+PM_TILE/2, 3, PM_PWR_C);
}

// Restore the background of a tile after a sprite was erased there.
// Redraws wall border, pellet, or just black as appropriate.
static void pm_restoreTile(uint8_t col, uint8_t row){
    if(pm_isWall(col,row)){ pm_drawWallTile(col,row); return; }
    pm_fillTile(col,row,PM_BG_C);
    if((pm_dots[row]>>col)&1u){
        bool isPwr=false;
        for(uint8_t i=0;i<4;i++) if(pm_pwrCol[i]==col&&pm_pwrRow[i]==row){isPwr=true;break;}
        if(isPwr) pm_drawPowerPellet(col,row); else pm_drawPellet(col,row);
    }
}

// ═══════════════════════════════════════════════════════════════
//  PAC-MAN SPRITE
// ═══════════════════════════════════════════════════════════════
static void pm_drawPacMan(bool erase){
    if(erase){
        pm_restoreTile(px2col(pm.px), py2row(pm.py));
        return;
    }
    int16_t cx=pm.px+PM_TILE/2, cy=pm.py+PM_TILE/2;
    uint8_t r=PM_TILE/2-1;
    tft.fillCircle(cx,cy,r,PM_PM_C);
    if((pm.animTick&1u)==0u){
        int16_t x0=pm.px,x1=pm.px+PM_TILE,y0=pm.py,y1=pm.py+PM_TILE;
        if     (pm.dx== 1) tft.fillTriangle(cx,cy,x1,y0,x1,y1,PM_BG_C);
        else if(pm.dx==-1) tft.fillTriangle(cx,cy,x0,y0,x0,y1,PM_BG_C);
        else if(pm.dy==-1) tft.fillTriangle(cx,cy,x0,y0,x1,y0,PM_BG_C);
        else               tft.fillTriangle(cx,cy,x0,y1,x1,y1,PM_BG_C);
    }
}

// ═══════════════════════════════════════════════════════════════
//  GHOST SPRITE
// ═══════════════════════════════════════════════════════════════
static void pm_drawGhost(uint8_t idx, bool erase){
    int16_t x=gh[idx].px, y=gh[idx].py;
    if(erase){
        // Restore whatever was under the ghost (wall / dot / empty)
        pm_restoreTile(px2col(x), py2row(y));
        return;
    }
    // Body colour: cyan-blue when frozen, normal colour otherwise
    uint16_t bodyCol = gh[idx].frozen ? PM_FREEZE_C
                                      : pgm_read_word(&ghostColor[idx]);
    tft.fillRoundRect(x,y,PM_TILE,PM_TILE-2,3,bodyCol);
    tft.fillRect(x,        y+PM_TILE-3,3,3,bodyCol);
    tft.fillRect(x+PM_TILE/2-1,y+PM_TILE-3,3,3,bodyCol);
    tft.fillRect(x+PM_TILE-3,  y+PM_TILE-3,3,3,bodyCol);
    tft.fillCircle(x+2,y+3,2,ST77XX_WHITE);
    tft.fillCircle(x+5,y+3,2,ST77XX_WHITE);
    tft.fillCircle(x+2,y+3,1,ST77XX_BLUE);
    tft.fillCircle(x+5,y+3,1,ST77XX_BLUE);
}

// ═══════════════════════════════════════════════════════════════
//  INIT
// ═══════════════════════════════════════════════════════════════
static void pm_initDots(void){
    memset(pm_dots,0,sizeof(pm_dots));
    pm_totalDots=pm_dotsEaten=0;
    for(uint8_t r=0;r<PM_ROWS;r++){
        for(uint8_t c=0;c<PM_COLS;c++){
            // Only place dots on truly open, non-pen-interior tiles
            if(!pm_isWall(c,r) && !pm_isPenInterior(c,r)){
                pm_dots[r]|=(1ul<<c);
                pm_totalDots++;
            }
        }
    }
    // Power pellets at the 4 open corner corridor tiles
    const uint8_t ppPos[4][2]={{1,1},{26,1},{1,29},{26,29}};
    for(uint8_t i=0;i<4;i++){pm_pwrCol[i]=ppPos[i][0];pm_pwrRow[i]=ppPos[i][1];}
}

static void pm_drawMaze(void){
    for(uint8_t r=0;r<PM_ROWS;r++)
        for(uint8_t c=0;c<PM_COLS;c++)
            if(pm_isWall(c,r)) pm_drawWallTile(c,r);
            else               pm_fillTile(c,r,PM_BG_C);
}

static void pm_drawAllDots(void){
    for(uint8_t r=0;r<PM_ROWS;r++){
        uint32_t bits=pm_dots[r]; if(!bits)continue;
        for(uint8_t c=0;c<PM_COLS;c++){
            if(!((bits>>c)&1u))continue;
            bool isPwr=false;
            for(uint8_t i=0;i<4;i++) if(pm_pwrCol[i]==c&&pm_pwrRow[i]==r){isPwr=true;break;}
            if(isPwr) pm_drawPowerPellet(c,r); else pm_drawPellet(c,r);
        }
    }
}

static void pm_drawHUD(void){
    tft.fillRect(0,0,SCREEN_W,PM_OY,PM_BG_C);
    tft.setTextSize(1);tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(2,4);tft.print(F("SC:"));tft.print(pm_score);
    for(uint8_t i=0;i<pm_lives&&i<5;i++) tft.fillCircle(180+i*11,8,4,PM_PM_C);
}

static void pm_respawnGhost(uint8_t idx){
    gh[idx].px = col2px(ghostStartCol[idx]);
    gh[idx].py = row2py(ghostStartRow[idx]);

    // Pick any valid initial direction (don't assume right/left is open)
    static const int8_t dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    gh[idx].dx = 0; gh[idx].dy = 0;
    for(uint8_t d = 0; d < 4; d++){
        uint8_t nc = ghostStartCol[idx] + dirs[d][0];
        uint8_t nr = ghostStartRow[idx] + dirs[d][1];
        if(!pm_isWall(nc, nr)){
            gh[idx].dx = dirs[d][0];
            gh[idx].dy = dirs[d][1];
            break;
        }
    }

    gh[idx].frozen = false;
    gh[idx].freezeTimer = 0;
}

static void pacmanInit(void){
    pm_score=0;pm_lives=3;
    pm_gameOver=pm_win=false;
    pm_ghostSlowCtr=0;
    pm.px=col2px(13);pm.py=row2py(23);
    pm.dx=1;pm.dy=0;pm.ndx=1;pm.ndy=0;pm.animTick=0;
    for(uint8_t i=0;i<4;i++) pm_respawnGhost(i);
    pm_initDots();
    tft.fillScreen(PM_BG_C);
    pm_drawMaze();
    pm_drawAllDots();
    pm_drawHUD();
    pm_drawPacMan(false);
    for(uint8_t i=0;i<4;i++) pm_drawGhost(i,false);
    pm_lastTick=millis();
}

// ═══════════════════════════════════════════════════════════════
//  JOYSTICK
// ═══════════════════════════════════════════════════════════════
static void pm_readJoystick(void){
    int vx=analogRead(A0), vy=analogRead(A1);
    if     (vx<300){pm.ndx=-1;pm.ndy= 0;}
    else if(vx>724){pm.ndx= 1;pm.ndy= 0;}
    else if(vy<300){pm.ndx= 0;pm.ndy=-1;}
    else if(vy>724){pm.ndx= 0;pm.ndy= 1;}
}

// ═══════════════════════════════════════════════════════════════
//  PAC-MAN MOVEMENT + PELLET EATING
// ═══════════════════════════════════════════════════════════════
static void pm_movePacMan(void){
    // Snap to tile grid
    uint8_t col=px2col(pm.px), row=py2row(pm.py);
    pm.px=col2px(col); pm.py=row2py(row);

    // Try to turn in the buffered direction
    uint8_t nc=(uint8_t)((int8_t)col+pm.ndx);
    uint8_t nr=(uint8_t)((int8_t)row+pm.ndy);
    if(!pm_isWall(nc,nr)){pm.dx=pm.ndx;pm.dy=pm.ndy;}

    // Move in current direction (keep direction even if blocked)
    nc=(uint8_t)((int8_t)col+pm.dx);
    nr=(uint8_t)((int8_t)row+pm.dy);
    if(!pm_isWall(nc,nr)){pm.px=col2px(nc);pm.py=row2py(nr);col=nc;row=nr;}

    pm.animTick++;

    // Eat pellet
    if((pm_dots[row]>>col)&1u){
        pm_dots[row]&=~(1ul<<col);
        pm_dotsEaten++;

        bool isPwr=false;
        for(uint8_t i=0;i<4;i++){
            if(pm_pwrCol[i]==col&&pm_pwrRow[i]==row){
                isPwr=true;
                pm_pwrCol[i]=0xFF; // mark consumed
            }
        }

        if(isPwr){
            pm_score+=50;
            // FREEZE all ghosts
            for(uint8_t i=0;i<4;i++){
                gh[i].frozen=true;
                gh[i].freezeTimer=PM_FREEZE_TICKS;
            }
        } else {
            pm_score+=10;
        }

        pm_fillTile(col,row,PM_BG_C); // erase eaten dot
        if(pm_dotsEaten>=pm_totalDots) pm_win=true;
        pm_drawHUD();
    }
}

// ═══════════════════════════════════════════════════════════════
//  GHOST AI
// ═══════════════════════════════════════════════════════════════
static void pm_moveGhost(uint8_t idx){
    Ghost &g=gh[idx];
    // Snap to tile
    uint8_t gcol=px2col(g.px), grow=py2row(g.py);
    g.px=col2px(gcol); g.py=row2py(grow);

    uint8_t pcol=px2col(pm.px), prow=py2row(pm.py);

    // Pick best passable direction (Manhattan distance to Pac-Man)
    static const int8_t dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    int8_t  bestDx=g.dx, bestDy=g.dy;
    int16_t bestDist=32767;
    bool    found=false;

    for(uint8_t d=0;d<4;d++){
        int8_t ndx=dirs[d][0], ndy=dirs[d][1];
        if(ndx==-g.dx&&ndy==-g.dy) continue; // no U-turn
        uint8_t nc=(uint8_t)((int8_t)gcol+ndx);
        uint8_t nr=(uint8_t)((int8_t)grow+ndy);
        if(pm_isWall(nc,nr)) continue;
        int16_t dist=abs((int16_t)nc-pcol)+abs((int16_t)nr-prow);
        if(!found||dist<bestDist){bestDist=dist;bestDx=ndx;bestDy=ndy;found=true;}
    }

    g.dx=bestDx; g.dy=bestDy;
    uint8_t nc=(uint8_t)((int8_t)gcol+g.dx);
    uint8_t nr=(uint8_t)((int8_t)grow+g.dy);
    if(!pm_isWall(nc,nr)){g.px=col2px(nc);g.py=row2py(nr);}
}

// ═══════════════════════════════════════════════════════════════
//  COLLISION
// Pixel-overlap check: two 8×8 sprites overlap if their bounding
// boxes intersect (each axis must have less than PM_TILE separation).
// ═══════════════════════════════════════════════════════════════
static void pm_checkCollision(void){
    for(uint8_t i=0;i<4;i++){
        // Ghost frozen → completely harmless, skip
        if(gh[i].frozen) continue;

        int16_t dx=pm.px-gh[i].px;
        int16_t dy=pm.py-gh[i].py;
        if(dx<0)dx=-dx;
        if(dy<0)dy=-dy;
        if(dx > PM_TILE/2 || dy > PM_TILE/2) continue; // no overlap

        // Ghost is active and overlaps → Pac-Man dies
        pm_lives--;
        pm_drawHUD();
        delay(600);

        if(pm_lives==0){pm_gameOver=true;return;}

        // Respawn
        pm_drawPacMan(true);
        for(uint8_t j=0;j<4;j++){pm_drawGhost(j,true);pm_respawnGhost(j);}
        pm.px=col2px(13);pm.py=row2py(23);
        pm.dx=1;pm.dy=0;pm.ndx=1;pm.ndy=0;
        delay(800);
        pm_drawPacMan(false);
        for(uint8_t j=0;j<4;j++) pm_drawGhost(j,false);
        return; // one death per tick
    }
}

// ═══════════════════════════════════════════════════════════════
//  END SCREEN
// ═══════════════════════════════════════════════════════════════
static void pm_drawEndScreen(void){
    tft.fillScreen(PM_BG_C);tft.setTextSize(2);
    if(pm_win){tft.setTextColor(ST77XX_GREEN);tft.setCursor(40,100);tft.print(F("YOU WIN!"));}
    else      {tft.setTextColor(ST77XX_RED);  tft.setCursor(20,100);tft.print(F("GAME OVER"));}
    tft.setTextSize(1);tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(50,140);tft.print(F("Score: "));tft.print(pm_score);
    tft.setCursor(15,162);tft.print(F("Press button for menu"));
}

// ═══════════════════════════════════════════════════════════════
//  MAIN PAC-MAN LOOP
// ═══════════════════════════════════════════════════════════════
static void handlePacMan(void){
    if(pm_gameOver||pm_win){
        pm_drawEndScreen();
        while(!buttonPressed()){delay(50);}
        gameMode=MODE_MENU;menuSelection=1;drawMenu();return;
    }
    if(buttonPressed()){gameMode=MODE_MENU;menuSelection=1;drawMenu();return;}
    if(millis()-pm_lastTick<PM_TICK_MS)return;
    pm_lastTick=millis();
    pm_ghostSlowCtr++;

    pm_readJoystick();

    // Erase
    pm_drawPacMan(true);
    for(uint8_t i=0;i<4;i++) pm_drawGhost(i,true);

    // Move Pac-Man
    pm_movePacMan();

    // Move / tick ghosts
    for(uint8_t i=0;i<4;i++){
        if(gh[i].frozen){
            // Count down freeze timer; ghost stays put
            if(gh[i].freezeTimer>0){
                if(--gh[i].freezeTimer==0) gh[i].frozen=false;
            }
        } else {
            pm_moveGhost(i);
        }
    }

    // Redraw
    pm_drawPacMan(false);
    for(uint8_t i=0;i<4;i++) pm_drawGhost(i,false);

    // Collision check AFTER drawing (ghosts are in their new positions)
    pm_checkCollision();
}

// ═══════════════════════════════════════════════════════════════
//  ARDUINO ENTRY POINTS
// ═══════════════════════════════════════════════════════════════
void setup(void){
    Serial.begin(9600);
    pinMode(JOY_SW,INPUT_PULLUP);
    tft.init(SCREEN_W,SCREEN_H);
    tft.setRotation(0);
    tft.fillScreen(ST77XX_BLACK);
    ADMUX =(1u<<REFS0);
    ADCSRA=(1u<<ADEN)|(1u<<ADPS2)|(1u<<ADPS1)|(1u<<ADPS0);
    TCCR1A=0; TCCR1B=(1u<<CS10);
    gameMode=MODE_MENU;menuSelection=0;
    drawMenu();
    Serial.println(F("Arcade Ready"));
}

void loop(void){
    switch(gameMode){
        case MODE_MENU:   handleMenu();   break;
        case MODE_SNAKE:  handleSnake();  break;
        case MODE_PACMAN: handlePacMan(); break;
    }
}