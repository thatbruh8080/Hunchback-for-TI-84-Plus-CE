// "GET OFF MY CASTLE" - Idk, Knight Stalker?
#include <graphx.h>
#include "knight.h"
#include "game_gfx.h"
#include "floor_map.h"

// The arcade updates the knight every 16 video ticks, not every player tick.
// He moves 8 pixels every 64 ticks
static uint8_t clock16, climbStep, weaponStep;
static int chaseX;
static bool chasing, hardGame;

void knightSetHard(bool hard) { hardGame = hard; }

void knightReset(void)
{
    clock16 = 1;
    climbStep = 0;
    weaponStep = 0;
    chaseX = 0;
    chasing = false;
}

void knightUpdate(void)
{
    if (++clock16 < (hardGame ? 8 : 16))
        return;
    clock16 = 0;
    if (!chasing) {
        if (++climbStep == 36) {
            chasing = true;
            weaponStep = 1;
        }
    } else {
        weaponStep = (weaponStep + 1) & 3;
        if (weaponStep == 0 && chaseX < 296)
            chaseX += 8;
    }
}

bool knightPassed(unsigned x)
{
    return climbStep >= 34 && (unsigned)(chaseX + 24) > x;
}

bool knightCaught(int playerX8)
{
    // The arcade checks horizontal progress here, not the player's jump height.
// You can't go over that guy in any way
    return climbStep >= 34 && playerX8 + 4 * 8 < (chaseX + 24) * 8;
}

void knightDraw(uint8_t level)
{
    if (climbStep < 34) {
        unsigned frame;
        if (climbStep < 28)
            frame = (climbStep & 2) ? 0 : 1;
        else if (climbStep < 32)
            frame = (climbStep & 2) ? 3 : 2;
        else
            frame = 4;
        // These original 16x24 blocks include the brickwork behind his feet.
        gfx_Sprite((gfx_sprite_t *)knightClimbCache[frame],
                   8, 136 - (climbStep / 4) * 8);
        return;
    }

    // Lay the original brick tile across every gap reached by the knight.
// Visual only. Quasi can't jump over that guy anyway so it's unnecessary to add collission to those bricks
    unsigned lastColumn = (chaseX + 24) / 8;
    for (unsigned column = 0; column < lastColumn; ++column) {
        if (!(floorMap[level][column / 8] & (1u << (column % 8))))
            gfx_Sprite((gfx_sprite_t *)knightBridgeCache, column * 8, 88);
    }
    unsigned frame = chasing && !(weaponStep & 2) ? 1 : 0;
    gfx_TransparentSprite((gfx_sprite_t *)knightChaseCache[frame], chaseX, 72);
}
