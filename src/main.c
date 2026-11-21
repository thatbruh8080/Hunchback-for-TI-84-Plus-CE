#include <stdio.h>
#include <graphx.h>
#include <keypadc.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "gfx/gfx.h"
#include "hunch_font.h"
#include "opening.h"
#include "game_gfx.h"
#include "title_pages.h"
#include "developer.h"
#include "league.h"

enum {
    QUASI_BASE = 0,
    ESMIE_BASE = 16,
    LOGO_COLOR = 20,
    TEXT_COLOR = 21,
    PLAYER_COLOR = 22,
    LOGO_X = 40,
    LOGO_Y = 64
};

typedef struct {
    int quasiX, quasiY;
    int esmieX, esmieY;
    bool flipQuasi, flipEsmie;
} TitlePose;

static const TitlePose poses[] = {
    { 96, 128, 120,  48, false, false }, // Esmeralda above the logo
    {104,  32, 240,  96, false, true  }, // Quasimodo above the logo
    {224,  80, 112, 144, true,  false }  // Quasimodo on the right
};

// Map each packed pixel value to an index in the screen palette.
static const uint8_t logoColors[] = {0, LOGO_COLOR};

static const uint8_t quasiColors[] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15
};

static const uint8_t esmieColors[] = {
    ESMIE_BASE, ESMIE_BASE + 1, ESMIE_BASE + 2, ESMIE_BASE + 3
};

// convimg stores the leftmost pixel in the high bits of each byte.
// These title sprites have fixed positions and fit entirely on screen.
static void drawPacked(const uint8_t *data, int width, int height,
                       int x, int y, int bpp,
                       const uint8_t *colors, bool flip)
{
    const unsigned mask = (1u << bpp) - 1;
    const int pixelsPerByte = 8 / bpp;
    const int bytesPerRow = width / pixelsPerByte;
    const int step = flip ? -1 : 1;
    uint8_t *screen = (uint8_t *)gfx_vbuffer;

    for (int row = 0; row < height; ++row) {
        uint8_t *dest = screen + (y + row) * GFX_LCD_WIDTH + x;
        int column = flip ? width - 1 : 0;

        for (int i = 0; i < bytesPerRow; ++i) {
            uint8_t packed = *data++;

            for (int shift = 8 - bpp; shift >= 0; shift -= bpp) {
                unsigned pixel = (packed >> shift) & mask;
                dest[column] = colors[pixel];
                column += step;
            }
        }
    }
}

static void drawActors(const TitlePose *pose)
{
    drawPacked(bigfatquasi_data, bigfatquasi_width, bigfatquasi_height,
               pose->quasiX, pose->quasiY, 4, quasiColors, pose->flipQuasi);

    drawPacked(esmie_data, esmie_width, esmie_height,
               pose->esmieX, pose->esmieY, 2, esmieColors, pose->flipEsmie);
}

// Erase one old sprite, then put back any logo pixels it covered.
static void restoreBackground(int x, int y, int width, int height)
{
    gfx_SetColor(0);
    gfx_FillRectangle(x, y, width, height);

    // Limit the bitmap lookup to the overlap with the logo.
    int left   = x > LOGO_X ? x : LOGO_X;
    int top    = y > LOGO_Y ? y : LOGO_Y;
    int right  = x + width;
    int bottom = y + height;

    if (right > LOGO_X + logo_width)
        right = LOGO_X + logo_width;

    if (bottom > LOGO_Y + logo_height)
        bottom = LOGO_Y + logo_height;

    if (left >= right || top >= bottom)
        return;

    uint8_t *screen = (uint8_t *)gfx_vbuffer;

    for (int sy = top; sy < bottom; ++sy) {
        const uint8_t *bits = logo_data + (sy - LOGO_Y) * (logo_width / 8);
        uint8_t *row = screen + sy * GFX_LCD_WIDTH;

        for (int sx = left; sx < right; ++sx) {
            int logoPixel = sx - LOGO_X;

            if (bits[logoPixel / 8] & (0x80u >> (logoPixel % 8)))
                row[sx] = LOGO_COLOR;
        }
    }
}

static void drawCredits(uint8_t credits)
{
    char text[] = "CREDIT 00";
    text[7] = '0' + credits / 10;
    text[8] = '0' + credits % 10;
    gfx_SetColor(0);
    gfx_FillRectangle(240, 224, 72, 8);
    gfx_SetTextFGColor(TEXT_COLOR);
    gfx_PrintStringXY(text, 240, 224);
}

