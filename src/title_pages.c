#include <graphx.h>
#include "league.h"
#include "title_pages.h"
#include "title_art.h"

enum { WHITE = 21, RED = 23, YELLOW, BLUE, MAGENTA, GREEN };

void titlePagesInit(void)
{
    gfx_palette[RED] = gfx_RGBTo1555(255, 0, 0);
    gfx_palette[YELLOW] = gfx_RGBTo1555(255, 255, 0);
    gfx_palette[BLUE] = gfx_RGBTo1555(0, 0, 255);
    gfx_palette[MAGENTA] = gfx_RGBTo1555(255, 0, 255);
    gfx_palette[GREEN] = gfx_RGBTo1555(0, 255, 0);
}

static void text(const char *s, int x, int y, uint8_t color)
{
    gfx_SetTextFGColor(color);
    gfx_PrintStringXY(s, x, y);
}

static void picture(const uint8_t *data, unsigned height, int x, int y,
                    const uint8_t *colors, unsigned bpp)
{
    uint8_t *dest = (uint8_t *)gfx_vbuffer + y * 320 + x;
    for (unsigned row = 0; row < height; ++row) {
        for (unsigned col = 0; col < 16; ++col)
            dest[col] = colors[(data[col / (8 / bpp)]
                >> (8 - bpp - (col % (8 / bpp)) * bpp)) & ((1u << bpp) - 1)];
        data += 2 * bpp;
        dest += 320;
    }
}

void titlePageCredits(uint8_t credits)
{
    char s[] = "CREDIT 00";
    s[7] += credits / 10;
    s[8] += credits % 10;
    gfx_SetColor(0);
    gfx_FillRectangle(8, 224, 304, 8);
    text(s, 200, 224, WHITE);
    if (credits)
        text("SELECT GAME", 8, 224, WHITE);
}

void titlePageDraw(bool instructions, uint8_t credits)
{
    // PLAYER INSTRUCTIONS SETUP
    static const uint8_t bellColors[] = {0, WHITE, YELLOW, MAGENTA, RED};
    gfx_SetDrawBuffer();
    gfx_FillScreen(0);
    gfx_SetTextBGColor(0);
    if (instructions) {
        text("PLAYER INSTRUCTIONS", 80, 16, RED);
        text("BUTTONS CONTROL HERO", 56, 48, YELLOW);
        text("PRESS BLUE BUTTON TO JUMP", 56, 72, YELLOW);
        text("SCORE EACH PHASE", 56, 96, YELLOW);
        text("HERO WINS A", 56, 120, YELLOW);
        picture(instructionBell, 16, 152, 112, bellColors, 4);
        text("EACH TIME", 176, 120, YELLOW);
        text("HE COMPLETES A WALL", 56, 144, YELLOW);
        text("FIVE", 56, 168, YELLOW);
        picture(instructionBell, 16, 96, 160, bellColors, 4);
        text("GIVES SUPER BONUS", 120, 168, YELLOW);
    } else {
        leagueDrawTable(LEAGUE_COUNT, 0, NULL);
        text("BONUS  MAN  AT   10000", 72, 128, RED);
        text("PRESS YOUR BLUE BUTTON", 72, 152, YELLOW);
        text("TO READ", 128, 168, YELLOW);
        text("PLAYER INSTRUCTIONS", 80, 184, YELLOW);
        text("COPYRIGHT 1983", 104, 200, WHITE);
        text("CENTURY ELECTRONICS UK LTD", 56, 208, WHITE);
    }
    titlePageCredits(credits);
    gfx_SwapDraw();
    gfx_Wait();
    gfx_SetDrawScreen();
}

void titleHeartsDraw(unsigned first, unsigned end)
{
    static const uint8_t colors[] = {0, RED, WHITE, 0};
    // Scatters hearts across these places
    static const uint8_t spots[][2] = {
        {200,20}, {144,124}, {32,180}, {56,132}, {136,156}, {144,52},
        {16,204}, {8,4}, {216,60}, {120,204}, {32,124}, {184,196},
        {48,44}, {80,156}, {176,148}, {112,92}, {224,44}, {96,188},
        {192,100}, {208,100}, {184,220}, {192,172}, {216,108}, {64,136}
    };
    if (end > 24)
        end = 24;
    for (unsigned i = first; i < end; ++i)
        picture(heart, 9, (unsigned)spots[i][0] * 304 / 232,
                spots[i][1], colors, 2);
}
