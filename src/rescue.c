#include <string.h>
#include <graphx.h>
#include "rescue.h"
#include "game_gfx.h"
#include "gfx/gfx.h"
#include "rescue_art.h"

Rescue rescue;
static uint8_t esmieCache[386], heartCache[146], smallBellCache[66], helpCache[258];
static uint8_t heartbreakCache[386], heartbreakFrame = 255;
static const uint16_t ropeX[2] = {52, 260};
static const uint8_t groupX[3] = {88, 144, 200};

static void expand(const uint8_t *p, uint8_t *out, unsigned w, unsigned h,
                   const uint8_t *colors)
{
    *out++ = w; *out++ = h;
    for (unsigned i = 0; i < w * h; ++i)
        *out++ = colors[(p[i / 4] >> (6 - (i % 4) * 2)) & 3];
}

void rescueArtPrepare(void)
{
    heartbreakFrame = 255;
    static const uint8_t ec[4] = {32, 33, 38, 39};
    static const uint8_t hc[4] = {32, 33, 39, 32};
    static const uint8_t helpColors[4] = {32, 33, 35, 38};
    expand(rescueHelp, helpCache, 16, 16, helpColors);
    static const uint8_t bc[4] = {32, 34, 38, 39};
    expand(esmie_data, esmieCache, 16, 24, ec);
    expand(rescueHeart, heartCache, 16, 9, hc);
    expand(rescueBell, smallBellCache, 8, 8, bc);
}

void rescueDrawHeart(int x, int y, bool broken)
{
    gfx_TransparentSprite((gfx_sprite_t *)heartCache, x, y);
    if (broken) {
        gfx_SetColor(32);
        gfx_Line(x + 8, y, x + 6, y + 3);
        gfx_Line(x + 6, y + 3, x + 9, y + 5);
        gfx_Line(x + 9, y + 5, x + 7, y + 8);
    }
}
static void drawHeartbreak(unsigned frame, int x, int y)
{
    if (frame >= 3) return; // Last ROM frame is empty. She's had enough.
    if (heartbreakFrame != frame) {
        static const uint8_t colors[4] = {32, 33, 39, 32};
        expand(rescueHeartbreak[frame], heartbreakCache, 16, 24, colors);
        heartbreakFrame = frame;
    }
    gfx_TransparentSprite((gfx_sprite_t *)heartbreakCache, x, y);
}

void rescueDrawHelp(int x, int y)
{
    gfx_TransparentSprite((gfx_sprite_t *)helpCache, x, y);
}

void rescueDrawEsmie(int x, int y)
{
    gfx_TransparentSprite((gfx_sprite_t *)esmieCache, x, y);
}

void rescueReset(void)
{
    memset(&rescue, 0, sizeof rescue);
    rescue.x8 = 72 * 8;
    rescue.feet8 = 184 * 8;
    rescue.held = rescue.lastRope = rescue.armed = -1;
    rescue.ropeOffset[0] = rescue.ropeOffset[1] = 128;
    rescue.ropeDelay[0] = 16;
    rescue.ropeDelay[1] = 240;
}

// ROM $6DFD: two pause/up/pause/down cycles, with staggered first delays.
static void ropesTick(void)
{
    for (int i = 1; i >= 0; --i) {
        if (rescue.ropeDelay[i]) { --rescue.ropeDelay[i]; continue; }
        unsigned n = rescue.ropeIndex[i];
        int delta = n >= 2 && n < 30 ? -2 : n >= 34 && n < 62 ? 2 : 0;
        rescue.ropeIndex[i] = (n + 1) & 63;
        rescue.ropeDelay[i] = 8;
        rescue.ropeOffset[i] += delta;
        if (!delta) break;
    }
}

static bool floorSupports(unsigned floor, int x8)
{
    int x = x8 / 8;
    for (int foot = 5; foot <= 11; foot += 6) {
        int px = x + foot;
        if (px < 72 || px >= 240) continue;
        bool hole = false;
        for (unsigned g = 0; g < 3; ++g)
            if ((rescue.cleared[floor] & (1u << g))
                && px >= groupX[g] && px < groupX[g] + 24) hole = true;
        if (!hole) return true;
    }
    return false;
}

