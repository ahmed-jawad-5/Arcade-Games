/*
 * SnakeGame.cpp — Snake + Pac-Man + Flappy Bird
 * (Arduino Uno + ST7789 240×320 + KY-023 joystick)
 *
 * Wiring:
 *   TFT CS→D10  DC→D9  RST→D8  MOSI→D11  SCK→D13
 *   Joystick VRx→A0  VRy→A1  SW→D2 (INPUT_PULLUP)
 *
 * Libraries: Adafruit ST7789 + Adafruit GFX
 * Also needs: snake_logic.S  +  Snake_asm.h  in the same sketch folder.
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "arcade_asm.h"

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
enum GameMode : uint8_t { MODE_MENU=0, MODE_SNAKE, MODE_PACMAN, MODE_FLAPPY };
static GameMode gameMode      = MODE_MENU;
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
static void flappyInit(void);     static void handleFlappy(void);

// ═══════════════════════════════════════════════════════════════
//  MENU  (updated for 3 games)
// ═══════════════════════════════════════════════════════════════
static void drawMenu(void){
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2); tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(30,30);  tft.print(F("ARCADE"));
    tft.setCursor(42,52);  tft.print(F("GAMES"));
    tft.drawFastHLine(10,76,220,ST77XX_WHITE);

    tft.setTextColor(menuSelection==0 ? ST77XX_GREEN : ST77XX_WHITE);
    tft.setCursor(40,100); tft.print(F("1. SNAKE"));

    tft.setTextColor(menuSelection==1 ? RGB565(255,255,0) : ST77XX_WHITE);
    tft.setCursor(28,145); tft.print(F("2. PAC-MAN"));

    tft.setTextColor(menuSelection==2 ? RGB565(255,160,0) : ST77XX_WHITE);
    tft.setCursor(16,190); tft.print(F("3. FLAPPY"));

    tft.drawFastHLine(10,228,220,ST77XX_WHITE);
    tft.setTextSize(1); tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(18,244); tft.print(F("Joystick UP/DOWN to select"));
    tft.setCursor(28,260); tft.print(F("Press button to start"));
}

static void handleMenu(void){
    int vy=analogRead(A1);
    static uint32_t lm=0;
    if(millis()-lm>300ul){
        if(vy<400 && menuSelection>0){
            menuSelection--;
            drawMenu(); lm=millis();
        } else if(vy>624 && menuSelection<2){
            menuSelection++;
            drawMenu(); lm=millis();
        }
    }
    if(buttonPressed()){
        if     (menuSelection==0){ gameMode=MODE_SNAKE;  resetAndRedrawSnake(); }
        else if(menuSelection==1){ gameMode=MODE_PACMAN; pacmanInit(); }
        else                     { gameMode=MODE_FLAPPY; flappyInit(); }
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
#define PM_OX    8
#define PM_OY   16

#define PM_WALL_C   RGB565(33,33,222)
#define PM_DOT_C    RGB565(255,222,176)
#define PM_PWR_C    ST77XX_WHITE
#define PM_PM_C     RGB565(255,255,0)
#define PM_FREEZE_C RGB565(0,180,255)
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
static inline bool pm_isPenInterior(uint8_t col, uint8_t row){
    return (col>=10&&col<=17&&row>=13&&row<=16);
}

// ═══════════════════════════════════════════════════════════════
//  PAC-MAN ENTITIES + GLOBALS
// ═══════════════════════════════════════════════════════════════
struct PacMan { int16_t px,py; int8_t dx,dy,ndx,ndy; uint8_t animTick; };
struct Ghost  {
    int16_t px,py;
    int8_t  dx,dy;
    bool    frozen;
    uint8_t freezeTimer;
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

static uint32_t pm_dots[PM_ROWS];
static uint8_t  pm_pwrCol[4];
static uint8_t  pm_pwrRow[4];

#define PM_TICK_MS      220u
#define PM_FREEZE_TICKS  40u

// FIXED ghost spawn positions — open pen interior tiles
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
    if(erase){ pm_restoreTile(px2col(pm.px), py2row(pm.py)); return; }
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
    if(erase){ pm_restoreTile(px2col(x), py2row(y)); return; }
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
//  PACMAN INIT HELPERS
// ═══════════════════════════════════════════════════════════════
static void pm_initDots(void){
    memset(pm_dots,0,sizeof(pm_dots));
    pm_totalDots=pm_dotsEaten=0;
    for(uint8_t r=0;r<PM_ROWS;r++)
        for(uint8_t c=0;c<PM_COLS;c++)
            if(!pm_isWall(c,r)&&!pm_isPenInterior(c,r)){
                pm_dots[r]|=(1ul<<c); pm_totalDots++;
            }
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

// FIXED: auto-detect valid first direction
static void pm_respawnGhost(uint8_t idx){
    gh[idx].px=col2px(ghostStartCol[idx]);
    gh[idx].py=row2py(ghostStartRow[idx]);
    static const int8_t dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    gh[idx].dx=0; gh[idx].dy=0;
    for(uint8_t d=0;d<4;d++){
        uint8_t nc=(uint8_t)(ghostStartCol[idx]+dirs[d][0]);
        uint8_t nr=(uint8_t)(ghostStartRow[idx]+dirs[d][1]);
        if(!pm_isWall(nc,nr)){
            gh[idx].dx=dirs[d][0];
            gh[idx].dy=dirs[d][1];
            break;
        }
    }
    gh[idx].frozen=false;
    gh[idx].freezeTimer=0;
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
    pm_drawMaze(); pm_drawAllDots(); pm_drawHUD();
    pm_drawPacMan(false);
    for(uint8_t i=0;i<4;i++) pm_drawGhost(i,false);
    pm_lastTick=millis();
}

// ═══════════════════════════════════════════════════════════════
//  JOYSTICK (pacman)
// ═══════════════════════════════════════════════════════════════
static void pm_readJoystick(void){
    int vx=analogRead(A0), vy=analogRead(A1);
    if     (vx<300){pm.ndx=-1;pm.ndy= 0;}
    else if(vx>724){pm.ndx= 1;pm.ndy= 0;}
    else if(vy<300){pm.ndx= 0;pm.ndy=-1;}
    else if(vy>724){pm.ndx= 0;pm.ndy= 1;}
}

// ═══════════════════════════════════════════════════════════════
//  PAC-MAN MOVEMENT
// ═══════════════════════════════════════════════════════════════
static void pm_movePacMan(void){
    uint8_t col=px2col(pm.px), row=py2row(pm.py);
    pm.px=col2px(col); pm.py=row2py(row);
    uint8_t nc=(uint8_t)((int8_t)col+pm.ndx);
    uint8_t nr=(uint8_t)((int8_t)row+pm.ndy);
    if(!pm_isWall(nc,nr)){pm.dx=pm.ndx;pm.dy=pm.ndy;}
    nc=(uint8_t)((int8_t)col+pm.dx);
    nr=(uint8_t)((int8_t)row+pm.dy);
    if(!pm_isWall(nc,nr)){pm.px=col2px(nc);pm.py=row2py(nr);col=nc;row=nr;}
    pm.animTick++;
    if((pm_dots[row]>>col)&1u){
        pm_dots[row]&=~(1ul<<col);
        pm_dotsEaten++;
        bool isPwr=false;
        for(uint8_t i=0;i<4;i++){
            if(pm_pwrCol[i]==col&&pm_pwrRow[i]==row){isPwr=true;pm_pwrCol[i]=0xFF;}
        }
        if(isPwr){
            pm_score+=50;
            for(uint8_t i=0;i<4;i++){gh[i].frozen=true;gh[i].freezeTimer=PM_FREEZE_TICKS;}
        } else { pm_score+=10; }
        pm_fillTile(col,row,PM_BG_C);
        if(pm_dotsEaten>=pm_totalDots) pm_win=true;
        pm_drawHUD();
    }
}

// ═══════════════════════════════════════════════════════════════
//  GHOST AI
// ═══════════════════════════════════════════════════════════════
static void pm_moveGhost(uint8_t idx){
    Ghost &g=gh[idx];
    uint8_t gcol=px2col(g.px), grow=py2row(g.py);
    g.px=col2px(gcol); g.py=row2py(grow);
    uint8_t pcol=px2col(pm.px), prow=py2row(pm.py);
    static const int8_t dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    int8_t  bestDx=g.dx, bestDy=g.dy;
    int16_t bestDist=32767;
    bool    found=false;
    for(uint8_t d=0;d<4;d++){
        int8_t ndx=dirs[d][0], ndy=dirs[d][1];
        if(ndx==-g.dx&&ndy==-g.dy) continue;
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
//  COLLISION (FIXED: half-tile tolerance)
// ═══════════════════════════════════════════════════════════════
static void pm_checkCollision(void){
    for(uint8_t i=0;i<4;i++){
        if(gh[i].frozen) continue;
        int16_t dx=pm.px-gh[i].px;
        int16_t dy=pm.py-gh[i].py;
        if(dx<0)dx=-dx; if(dy<0)dy=-dy;
        if(dx>PM_TILE/2||dy>PM_TILE/2) continue;
        pm_lives--;
        pm_drawHUD();
        delay(600);
        if(pm_lives==0){pm_gameOver=true;return;}
        pm_drawPacMan(true);
        for(uint8_t j=0;j<4;j++){pm_drawGhost(j,true);pm_respawnGhost(j);}
        pm.px=col2px(13);pm.py=row2py(23);
        pm.dx=1;pm.dy=0;pm.ndx=1;pm.ndy=0;
        delay(800);
        pm_drawPacMan(false);
        for(uint8_t j=0;j<4;j++) pm_drawGhost(j,false);
        return;
    }
}

// ═══════════════════════════════════════════════════════════════
//  PAC-MAN END SCREEN
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
//  PAC-MAN MAIN LOOP
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
    pm_drawPacMan(true);
    for(uint8_t i=0;i<4;i++) pm_drawGhost(i,true);
    pm_movePacMan();
    for(uint8_t i=0;i<4;i++){
        if(gh[i].frozen){
            if(gh[i].freezeTimer>0) if(--gh[i].freezeTimer==0) gh[i].frozen=false;
        } else { pm_moveGhost(i); }
    }
    pm_drawPacMan(false);
    for(uint8_t i=0;i<4;i++) pm_drawGhost(i,false);
    pm_checkCollision();
}

// ═══════════════════════════════════════════════════════════════
//  FLAPPY BIRD CONSTANTS  (minimal pixel-art style)
// ═══════════════════════════════════════════════════════════════
#define FB_BIRD_X       50
#define FB_BIRD_S        6   // bird is a 6×6 white square
#define FB_GRAVITY       1
#define FB_FLAP_V       -7
#define FB_PIPE_W       14   // narrower, plain white outline
#define FB_PIPE_GAP     60
#define FB_PIPE_SPEED    3
#define FB_PIPE_COUNT    3
#define FB_PIPE_SPACING 100
#define FB_TICK_MS      45u
#define FB_BG_C         ST77XX_BLACK
#define FB_FG_C         ST77XX_WHITE
#define FB_GROUND_Y     (SCREEN_H - 12)

// ═══════════════════════════════════════════════════════════════
//  FLAPPY BIRD GLOBALS
// ═══════════════════════════════════════════════════════════════
struct FBPipe { int16_t x; int16_t gapY; bool scored; };

static int16_t  fb_birdY;
static int8_t   fb_birdVY;
static FBPipe   fb_pipes[FB_PIPE_COUNT];
static int16_t  fb_score;
static bool     fb_gameOver;
static bool     fb_started;
static uint32_t fb_lastTick;

// ═══════════════════════════════════════════════════════════════
//  FLAPPY BIRD HELPERS  — delta-only rendering
//  Each pipe moves FB_PIPE_SPEED px left per tick.
//  We only paint two thin vertical strips per pipe:
//    • erase the FB_PIPE_SPEED-wide column vacated on the RIGHT
//    • draw  the FB_PIPE_SPEED-wide column exposed  on the LEFT
//  Bird moves vertically only, so we only erase/draw the rows
//  that changed (the top or bottom sliver based on velocity).
// ═══════════════════════════════════════════════════════════════

// Full pipe draw/erase — used only on init and pipe recycle
static void fb_drawPipeFull(uint8_t idx, bool erase){
    FBPipe &p = fb_pipes[idx];
    uint16_t c = erase ? FB_BG_C : FB_FG_C;
    int16_t topH = p.gapY;
    int16_t botY = p.gapY + FB_PIPE_GAP;
    int16_t botH = FB_GROUND_Y - botY;
    if(topH > 0) tft.fillRect(p.x, 0,    FB_PIPE_W, topH, c);
    if(botH > 0) tft.fillRect(p.x, botY, FB_PIPE_W, botH, c);
}

// Delta pipe update — only paints the changed strips
static void fb_updatePipe(uint8_t idx){
    FBPipe &p = fb_pipes[idx];
    int16_t oldX = p.x + FB_PIPE_SPEED;   // where it WAS
    int16_t newX = p.x;                    // where it IS now
    int16_t topH = p.gapY;
    int16_t botY = p.gapY + FB_PIPE_GAP;
    int16_t botH = FB_GROUND_Y - botY;
    // Erase the right edge strip (now uncovered)
    if(topH > 0) tft.fillRect(oldX + FB_PIPE_W - FB_PIPE_SPEED, 0,
                               FB_PIPE_SPEED, topH, FB_BG_C);
    if(botH > 0) tft.fillRect(oldX + FB_PIPE_W - FB_PIPE_SPEED, botY,
                               FB_PIPE_SPEED, botH, FB_BG_C);
    // Draw the new left edge strip
    if(topH > 0) tft.fillRect(newX, 0,    FB_PIPE_SPEED, topH, FB_FG_C);
    if(botH > 0) tft.fillRect(newX, botY, FB_PIPE_SPEED, botH, FB_FG_C);
}

// Bird: 6×6 white square — full draw/erase
static void fb_drawBirdFull(bool erase){
    tft.fillRect(FB_BIRD_X - FB_BIRD_S/2, fb_birdY - FB_BIRD_S/2,
                 FB_BIRD_S, FB_BIRD_S, erase ? FB_BG_C : FB_FG_C);
}

// Delta bird — only erase the rows left behind, draw the new rows
// oldY = previous fb_birdY before the move
static void fb_updateBird(int16_t oldY){
    int16_t dy = fb_birdY - oldY;
    if(dy == 0){ fb_drawBirdFull(false); return; }
    int16_t bx = FB_BIRD_X - FB_BIRD_S/2;
    if(dy > 0){
        // moved down: erase top strip, draw bottom strip
        int16_t er = (dy > FB_BIRD_S) ? FB_BIRD_S : dy;
        tft.fillRect(bx, oldY - FB_BIRD_S/2, FB_BIRD_S, er,        FB_BG_C);
        tft.fillRect(bx, fb_birdY + FB_BIRD_S/2 - er, FB_BIRD_S, er, FB_FG_C);
    } else {
        // moved up: erase bottom strip, draw top strip
        int16_t er = (-dy > FB_BIRD_S) ? FB_BIRD_S : -dy;
        tft.fillRect(bx, oldY + FB_BIRD_S/2 - er, FB_BIRD_S, er,   FB_BG_C);
        tft.fillRect(bx, fb_birdY - FB_BIRD_S/2, FB_BIRD_S, er,    FB_FG_C);
    }
}

// Ground: single white line
static void fb_drawGround(void){
    tft.drawFastHLine(0, FB_GROUND_Y, SCREEN_W, FB_FG_C);
}

// HUD: only redrawn on score change
static void fb_drawHUD(void){
    tft.fillRect(0, 0, 72, 10, FB_BG_C);
    tft.setTextSize(1); tft.setTextColor(FB_FG_C);
    tft.setCursor(2, 2); tft.print(F("SC:")); tft.print(fb_score);
}

static void fb_initPipe(uint8_t idx, int16_t startX){
    fb_pipes[idx].x      = startX;
    uint8_t range        = (uint8_t)(FB_GROUND_Y - FB_PIPE_GAP - 40);
    fb_pipes[idx].gapY   = 20 + (int16_t)(TCNT1 % range);
    fb_pipes[idx].scored = false;
}

// ═══════════════════════════════════════════════════════════════
//  flappyInit()
// ═══════════════════════════════════════════════════════════════
static void flappyInit(void){
    fb_score    = 0;
    fb_gameOver = false;
    fb_started  = false;
    fb_birdY    = SCREEN_H / 2;
    fb_birdVY   = 0;
    for(uint8_t i=0;i<FB_PIPE_COUNT;i++)
        fb_initPipe(i, SCREEN_W + 40 + i * FB_PIPE_SPACING);
    tft.fillScreen(FB_BG_C);
    fb_drawGround();
    for(uint8_t i=0;i<FB_PIPE_COUNT;i++) fb_drawPipeFull(i,false);
    fb_drawBirdFull(false);
    fb_drawHUD();
    tft.setTextSize(1); tft.setTextColor(FB_FG_C);
    tft.setCursor(18, SCREEN_H/2 - 16); tft.print(F("FLAPPY BIRD"));
    tft.setCursor(10, SCREEN_H/2 + 2);  tft.print(F("Press to flap!"));
    fb_lastTick = millis();
}

// ═══════════════════════════════════════════════════════════════
//  handleFlappy()
// ═══════════════════════════════════════════════════════════════
static void handleFlappy(void){
    // ── Game over screen ────────────────────────────────────────
    if(fb_gameOver){
        tft.fillScreen(FB_BG_C);
        tft.setTextSize(2); tft.setTextColor(FB_FG_C);
        tft.setCursor(20,100); tft.print(F("GAME OVER"));
        tft.setTextSize(1);
        tft.setCursor(35,140); tft.print(F("Score: ")); tft.print(fb_score);
        tft.setCursor(15,162); tft.print(F("Press button for menu"));
        while(!buttonPressed()){delay(50);}
        gameMode=MODE_MENU; menuSelection=2; drawMenu();
        return;
    }

    // ── Input ───────────────────────────────────────────────────
    bool flap=false;
    if(analogRead(A1)<300) flap=true;
    if(buttonPressed())    flap=true;

    if(flap && !fb_started){
        tft.fillRect(0, SCREEN_H/2-20, SCREEN_W, 28, FB_BG_C);
        fb_started=true;
    }

    // ── Tick throttle ───────────────────────────────────────────
    if(millis()-fb_lastTick < FB_TICK_MS) return;
    fb_lastTick=millis();

    // ── Wait animation — just bob the bird 1px, no pipes involved
    if(!fb_started){
        static int8_t hoverDir=1;
        int16_t oldY = fb_birdY;
        fb_birdY += hoverDir;
        if(fb_birdY > SCREEN_H/2+4) hoverDir=-1;
        if(fb_birdY < SCREEN_H/2-4) hoverDir= 1;
        fb_updateBird(oldY);
        return;
    }

    // ── Physics ─────────────────────────────────────────────────
    if(flap) fb_birdVY = FB_FLAP_V;
    fb_birdVY += FB_GRAVITY;
    if(fb_birdVY >  12) fb_birdVY =  12;
    if(fb_birdVY < -10) fb_birdVY = -10;
    int16_t oldBirdY = fb_birdY;
    fb_birdY += fb_birdVY;

    // ── Collision check ─────────────────────────────────────────
    if(fb_birdY - FB_BIRD_S/2 <= 0 || fb_birdY + FB_BIRD_S/2 >= FB_GROUND_Y){
        fb_gameOver=true; return;
    }
    for(uint8_t i=0;i<FB_PIPE_COUNT;i++){
        int16_t bx1=FB_BIRD_X - FB_BIRD_S/2 + 1, bx2=FB_BIRD_X + FB_BIRD_S/2 - 1;
        int16_t by1=fb_birdY  - FB_BIRD_S/2 + 1, by2=fb_birdY  + FB_BIRD_S/2 - 1;
        int16_t px1=fb_pipes[i].x,                px2=fb_pipes[i].x + FB_PIPE_W;
        int16_t gapTop=fb_pipes[i].gapY,          gapBot=fb_pipes[i].gapY + FB_PIPE_GAP;
        if((bx2>px1)&&(bx1<px2)&&!((by1>gapTop)&&(by2<gapBot))){
            fb_gameOver=true; return;
        }
    }

    // ── Pipe positions + scoring ─────────────────────────────────
    bool scoreChanged=false;
    for(uint8_t i=0;i<FB_PIPE_COUNT;i++){
        bool recycle = (fb_pipes[i].x + FB_PIPE_W - FB_PIPE_SPEED < 0);
        if(recycle){
            // Full erase at old position before repositioning
            fb_drawPipeFull(i, true);
            fb_pipes[i].x -= FB_PIPE_SPEED;
            int16_t maxX=0;
            for(uint8_t j=0;j<FB_PIPE_COUNT;j++)
                if(fb_pipes[j].x>maxX) maxX=fb_pipes[j].x;
            fb_initPipe(i, maxX + FB_PIPE_SPACING);
            fb_drawPipeFull(i, false);
        } else {
            fb_pipes[i].x -= FB_PIPE_SPEED;
            fb_updatePipe(i);   // delta — only 2×FB_PIPE_SPEED-wide strips
        }
        if(!fb_pipes[i].scored && (FB_BIRD_X - FB_BIRD_S/2) > (fb_pipes[i].x + FB_PIPE_W)){
            fb_pipes[i].scored=true; fb_score++; scoreChanged=true;
        }
    }

    // ── Bird delta draw ──────────────────────────────────────────
    fb_updateBird(oldBirdY);

    // ── Restore ground line (pipes may overwrite bottom pixel) ───
    tft.drawFastHLine(0, FB_GROUND_Y, SCREEN_W, FB_FG_C);

    if(scoreChanged) fb_drawHUD();
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
    gameMode=MODE_MENU; menuSelection=0;
    drawMenu();
    Serial.println(F("Arcade Ready"));
}

void loop(void){
    switch(gameMode){
        case MODE_MENU:   handleMenu();   break;
        case MODE_SNAKE:  handleSnake();  break;
        case MODE_PACMAN: handlePacMan(); break;
        case MODE_FLAPPY: handleFlappy(); break;
    }
}