static void drawPlayerStart(uint8_t credits)
{
    gfx_SetDrawBuffer();
    gfx_FillScreen(0);
    gfx_SetTextFGColor(PLAYER_COLOR);
    gfx_PrintStringXY("PLAYER 1", 128, 116);
    drawCredits(credits);
    gfx_SwapDraw();
    gfx_Wait();
    gfx_SetDrawScreen();
}

static void drawTitle(uint8_t credits)
{
    // Draws title
    gfx_SetDrawBuffer();
    gfx_SetClipRegion(0, 0, 320, 240);
    gfx_SetTextScale(1, 1);
    gfx_SetTextBGColor(0);
    gfx_SetTextTransparentColor(0);
    gfx_SetPalette(titlequasi_palette, sizeof_titlequasi_palette, QUASI_BASE);
    gfx_SetPalette(esmie_palette, sizeof_esmie_palette, ESMIE_BASE);
    gfx_palette[LOGO_COLOR] = gfx_RGBTo1555(0, 255, 0);
    gfx_palette[TEXT_COLOR] = gfx_RGBTo1555(255, 255, 255);
    gfx_palette[PLAYER_COLOR] = gfx_RGBTo1555(0, 255, 255);
    titlePagesInit();
    gfx_FillScreen(0);
    hunchInstallFont();
    gfx_SetTextFGColor(TEXT_COLOR);
    gfx_PrintStringXY("COPYRIGHT 1983", 104, 184);
    gfx_PrintStringXY("CENTURY ELECTRONICS UK LTD", 56, 192);
    gfx_PrintStringXY("CE PORT BY thatbruh8080", 72, 208);
    drawCredits(credits);
    if (credits)
        gfx_PrintStringXY("SELECT GAME", 8, 224);
    drawPacked(logo_data, logo_width, logo_height,
               LOGO_X, LOGO_Y, 1, logoColors, false);
    drawActors(&poses[0]);
    gfx_SwapDraw();
    gfx_Wait();
    gfx_SetDrawScreen();
}

static void drawBlack(void)
{
    gfx_SetDrawBuffer();
    gfx_SetClipRegion(0, 0, 320, 240);
    gfx_FillScreen(0);
    gfx_SwapDraw();
    gfx_Wait();
    gfx_SetDrawScreen();
}

typedef enum {
    TITLE, HEARTS, HIGH_SCORES, INSTRUCTIONS, BLACK,
    PLAYER_START, OPENING, ATTRACT, DEVELOPER
} Screen;