unsigned rescueUpdate(bool left, bool right, bool jump, const uint8_t *arc)
{
    Rescue *r = &rescue;
    ropesTick();
    // Five Esmies, 32 steps each, one step every 38 video ticks.
    if (++r->clock38 == 38) { r->clock38 = 0; ++r->progress; }
    if (r->progress >= 160) return 1;
    int direction = (int)right - (int)left;
    if (r->held >= 0) {
        r->feet8 = r->ropeOffset[r->held] * 8 - r->grip8;
        if (r->feet8 < 60 * 8) {
            r->feet8 = 60 * 8;
            r->grip8 = r->ropeOffset[r->held] * 8 - r->feet8;
        }
        r->frame = 7;
        if (!jump) return 0;
        r->lastRope = r->held; r->held = -1;
        // Aim at the nearest storey. Missing it is not a shortcut downstairs.
        int target = (204 - r->feet8 / 8) / 40;
        r->floor = target < 0 ? 0 : target > 3 ? 3 : target;
        r->airborne = true; r->jumpTick = 0; r->jumpStart8 = r->feet8;
    } else if (jump && !r->airborne) {
        r->airborne = true; r->jumpTick = 0; r->jumpStart8 = r->feet8;
    }
    if (direction) {
        r->facingLeft = direction < 0;
        r->x8 += direction * 4;
        if (!r->airborne) r->frame = (++r->walkTick / 5) & 3;
    }
    if (r->x8 < 40 * 8) r->x8 = 40 * 8;
    if (r->x8 > 257 * 8) r->x8 = 257 * 8;
    int oldFeet = r->feet8;
    if (r->airborne) {
        if (r->jumpTick < 48) {
            ++r->jumpTick;
            r->feet8 = r->jumpStart8 - arc[r->jumpTick] * 8;
            r->velocity8 = r->feet8 - oldFeet;
        } else {
            r->velocity8 += 4;
            if (r->velocity8 > 40) r->velocity8 = 40;
            r->feet8 += r->velocity8;
        }
        r->frame = 4 + r->jumpTick * 3 / 49;
        if (r->velocity8 >= 0) {
            int f = r->floor;
            int y8 = (184 - f * 40) * 8;
            if (oldFeet <= y8 && r->feet8 >= y8 && floorSupports(f, r->x8)) {
                if (r->floor != f) r->armed = -1;
                r->feet8 = y8; r->floor = f; r->airborne = false;
                r->lastRope = -1; r->velocity8 = 0;
                r->frame = (r->walkTick / 5) & 3;
            }
        }
    }
    // Ropes can be caught along their length; 2nd releases with the normal arc.
    for (unsigned i = 0; i < 2; ++i) {
        int hand = r->facingLeft ? 3 : 12;
        int dx = r->x8 / 8 + hand - ropeX[i];
        // ROM $2B40 blocks catching during a jump, not until you shuffle sideways.
        if (r->lastRope == (int)i && r->jumpTick >= 48) r->lastRope = -1;
        if (r->lastRope != (int)i && dx >= -3 && dx <= 3
            && r->feet8 / 8 - 12 >= 48
            && r->feet8 / 8 - 12 < r->ropeOffset[i] + 44) {
            r->held = i; r->grip8 = r->ropeOffset[i] * 8 - r->feet8;
            r->x8 = (ropeX[i] - hand) * 8; r->frame = 7; r->armed = -1;
            return 0;
        }
    }
    // Jumping over a bell still pulls it out. Quasi isn't getting out of chores.
    int floorY8 = (184 - r->floor * 40) * 8;
    bool overFloor = !r->airborne || (r->jumpStart8 == floorY8
        && r->feet8 <= floorY8 && r->jumpTick <= 48);
    if (overFloor) {
        int centre = r->x8 / 8 + 8;
        if (r->armed >= 0) {
            int g = r->armed;
            if (centre < groupX[g] || centre >= groupX[g] + 24) {
                r->cleared[r->floor] |= 1u << g;
                r->armed = -1;
            }
        }
        for (unsigned g = 0; g < 3; ++g)
            if (!(r->cleared[r->floor] & (1u << g))
                && centre >= groupX[g] + 8 && centre < groupX[g] + 16) r->armed = g;
        if (!r->airborne && !floorSupports(r->floor, r->x8)) {
            // Meeting Claude Frollo. An unopened rivet downstairs won't save him.
            return 1;
        }
    } else r->armed = -1;
    // Give a vertical jump its rope re-grab above, then commit a miss to death.
    // Meeting Claude Frollo. Only the intended floor gets a landing check.
    if (r->airborne && r->jumpTick >= 48 && r->feet8 >= floorY8) return 1;
    if (r->feet8 > 208 * 8) return 1;
    if ((r->cleared[0] & r->cleared[1] & r->cleared[2] & r->cleared[3]) == 7) return 2;
    return 0;
}

