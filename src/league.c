#include <fileioc.h>
#include <graphx.h>
#include <keypadc.h>
#include <string.h>
#include <time.h>
#include "title_pages.h"
#include "league.h"

LeagueEntry league[LEAGUE_COUNT];
static bool dirty;
enum { SAVE_SIZE = 76 };

static uint16_t checksum(const uint8_t *data, unsigned size)
{
    uint16_t sum = 0;
    for (unsigned i = 0; i < size - 2; ++i)
        sum = (uint16_t)(sum * 33u + data[i]);
    return sum;
}

static bool validLetter(char c)
{
    return c == ' ' || c == '.' || (c >= 'A' && c <= 'Z');
}

void leagueLoad(void)
{
    dirty = false;
    for (unsigned i = 0; i < LEAGUE_COUNT; ++i) {
        league[i].hundreds = 500; // 500 hundreds = 50,000 points.
        memset(league[i].name, ' ', LEAGUE_NAME);
        league[i].name[LEAGUE_NAME] = '\0';
    }

    uint8_t data[SAVE_SIZE];
    uint8_t file = ti_Open("HUNCHSAV", "r");
    if (!file) return; // First run. The league hasn't hired anyone yet.
    unsigned size = ti_GetSize(file);
    bool ok = size == SAVE_SIZE || size == 41;
    if (ok) ok = ti_Read(data, 1, size, file) == size;
    ti_Close(file);
    if (!ok) return;
    unsigned letters = size == 41 ? 3 : LEAGUE_NAME;
    if (memcmp(data, size == 41 ? "HNC1" : "HNC2", 4)) return;
    uint16_t saved = data[size - 2] | ((uint16_t)data[size - 1] << 8);
    if (saved != checksum(data, size)) return;

    LeagueEntry loaded[LEAGUE_COUNT];
    for (unsigned i = 0; i < LEAGUE_COUNT; ++i) {
        unsigned p = 4 + i * (4 + letters);
        uint32_t score = 0;
        for (unsigned b = 0; b < 4; ++b)
            score |= (uint32_t)data[p + b] << (8 * b);
        if (score > 0xFFFFFFUL) return;
        if (i && score > loaded[i - 1].hundreds) return;
        loaded[i].hundreds = score;
        memset(loaded[i].name, ' ', LEAGUE_NAME);
        for (unsigned n = 0; n < letters; ++n) {
            char c = data[p + 4 + n];
            if (!validLetter(c)) return;
            loaded[i].name[n] = c;
        }
        loaded[i].name[LEAGUE_NAME] = '\0';
    }
    memcpy(league, loaded, sizeof league);
}

bool leagueSave(void)
{
    if (!dirty) return true;
    uint8_t data[SAVE_SIZE];
    memcpy(data, "HNC2", 4);
    for (unsigned i = 0; i < LEAGUE_COUNT; ++i) {
        unsigned p = 4 + i * (4 + LEAGUE_NAME);
        for (unsigned b = 0; b < 4; ++b)
            data[p + b] = league[i].hundreds >> (8 * b);
        memcpy(data + p + 4, league[i].name, LEAGUE_NAME);
    }
    uint16_t sum = checksum(data, SAVE_SIZE);
    data[SAVE_SIZE - 2] = sum;
    data[SAVE_SIZE - 1] = sum >> 8;
    uint8_t file = ti_Open("HUNCHSAV", "w");
    if (!file) return false;
    bool ok = ti_Write(data, 1, SAVE_SIZE, file) == SAVE_SIZE;
    // Called after gfx_End(): archive garbage collection may use the OS screen.
    if (ok) ok = ti_SetArchiveStatus(true, file) != 0;
    ti_Close(file);
    if (ok) dirty = false;
    return ok;
}

unsigned leagueRank(uint32_t hundreds)
{
    for (unsigned i = 0; i < LEAGUE_COUNT; ++i)
        if (hundreds >= league[i].hundreds) return i;
    return LEAGUE_COUNT;
}

void leagueScoreText(char *out, uint32_t hundreds)
{
    char reverse[8];
    unsigned length = 0;
    if (!hundreds) {
        strcpy(out, "0");
        return;
    }
    do {
        reverse[length++] = '0' + hundreds % 10;
        hundreds /= 10;
    } while (hundreds);
    while (length) *out++ = reverse[--length];
    *out++ = '0';
    *out++ = '0';
    *out = '\0';
}

// Both screens use the same table. The pending entry is only a preview.
static void text(const char *s, int x, int y, uint8_t color)
{
    gfx_SetTextFGColor(color);
    gfx_PrintStringXY(s, x, y);
}

void leagueDrawTable(unsigned rank, uint32_t hundreds, const char *name)
{
    static const uint8_t colors[] = {23, 26, 24, 27, 25};
    char score[11];
    text("HI SCORE", 136, 8, 21);
    leagueScoreText(score, rank == 0 ? hundreds : league[0].hundreds);
    text(score, 128, 16, 21);
    text("LEAGUE", 64, 24, 25);
    text("ORDER", 64, 32, 25);
    text("SCORE", 120, 32, 25);
    text("NAME", 232, 32, 25);
    for (unsigned i = 0; i < LEAGUE_COUNT; ++i) {
        unsigned source = i > rank ? i - 1 : i;
        char row[] = "NO1";
        row[2] += i;
        text(row, 72, 48 + i * 16, colors[i]);
        leagueScoreText(score, i == rank ? hundreds : league[source].hundreds);
        text(score, 120, 48 + i * 16, colors[i]);
        text(i == rank ? name : league[source].name, 232, 48 + i * 16, colors[i]);
    }
}