int main(void)
{
    Screen screen = TITLE, next = TITLE;
    unsigned pose = 0, heartsDrawn = 0, blankTicks = 32;
    bool logoIsGreen = true, demoNext = true, redDemo = false;
    uint8_t credits = 0;
    Developer developer;
    developerReset(&developer);
    const uint32_t frameTicks = CLOCKS_PER_SEC / 50;
    leagueLoad();
    gfx_Begin();
    drawTitle(credits);
    uint32_t lastFlash = clock(), lastPose = lastFlash;
    uint32_t stateTime = lastFlash, lastFrame = lastFlash;

    // If a key is pressed while launching just ignore it
    kb_Scan();
    uint8_t previousKeys[8];
    for (unsigned g = 1; g < 8; ++g) previousKeys[g] = kb_Data[g];
    bool coinWasDown = kb_IsDown(kb_Key5);
    bool twoPlayers = false;
    bool startWasDown = kb_IsDown(kb_Key1);
    bool jumpWasDown = kb_IsDown(kb_Key2nd);
    for (;;) {
        kb_Scan();
        if (kb_IsDown(kb_KeyClear))
            break;
        uint32_t now = clock();
        uint16_t pressedKey = 0;
        for (unsigned g = 1; g < 8; ++g) {
            uint8_t keys = kb_Data[g];
            uint8_t pressed = keys & ~previousKeys[g];
            if (pressed) {
                // Two fresh keys at once aren't a secret letter.
                pressedKey = pressedKey || (pressed & (pressed - 1))
                    ? 0xffff : (g << 8) | pressed;
            }
            previousKeys[g] = keys;
        }
        bool keyPressed = pressedKey != 0;
        bool codeCoin = screen == TITLE && developer.code == 2
            && pressedKey == kb_Key5;
        if (screen == TITLE && keyPressed) {
            char letter = pressedKey == kb_KeySquare ? 'I' : pressedKey == kb_KeyLn ? 'S'
                : pressedKey == kb_Key5 ? 'U' : pressedKey == kb_KeyPrgm ? 'C'
                : pressedKey == kb_KeyLParen ? 'K' : '?';
            if (developerLetter(&developer, letter)) {
                developerReset(&developer);
                gameGfxPrepare();
                openingDeveloperPreview(1, 3, 0, credits);
                screen = DEVELOPER;
                continue;
            }
            if (developer.code) stateTime = now;
        } else if (screen != TITLE) developer.code = 0;
        if (screen == OPENING && openingDeveloperActive()
            && keyPressed && kb_IsDown(kb_KeyDel)) {
            developer.stage = openingDeveloperStage();
            developer.row = 0;
            openingDeveloperPreview(developer.stage, developer.lives, 0, credits);
            screen = DEVELOPER;
            coinWasDown = kb_IsDown(kb_Key5); startWasDown = kb_IsDown(kb_Key1);
            jumpWasDown = kb_IsDown(kb_Key2nd);
            continue;
        }
        if (screen == DEVELOPER) {
            char key = !keyPressed ? 0 : kb_IsDown(kb_KeyUp) ? 'U'
                : kb_IsDown(kb_KeyDown) ? 'D' : kb_IsDown(kb_KeyLeft) ? 'L'
                : kb_IsDown(kb_KeyRight) ? 'R' : kb_IsDown(kb_KeyEnter) ? 'E' : 0;
            unsigned action = developerControl(&developer, key);
            if (action == 1) {
                openingDeveloperBegin(developer.stage, developer.lives, credits);
                openingDraw(credits); screen = OPENING; lastFrame = now;
            } else if (action == 2) {
                drawTitle(credits); screen = TITLE; pose = 0; logoIsGreen = true;
                stateTime = lastFlash = lastPose = now;
            } else if (key) openingDeveloperPreview(developer.stage, developer.lives, developer.row, credits);
            // Menu keys must not leak into coin/start/jump handling.
            coinWasDown = kb_IsDown(kb_Key5); startWasDown = kb_IsDown(kb_Key1);
            jumpWasDown = kb_IsDown(kb_Key2nd);
            continue;
        }
        bool coinDown = kb_IsDown(kb_Key5);
        bool startDown = kb_IsDown(kb_Key1);
        bool jumpDown = kb_IsDown(kb_Key2nd);
        bool startPressed = startDown && !startWasDown;
        bool jumpPressed = jumpDown && !jumpWasDown;
        bool creditsChanged = coinDown && !coinWasDown && !codeCoin && credits < 99;
        coinWasDown = coinDown;
        startWasDown = startDown;
        jumpWasDown = jumpDown;
        if (creditsChanged) {
            ++credits;
            if (screen == HIGH_SCORES || screen == INSTRUCTIONS)
                titlePageCredits(credits);
            else if (screen == TITLE || screen == HEARTS || screen == PLAYER_START) {
                drawCredits(credits);
                if (screen != PLAYER_START)
                    gfx_PrintStringXY("SELECT GAME", 8, 224);
            }
        }

        bool canStart = screen != PLAYER_START
            && !(screen == BLACK && next == PLAYER_START)
            && screen != OPENING;
        bool pairStart = pressedKey == kb_Key2 && credits >= 2;
        if (canStart && (pairStart || (startPressed && credits))) {
            twoPlayers = pairStart;
            credits -= pairStart ? 2 : 1;
            next = PLAYER_START;
            blankTicks = 10;
            screen = BLACK;
            stateTime = now;
            drawBlack();
        }

        if (screen == BLACK) {
            if (now - stateTime < blankTicks * frameTicks)
                continue;
            screen = next;
            // Black is an actual timed state, not a delay that locks the keys.
            gfx_SetTextScale(1, 1);
            gfx_SetTextBGColor(0);
            gfx_SetTextTransparentColor(0);
            if (screen == TITLE) {
                pose = 0;
                logoIsGreen = true;
                drawTitle(credits);
                lastFlash = lastPose = clock();
            } else if (screen == HIGH_SCORES || screen == INSTRUCTIONS) {
                titlePageDraw(screen == INSTRUCTIONS, credits);
            } else if (screen == PLAYER_START) {
                drawPlayerStart(credits);
                gameGfxPrepare(); // PLAYER 1 keeps everyone busy while the graphics unpack into 8 bpp mode
            } else if (screen == ATTRACT) {
                gameGfxPrepare();
                openingAttractBegin(credits, redDemo);
                openingDraw(credits);
                lastFrame = clock();
            }
            stateTime = clock();
            continue;
        }
        if (screen == PLAYER_START) {
            if (now - stateTime >= CLOCKS_PER_SEC) {
                openingStartGame(credits, twoPlayers);
                openingDraw(credits);
                screen = OPENING;
                lastFrame = clock();
            }
            continue;
        }
        if (screen == OPENING || screen == ATTRACT) {
            if (screen == OPENING)
                openingInput(kb_IsDown(kb_KeyLeft), kb_IsDown(kb_KeyRight), jumpPressed);
            uint32_t ticks = (now - lastFrame) / frameTicks;
            bool changed = creditsChanged, finished = false;
            if (ticks) {
                lastFrame += ticks * frameTicks;
                if (ticks > 5)
                    ticks = 5;
                while (ticks--) {
                    if (screen == ATTRACT) {
                        finished = openingAttractUpdate();
                        changed = true;
                        if (finished)
                            break;
                    } else {
                        changed |= openingUpdate();
                    }
                }
            }
            if (finished) {
                redDemo = true;
                demoNext = false;
                next = HIGH_SCORES;
                blankTicks = 32;
            } else if (screen == OPENING && openingReturnToTitle()) {
                if (openingScoreEligible()) leagueEnter(openingScoreHundreds(), &credits);
                titlePagesInit();
                coinWasDown = kb_IsDown(kb_Key5);
                startWasDown = kb_IsDown(kb_Key1);
                jumpWasDown = kb_IsDown(kb_Key2nd);
                now = clock();
                if (openingResumePartner()) {
                    for (unsigned g = 1; g < 8; ++g) previousKeys[g] = kb_Data[g];
                    openingDraw(credits);
                    lastFrame = now;
                    continue;
                }
                next = HIGH_SCORES;
                blankTicks = 10;
                demoNext = true;
            } else {
                if (changed)
                    openingDraw(credits);
                continue;
            }
        } else if (screen == HIGH_SCORES) {
            if (jumpPressed) {
                next = INSTRUCTIONS;
                blankTicks = 10;
            } else if (now - stateTime >= 6 * CLOCKS_PER_SEC) {
                next = demoNext ? ATTRACT : TITLE;
                blankTicks = demoNext ? 10 : 5;
            } else {
                continue;
            }
        } else if (screen == INSTRUCTIONS) {
            if (now - stateTime < 10 * CLOCKS_PER_SEC)
                continue;
            next = HIGH_SCORES;
            blankTicks = 32;
        } else if (screen == HEARTS) {
            unsigned target = (now - stateTime) / (2 * frameTicks);
            if (target > 24)
                target = 24;
            titleHeartsDraw(heartsDrawn, target);
            heartsDrawn = target;
            if (target < 24)
                continue;
            demoNext = true;
            next = HIGH_SCORES;
            blankTicks = 32;
        } else if (screen == TITLE) {
            uint32_t flashes = (now - lastFlash) / (8 * frameTicks);
            if (flashes) {
                lastFlash += flashes * 8 * frameTicks;
                if (flashes & 1)
                    logoIsGreen = !logoIsGreen;
                gfx_palette[LOGO_COLOR] = logoIsGreen
                    ? gfx_RGBTo1555(0, 255, 0) : gfx_RGBTo1555(255, 0, 255);
            }
            if (now - lastPose >= 2 * CLOCKS_PER_SEC) {
                lastPose = now;
                const TitlePose *old = &poses[pose];
                restoreBackground(old->quasiX, old->quasiY, bigfatquasi_width, bigfatquasi_height);
                restoreBackground(old->esmieX, old->esmieY, esmie_width, esmie_height);
                pose = (pose + 1) % 3;
                drawActors(&poses[pose]);
            }
            if (now - stateTime >= 640 * frameTicks) {
                // Have the logo freeze while the hearts be drawing (Like in the coin op)
                screen = HEARTS;
                heartsDrawn = 0;
                stateTime = now;
            }
            continue;
        } else {
            continue;
        }
        screen = BLACK;
        stateTime = now;
        drawBlack();
    }
    gfx_End();
    if (!leagueSave()) {
        puts("HUNCHSAV save/archive failed.");
        return 1;
    }
    return 0;
}