void rescueDraw(int offset, bool occupants)
{
    // The CE borders use existing bricks. Nothing fancy for the eZ80 to unpack.
    gfx_SetColor(32); gfx_FillRectangle(offset, 0, 320, 192);
    for (unsigned y = 0; y < 192; y += 8) {
        if (y < 168) for (unsigned x = y < 88 ? 24 : 0; x < 32; x += 8)
            gfx_Sprite(castletilesSprites[30], offset + x, y);
        for (unsigned x = 280; x < 320; x += 8)
            gfx_Sprite(castletilesSprites[30], offset + x, y);
    }
    // Someone forgot to water this corner. Put the meadow back.
    static const uint8_t meadow[4] = {38, 39, 42, 42};
    for (unsigned x = 0; x < 4; ++x) {
        gfx_Sprite(castletilesSprites[meadow[x]], offset + x * 8, 160);
        for (unsigned y = 168; y < 192; y += 8)
            gfx_Sprite(castletilesSprites[45], offset + x * 8, y);
    }
    for (unsigned f = 0; f < 4; ++f) for (unsigned x = 72; x < 240; x += 8) {
        int group = -1;
        for (unsigned g = 0; g < 3; ++g) if (x >= groupX[g] && x < groupX[g] + 24u) group = g;
        if (group >= 0 && (rescue.cleared[f] & (1u << group))) continue;
        gfx_Sprite(group < 0 ? castletilesSprites[23] : (gfx_sprite_t *)smallBellCache,
                   offset + x, 184 - f * 40);
    }
    gfx_SetColor(38); gfx_HorizLine(offset + 40, 32, 228);
    for (unsigned i = 0; i < 2; ++i) {
        gfx_TransparentSprite(game_bell, offset + ropeX[i] - 8, 32);
        gfx_SetColor(39);
        gfx_VertLine(offset + ropeX[i], 48, rescue.ropeOffset[i] + 44 - 48);
    }
    if (occupants) for (unsigned i = rescue.progress / 32; i < 5; ++i) {
        if (i == rescue.progress / 32 && rescue.progress % 32 >= 28)
            drawHeartbreak(rescue.progress % 32 - 28, offset + 64 + (4 - i) * 16, 8);
        else rescueDrawEsmie(offset + 64 + (4 - i) * 16, 8);
    }
}

static int orbit(unsigned phase)
{
    unsigned q = (phase >> 6) & 3, n = phase & 63;
    if (q & 1) n = 64 - n;
    int value = heartSine[n];
    return q >= 2 ? -value : value;
}
void rescueCelebration(unsigned tick)
{
    gfx_SetClipRegion(0, 0, 320, 240);
    gfx_FillScreen(32);
    rescueDrawEsmie(112, 118);
    gfx_SetTextFGColor(39); gfx_PrintStringXY("MY HERO!", 92, 100);
    for (unsigned i = 0; i < 16; ++i) {
        unsigned p = tick + i * 16;
        rescueDrawHeart(152 + orbit(p), 122 + orbit(p + 64) * 3 / 4, false);
    }
}