// Each row has nine characters and one command
static const char grid[] = "ABCDEFGHI JKLMNOPQR STUVWXYZ. ";

enum { UP = 1, DOWN = 2, LEFT = 4, RIGHT = 8, SELECT = 16,
       CANCEL = 32, COIN = 64 };

static unsigned moveCell(unsigned cell, uint8_t direction)
{
    if (direction & LEFT) return (cell + 29) % 30;
    if (direction & RIGHT) return (cell + 1) % 30;
    if (direction & UP) return (cell + 20) % 30;
    return (cell + 10) % 30;
}

static bool chooseCell(char *name, unsigned *cursor, unsigned cell)
{
    if (cell == 29) return true; // END: the league has heard enough.
    if (cell == 19) {
        // Rub rub rub to the left
        name[*cursor] = ' ';
        if (*cursor) --*cursor;
    } else {
        name[*cursor] = grid[cell];
        if (*cursor < LEAGUE_NAME - 1) ++*cursor;
    }
    return false;
}

static void drawRegistration(unsigned rank, uint32_t hundreds,
                             const char *name, unsigned cell,
                             unsigned remaining, uint8_t credits)
{
    // Sets up the whole high score name insertion time stuff
    gfx_Wait();
    gfx_FillScreen(0);
    leagueDrawTable(rank, hundreds, name);
    text("NAME REGISTRATION", 92, 128, 21);
    for (unsigned i = 0; i < 30; ++i) {
        char letter[] = {grid[i], '\0'};
        const char *label = i == 9 ? "SPACE" : i == 19 ? "RUB"
            : i == 29 ? "END" : letter;
        text(label, 64 + (i % 10) * 16, 156 + (i / 10) * 16,
             i == cell ? 23 : 21);
    }
    char timer[] = "TIME 00";
    timer[5] += remaining / 10;
    timer[6] += remaining % 10;
    text(timer, 192, 204, 23);
    char creditText[] = "CREDIT 00";
    creditText[7] += credits / 10;
    creditText[8] += credits % 10;
    text(creditText, 200, 224, 21);
    gfx_SwapDraw();
}

void leagueEnter(uint32_t hundreds, uint8_t *credits)
{
    unsigned rank = leagueRank(hundreds);
    if (rank == LEAGUE_COUNT) return;
    char name[LEAGUE_NAME + 1];
    memset(name, ' ', LEAGUE_NAME);
    name[LEAGUE_NAME] = '\0';
    unsigned cell = 0, cursor = 0, remaining = 30;
    uint8_t previous = 0, oldDirection = 0;
    bool redraw = true, ready = false;
    // Counts every 64 ticks at 50 hz
    const uint32_t timerPeriod = (uint32_t)CLOCKS_PER_SEC * 64 / 50;
    const uint32_t repeatPeriod = (uint32_t)CLOCKS_PER_SEC * 16 / 50;
    uint32_t start = clock(), lastMove = start;
    titlePagesInit();
    gfx_palette[0] = gfx_RGBTo1555(0, 0, 0);
    gfx_palette[21] = gfx_RGBTo1555(255, 255, 255);
    gfx_SetDrawBuffer();
    gfx_SetClipRegion(0, 0, 320, 240);
    gfx_SetTextScale(1, 1);
    gfx_SetTextBGColor(0);
    gfx_SetTextTransparentColor(0);
    for (;;) {
        uint32_t now = clock();
        unsigned elapsed = (now - start) / timerPeriod;
        if (elapsed >= 30) break;
        if (remaining != 30 - elapsed) {
            remaining = 30 - elapsed;
            redraw = true;
        }
        if (redraw) {
            drawRegistration(rank, hundreds, name, cell, remaining, *credits);
            redraw = false;
        }
        kb_Scan();
        uint8_t keys = (kb_IsDown(kb_KeyUp) ? UP : 0)
            | (kb_IsDown(kb_KeyDown) ? DOWN : 0)
            | (kb_IsDown(kb_KeyLeft) ? LEFT : 0)
            | (kb_IsDown(kb_KeyRight) ? RIGHT : 0)
            | (kb_IsDown(kb_Key2nd) ? SELECT : 0)
            | (kb_IsDown(kb_KeyClear) ? CANCEL : 0)
            | (kb_IsDown(kb_Key5) ? COIN : 0);
        // A held jump button isn't permission to name him AAAAAAAAAA.
        if (!ready) {
            if (!keys) ready = true;
            continue;
        }
        uint8_t pressed = keys & ~previous;
        previous = keys;
        if ((pressed & COIN) && *credits < 99) {
            ++*credits;
            redraw = true;
        }
        if (pressed & CANCEL) {
            do { kb_Scan(); } while (kb_IsDown(kb_KeyClear));
            gfx_Wait();
            gfx_SetDrawScreen();
            return;
        }
        uint8_t direction = keys & (UP | DOWN | LEFT | RIGHT);
        // Resolve simultaneous directions consistently.
        direction = direction & LEFT ? LEFT : direction & RIGHT ? RIGHT
            : direction & UP ? UP : direction & DOWN;
        if (direction && (direction != oldDirection || now - lastMove >= repeatPeriod)) {
            cell = moveCell(cell, direction);
            lastMove = now;
            redraw = true;
        }
        oldDirection = direction;
        if (pressed & SELECT) {
            if (chooseCell(name, &cursor, cell)) break;
            redraw = true;
        }
    }
    for (unsigned i = LEAGUE_COUNT - 1; i > rank; --i)
        league[i] = league[i - 1];
    league[rank].hundreds = hundreds;
    strcpy(league[rank].name, name);
    dirty = true;
    gfx_Wait();
    gfx_SetDrawScreen();
}