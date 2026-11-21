#include <stdint.h>
#include <graphx.h>
#include "guards.h"
#include "game_gfx.h"
#include "knight.h"

static const uint8_t guardX[3] = {96, 152, 208};
static uint8_t pose[3], delay[3];

void guardsReset(void)
{
    for (unsigned i = 0; i < 3; ++i)
        pose[i] = 0;
    // Staggered like the ROM. Zero wraps to 255 on the first countdown tick.
    delay[0] = 32;
    delay[1] = 16;
    delay[2] = 0;
}

void guardsUpdate(void)
{
    // Rest, lift, poke some butt, lower. A full cycle takes 82 ticks.
    static const uint8_t duration[4] = {64, 5, 7, 6};
    for (unsigned i = 0; i < 3; ++i) {
        if (knightPassed(guardX[i])) {
            pose[i] = 0; // Bricked in. His butt-skewering shift is over.
            continue;
        }
        if (--delay[i] == 0) {
            pose[i] = (pose[i] + 1) & 3;
            delay[i] = duration[pose[i]];
        }
    }
}

bool guardsHit(int playerX8, int playerFeet)
{
    // Red spear tips in the three ROM pictures are at y=85, 81 and 72.
    static const uint8_t tipY[4] = {85, 81, 72, 81};
    int left = playerX8 / 8 + 4;
    int right = playerX8 / 8 + 12;
    for (unsigned i = 0; i < 3; ++i) {
        int spearX = guardX[i] + 1;
        if (!knightPassed(guardX[i]) && right > spearX && left < spearX + 1
            && playerFeet > tipY[pose[i]] && playerFeet - 20 < 96)
            return true;
    }
    return false;
}

void guardsDrawOffset(int offset)
{
    for (unsigned i = 0; i < 3; ++i) {
        unsigned frame = pose[i] == 3 ? 1 : pose[i];
        gfx_sprite_t *guard = (gfx_sprite_t *)pitguardCache[frame];
        if (knightPassed(guardX[i])) {
            guard = (gfx_sprite_t *)pitguardCache[0];
            // Rest below the new bricks, with the spear hidden by the ceiling.
            gfx_region_t clip = {0, 0, 320, 240};
            if (gfx_GetClipRegion(&clip) && clip.ymax > 96) {
                gfx_SetClipRegion(clip.xmin, clip.ymin < 96 ? 96 : clip.ymin,
                                  clip.xmax, clip.ymax);
                gfx_TransparentSprite(guard, guardX[i] + offset, 72);
                gfx_SetClipRegion(clip.xmin, clip.ymin, clip.xmax, clip.ymax);
            }
        } else {
            gfx_TransparentSprite(guard, guardX[i] + offset, 72);
        }
    }
}

void guardsDraw(void)
{
    guardsDrawOffset(0);
}
