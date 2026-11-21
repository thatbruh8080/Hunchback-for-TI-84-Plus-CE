#include <graphx.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "gfx/gfx.h"
#include "opening.h"
#include "game_gfx.h"
#include "opening_map.h"
#include "level3_map.h"
#include "floor_map.h"
#include "scoring.h"
#include "rope.h"
#include "knight.h"
#include "guards.h"
#include "projectiles.h"
#include "rescue.h"
#include "stages.h"
#include "tower_map.h"

enum {
    SCENE_BASE = 32,
    SCENE_BLACK = SCENE_BASE,
    SCENE_RED = SCENE_BASE + 1,
    SCENE_GREEN = SCENE_BASE + 3,
    SCENE_MORTAR = SCENE_BASE + 5,
    SCENE_YELLOW = SCENE_BASE + 6,
    SCENE_WHITE = SCENE_BASE + 7,
    CASTLE_Y = 32,
    CASTLE_BOTTOM = 192,
    WALL_TOP = 88,
    GROUND_FEET = 190,
    QUASI_WIDTH = 16,
    QUASI_HEIGHT = 24,
    CENTER_X = (GFX_LCD_WIDTH - QUASI_WIDTH) / 2,
    START_X = 24,
    SCROLL_SPEED = 4,
    WALK_SPEED8 = 4, // Half a pixel per tick: 25 pixels/second
    HOP_TICKS = 24,
    HOP_HEIGHT = 20,
    CLIMB_TICKS = 50,
    MINIMAP_X = 72,
    MINIMAP_Y = 200
};

typedef enum {
    REVEAL_CASTLE,
    HOP_IN,
    SCROLL_TO_START,
    HOP_TO_WALL,
    JUMP_ONTO_WALL,
    LEVEL_READY,
    FALLING,
    FRIED,
    GAME_OVER,
    SUPER_BONUS,
    LEVEL_CLEAR,
    LEVEL_SCROLL,
    CASTLE_CLEAR,
    DEATH_WIPE,
    RESTART_CARD,
    RESTART_SLIDE,
    RESCUE_CELEBRATE,
    RESCUE_PAYOUT
} OpeningPhase;

static OpeningPhase phase;
static bool attractMode;
static bool developerRun, developerUnlimited, developerMenu;
static uint8_t developerRow;
static unsigned paradeTick;
static uint8_t paradeRefresh;
static int cameraX;

static int quasiX8;
static int quasiFeet;
static unsigned hopTick;
static unsigned climbTick;
static uint8_t quasiFrame;
static uint8_t lastFlippedFrame;
static uint8_t flippedQuasi[2 + QUASI_WIDTH * QUASI_HEIGHT];

// Gameplay positions and velocity use eighths of a pixel. Doesn't mean floating points
static int feet8, velocity8;
static int jumpStart8;
static bool onRope, releasedRope;
static uint8_t leftArrow[38];
static int8_t heldBell, lastBell;
static const uint8_t bellX[4] = {114, 142, 170, 198};
static int clearStartX8;
// One small background patch per draw buffer. The HUD deserves a break.
static uint8_t fallPatch[2][QUASI_WIDTH * QUASI_HEIGHT];
static int fallPatchY[2];
static unsigned fallBuffer;
static bool fallCached;
static uint8_t fireballAnimation;
static uint8_t jumpTick, flameTick, flameRandom;
static uint8_t flameFrame[15];

// Quasi jumps 18 pixels over 0.96 seconds
// Lookup table to spare the eZ80 from doing the same math over and over.
static const uint8_t jumpHeight[49] = {
    0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 14, 15, 15, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 16, 16, 16, 15, 15, 14, 13, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 1, 0
};
static Scoring scoring;
static unsigned clearAward, clearTick;
static uint8_t scoreDirty;
static uint8_t hudTick;
static unsigned transitionTick;
static uint8_t hudRefresh;
static uint8_t level, lives, walkTick, deathTick;
typedef enum { PIT_DROP, MISSED_JUMP, WEAPON_HIT, CAUGHT } FallCause;
static bool hitFall;
static bool paired;
static unsigned playerIndex;
static struct {
    Scoring score;
    uint8_t level, lives;
} partner;

static bool leftDown, rightDown, jumpQueued, airborne, facingLeft;

void openingInput(bool left, bool right, bool jumpPressed)
{
    leftDown = left;
    rightDown = right;
    if (phase == LEVEL_READY && jumpPressed)
        jumpQueued = true;
}

bool openingGameOver(void)
{
    return phase == GAME_OVER || phase == CASTLE_CLEAR;
}

bool openingReturnToTitle(void)
{
    return phase == GAME_OVER && transitionTick >= 150;
}

static bool stageHasRope(unsigned stage) { return (stage & 15) == 1 || (stage & 15) == 6; }
static bool stageHasBells(unsigned stage) { return (stage & 15) == 8 || (stage & 15) == 13; }
static bool stageHasGuards(unsigned stage) { return (stageHazards[stage & 31] & 1) != 0; }
static bool stageHasFire(unsigned stage) { return stageHasRope(stage) || stageHasBells(stage); }
static bool rescueStage(void) { return (level & 15) == 15; }
static bool towerStage(void) { return (level & 15) == 14; }
static bool ropeStage(void) { return stageHasRope(level); }
static bool guardStage(void) { return stageHasGuards(level); }
static bool bellStage(void) { return stageHasBells(level); }
static bool fireStage(void) { return stageHasFire(level); }
static bool fireballStage(void) { return stageShots[level] && (stageHazards[level] & 4); }
static bool arrowStage(void) { return stageShots[level] && !(stageHazards[level] & 4); }

static void stagePalette(unsigned stage)
{
    // ROM $0554: first circuit then the harder circuit's slightly rude paint job.
    unsigned c = (uint8_t)~stagePaint[stage & 31];
    unsigned g = (c >> 2) & 7, b = (c >> 5) & 7;
    gfx_palette[SCENE_BASE + 2] = gfx_RGBTo1555((c & 3) * 85,
        (g << 5) | (g << 2) | (g >> 1), (b << 5) | (b << 2) | (b >> 1));
}

static int projectileY(const Projectile *p)
{
    if (rescueStage()) return 184 - p->floor * 40 - ((p->flags & 128) ? 30 : 10);
    return (p->flags & 128) ? 78 : (fireballStage() ? 54 : 58);
}

static void awardPendingLife(void)
{
    if (scoring.extraLifePending) {
        if (!developerUnlimited) ++lives;
        scoring.extraLifePending = false;
    }
}

static bool supported(void)
{
    // Test both feet, not just the sprite centre, so a foot can land on an edge.
    unsigned left = (quasiX8 / 8 + 5) / 8;
    unsigned right = (quasiX8 / 8 + 11) / 8;
    return (left < 40 && (floorMap[level][left / 8] & (1u << (left % 8))))
        || (right < 40 && (floorMap[level][right / 8] & (1u << (right % 8))));
}

static void resetPlayer(void)
{
    quasiX8 = START_X * 8;
    quasiFeet = WALL_TOP;
    feet8 = WALL_TOP * 8;
    velocity8 = 0;
    quasiFrame = 0;
    walkTick = 0;
    airborne = false;
    facingLeft = false;
    jumpQueued = false;
    jumpTick = 0;
    jumpStart8 = WALL_TOP * 8;
    onRope = releasedRope = false;
    heldBell = lastBell = -1;
    fallCached = false;
    ropeReset();
    knightSetHard(level >= 16);
    knightReset();
    guardsReset();
    projectilesReset(level);
    fireballAnimation = 0;
    if (rescueStage()) {
        rescueReset();
        quasiX8 = rescue.x8; feet8 = rescue.feet8; quasiFeet = feet8 / 8;
    }
    scoringStartStage(&scoring);
    phase = LEVEL_READY;
}

static void fallStep(void)
{
    // $2842 is 1, 2, 3, 4, 5, 6, 7. Seven is plenty for this poor fellow.
    if (velocity8 < 7 * 8) velocity8 += 8;
    feet8 += velocity8;
    quasiFeet = feet8 / 8;
}

static void beginFall(FallCause cause)
{
    phase = FALLING;
    jumpQueued = false;
    onRope = false;
    hitFall = cause == WEAPON_HIT || cause == CAUGHT;
    deathTick = cause == WEAPON_HIT ? 0 : 50;
    velocity8 = cause == MISSED_JUMP ? 6 * 8 : 0;
    if (cause == PIT_DROP) fallStep();
}

static void updatePlayer(void)
{
    scoringTick(&scoring);
    projectilesUpdate();
    if (fireballStage()) fireballAnimation = (fireballAnimation + 3) % 25;
    if (heldBell >= 0) {
        quasiX8 = (bellX[heldBell] - (facingLeft ? 3 : 12)) * 8;
        feet8 = WALL_TOP * 8;
        quasiFeet = WALL_TOP;
        quasiFrame = 7;
        if (!jumpQueued) goto hazards;
        lastBell = heldBell;
        heldBell = -1;
        airborne = true;
        jumpStart8 = feet8;
        jumpTick = 0;
    }
    if (ropeStage())
        ropeUpdate();
    if (onRope) {
        // Align the hands on the bottom tip
        quasiX8 = ropeEndX8() - (facingLeft ? 3 : 12) * 8;
        feet8 = ropeEndY8() + 12 * 8;
        quasiFeet = feet8 / 8;
        quasiFrame = 7;
        if (!jumpQueued)
            return;
        onRope = false;
        releasedRope = true; // Can't immediately catch the same rope again n again
        airborne = true;
        jumpStart8 = feet8;
        jumpTick = 0;
    }
    
    int direction = (int)rightDown - (int)leftDown;
    if (jumpQueued && !airborne) {
        airborne = true;
        jumpTick = 0;
        jumpStart8 = feet8;
    }
    jumpQueued = false;
    if (direction) {
        facingLeft = direction < 0;
        quasiX8 += direction * WALK_SPEED8;
    }
    if (quasiX8 < 0)
        quasiX8 = 0;
    if (quasiX8 > (GFX_LCD_WIDTH - QUASI_WIDTH) * 8)
        quasiX8 = (GFX_LCD_WIDTH - QUASI_WIDTH) * 8;
    if (airborne) {
        int oldFeet8 = feet8;
        if (jumpTick < 48) {
            ++jumpTick;
            feet8 = jumpStart8 - jumpHeight[jumpTick] * 8;
            velocity8 = feet8 - oldFeet8;
        } else {
            // A release starts at the hand's actual height, not at the wall.
            // Continue descending if its arc ends above the landing surface.
            velocity8 += 4;
            if (velocity8 > 40)
                velocity8 = 40;
            feet8 += velocity8;
        }
        quasiFrame = 4 + jumpTick * 3 / 49;
        if (ropeStage() && !releasedRope) {
            int handX8 = quasiX8 + (facingLeft ? 3 : 12) * 8;
            int handY8 = feet8 - 12 * 8;
            int dx = handX8 - ropeEndX8();
            int dy = handY8 - ropeEndY8();
            // A small hand hitbox tolerates the two objects crossing between ticks.
            if (dx >= -6 * 8 && dx <= 6 * 8 && dy >= -8 * 8 && dy <= 8 * 8) {
                onRope = true;
                quasiFrame = 7;
                quasiX8 = ropeEndX8() - (facingLeft ? 3 : 12) * 8;
                feet8 = ropeEndY8() + 12 * 8;
                quasiFeet = feet8 / 8;
                return;
            }
        }
        if (bellStage() && velocity8 >= 0) {
            int handX8 = quasiX8 + (facingLeft ? 3 : 12) * 8;
            int handY8 = feet8 - 12 * 8;
            for (unsigned i = 0; i < 4; ++i) {
                int dx = handX8 - bellX[i] * 8;
                if ((int)i != lastBell && dx >= -6 * 8 && dx <= 6 * 8
                    && handY8 >= 68 * 8 && handY8 <= 80 * 8) {
                    heldBell = i;
                    quasiX8 = (bellX[i] - (facingLeft ? 3 : 12)) * 8;
                    feet8 = WALL_TOP * 8;
                    quasiFeet = WALL_TOP;
                    quasiFrame = 7;
                    goto hazards;
                }
            }
        }
        // Check crossing, not equality, so a fast fall cannot skip the floor.
        if (velocity8 >= 0 && oldFeet8 <= WALL_TOP * 8 && feet8 >= WALL_TOP * 8) {
            if (supported()) {
                feet8 = WALL_TOP * 8;
                airborne = false;
                releasedRope = false;
                lastBell = -1;
                velocity8 = 0;
                quasiFrame = (walkTick / 5) % 4;
            } else {
                beginFall(PIT_DROP);
            }
        }
    } else if (!supported()) {
        beginFall(PIT_DROP);
    } else if (direction) {
        quasiFrame = (++walkTick / 5) % 4;
    }
    if (phase == LEVEL_READY && airborne && jumpTick == 48 && feet8 > WALL_TOP * 8)
        beginFall(MISSED_JUMP);
hazards:
    // With no horizontal input, keep the current walking frame.
    quasiFeet = feet8 / 8;
    if ((fireballStage() || arrowStage()) && phase == LEVEL_READY) {
        int x = quasiX8 / 8;
        for (unsigned i = 0; i < 2; ++i) {
            const Projectile *p = &projectiles[i];
            if (!p->flags) continue;
            int px = p->x8 / 8;
            int y = projectileY(p);
            int inset = fireballStage() ? 2 : 1;
            int top = fireballStage() ? 1 : 0;
            int height = fireballStage() ? 8 : 3;
            // Both boys can choose the same height. No reserved lanes here.
            if (x + 12 > px + inset && x + 4 < px + 12 - inset
                && quasiFeet > y + top && quasiFeet - 20 < y + height) {
                beginFall(WEAPON_HIT);
                break;
            }
        }
    }
    // The white bell rope is at x=300, spanning y=47 through y=79.
// Contact clears the level; reaching the screen edge does not.
    if (phase == LEVEL_READY && quasiX8 / 8 + 12 >= 300
        && quasiX8 / 8 + 4 <= 300 && quasiFeet > 47
        && quasiFeet - 20 < 80) {
        clearAward = scoringClearStage(&scoring, level + 1);
        awardPendingLife();
        hudRefresh = 2;
        scoreDirty = 2;
        clearTick = 0;
        jumpQueued = false;
        phase = scoring.bells == 5 ? SUPER_BONUS : LEVEL_CLEAR;
        if (phase == LEVEL_CLEAR)
            stagePalette(level + 1);
    }
}

static gfx_tilemap_t castle = {
    .map = (uint8_t *)openingMap,
    .tiles = castletilesSprites,
    .tile_height = 8,
    .tile_width = 8,
    .draw_height = OPENING_MAP_HEIGHT,
    .draw_width = 40,
    .type_width = gfx_tile_8_pixel,
    .type_height = gfx_tile_8_pixel,
    .height = OPENING_MAP_HEIGHT,
    .width = OPENING_MAP_WIDTH,
    .y_loc = CASTLE_Y,
    .x_loc = 0
};

// Draw only the requested horizontal slice of a castle map.
// Complete tiles are guaranteed to fit on screen, so they need no clipping.
// Only the partial tile at either end takes the slower clipped path.
static void drawCastleSpan(const uint8_t *map, unsigned mapWidth,
                           unsigned sourceX, unsigned screenX, unsigned width)
{
    if (!width)
        return;
    castle.map = (uint8_t *)map;
    castle.width = mapWidth;
    gfx_SetClipRegion(screenX, CASTLE_Y, screenX + width, CASTLE_BOTTOM);

    unsigned partial = sourceX & 7;
    if (partial) {
        unsigned count = 8 - partial;
        if (count > width)
            count = width;
        castle.x_loc = screenX;
        castle.draw_width = 1;
        gfx_Tilemap(&castle, sourceX, 0);
        sourceX += count;
        screenX += count;
        width -= count;
    }
    unsigned columns = width / 8;
    if (columns) {
        castle.x_loc = screenX;
        castle.draw_width = columns;
        gfx_Tilemap_NoClip(&castle, sourceX, 0);
        unsigned count = columns * 8;
        sourceX += count;
        screenX += count;
        width -= count;
    }
    if (width) {
        castle.x_loc = screenX;
        castle.draw_width = 1;
        gfx_Tilemap(&castle, sourceX, 0);
    }
}

static void drawStageSpan(unsigned stage, unsigned sourceX,
                          unsigned screenX, unsigned width)
{
    if ((stage & 15) == 15) {
        gfx_SetClipRegion(screenX, CASTLE_Y, screenX + width, CASTLE_BOTTOM);
        rescueDraw((int)screenX - (int)sourceX, true);
    } else if ((stage & 15) == 14)
        drawCastleSpan((const uint8_t *)towerMap, 40, sourceX, screenX, width);
    else if (stage == 0)
        drawCastleSpan((const uint8_t *)openingMap, OPENING_MAP_WIDTH,
                       sourceX, screenX, width);
    else if (stageHasFire(stage))
        drawCastleSpan((const uint8_t *)openingMap, OPENING_MAP_WIDTH,
                       320 + sourceX, screenX, width);
    else
        drawCastleSpan((const uint8_t *)level3Map, 40, sourceX, screenX, width);
}

static void drawCastle(void)
{
    unsigned slideX = phase == RESTART_SLIDE ? 320 - transitionTick * 8 : 0;
    if (phase == LEVEL_SCROLL) {
        unsigned offset = cameraX - level * 320;
        drawStageSpan(level, offset, 0, 320 - offset);
        drawStageSpan(level + 1, 0, 320 - offset, offset);
    } else if (phase >= LEVEL_READY) {
        drawStageSpan(level, 0, slideX, 320 - slideX);
    } else {
        drawCastleSpan((const uint8_t *)openingMap, OPENING_MAP_WIDTH,
                       cameraX, 0, 320);
    }
    castle.map = (uint8_t *)openingMap;
    castle.width = OPENING_MAP_WIDTH;
    castle.draw_width = 40;
    castle.x_loc = 0;
    gfx_SetClipRegion(0, CASTLE_Y, GFX_LCD_WIDTH, CASTLE_BOTTOM);
}

static void drawCredits(uint8_t credits)
{
    char text[] = "CREDIT 00";
    text[7] = '0' + credits / 10;
    text[8] = '0' + credits % 10;
    gfx_SetColor(SCENE_BLACK);
    gfx_FillRectangle(200, 224, 72, 8);
    gfx_SetTextFGColor(SCENE_WHITE);
    gfx_PrintStringXY(text, 200, 224);
}

// Five big and chonky digits below the SCORE
// Seven digits fit between the minimap's right edge and x=256.
static void drawScore(void)
{
    unsigned value = scoring.hundreds % 100000u;
    int x = 208;
    gfx_SetColor(SCENE_BLACK);
    gfx_FillRectangle(144, 208, 112, 16);
    gfx_Sprite(scoredigitsSprites[0], 240, 208);
    if (!value)
        return;
    gfx_Sprite(scoredigitsSprites[0], 224, 208);
    do {
        gfx_Sprite(scoredigitsSprites[value % 10], x, 208);
        value /= 10;
        x -= 16;
    } while (value && x >= 144);
}

// Format only the popup's small number, without sprintf or a large score multiply.
static void drawAward(int offset)
{
    char text[8];
    char *end = text + sizeof(text) - 1;
    char *number = end;
    unsigned value = clearAward;
    *number = '\0';
    *--number = '0';
    if (value) {
        *--number = '0';
        do {
            *--number = '0' + value % 10;
            value /= 10;
        } while (value);
    }
    gfx_PrintStringXY(number, offset + 48 - (int)(end - number) * 8, 101);
}

static void drawHud(uint8_t credits)
{
    // Original arcade graphics converted to 8 bpp
    gfx_Sprite(game_hud_super_bonus, rescueStage() ? 160 : 224, 0);
    gfx_Sprite(game_hud_score, 176, 192);
    if (!rescueStage()) {
        gfx_Sprite(game_bellrope, 16, 8);
        for (unsigned i = 0; i < scoring.bells; ++i)
            gfx_TransparentSprite(game_bell, 16 + i * 16, 8);
    }
    drawScore();

    gfx_SetTextScale(1, 1);
    gfx_SetTextFGColor(SCENE_WHITE);
    gfx_PrintStringXY("PLAYER ", 48, 224);
    // Spare lifecons are refreshed by openingDraw().
    gfx_SetColor(SCENE_YELLOW);
    gfx_Rectangle(65, 193, 78, 29);
    gfx_SetColor(SCENE_RED);
    gfx_Rectangle(66, 194, 76, 27);
    gfx_SetColor(SCENE_YELLOW);
    gfx_Rectangle(67, 195, 74, 25);
    drawCredits(credits);
}

static void drawMinimap(void)
{
    // Opaque drawing restores the old marker too. The castle is the arcade icon.
    gfx_Sprite(game_minicastle, MINIMAP_X, MINIMAP_Y);
    if (rescueStage()) {
        // Esmie is in the rescue room now. Keep the ramparts and the blinking dot.
        gfx_SetColor(SCENE_BLACK);
        gfx_FillRectangle(MINIMAP_X + 59, MINIMAP_Y, 5, 8);
    }
    if (!attractMode && phase >= LEVEL_READY && phase != RESTART_SLIDE && !((hudTick - 1u) & 16)) {
        // Stage 1 and 2 are four pixels apart in the footage, not world positions.
// Stage 3 continues that spacing until its marker is verified separately.
        gfx_SetColor(SCENE_GREEN);
        gfx_FillRectangle(MINIMAP_X + 1 + (level & 15) * 4, MINIMAP_Y + 6, 2, 2);
    }
}

static void beginScene(uint8_t credits, bool presentHud)
{
    paired = false;
    playerIndex = 0;
    projectilesSeed((uint32_t)clock());
    projectilesReset(0);
    // Opening prepping
    attractMode = false;
    developerRun = developerUnlimited = developerMenu = false;
    fallCached = false;
    heldBell = lastBell = -1;
    gfx_FlipSpriteY(game_arrow, (gfx_sprite_t *)leftArrow);
    hudTick = 0;
    level = 0;
    ropeReset();
    knightSetHard(level >= 16);
    knightReset();
    guardsReset();
    onRope = releasedRope = false;
    lives = 3;
    scoringNewGame(&scoring);
    flameTick = 0;
    flameRandom = 0xB8;
    for (unsigned i = 0; i < 15; ++i)
        flameFrame[i] = (i * 5 + i / 3) % 6;
    clearAward = 0;
    scoreDirty = 0;
    hudRefresh = 0;
    transitionTick = 0;
    leftDown = rightDown = jumpQueued = airborne = facingLeft = false;
    phase = REVEAL_CASTLE;
    cameraX = OPENING_CASTLE_WIDTH;
    quasiX8 = GFX_LCD_WIDTH * 8;
    quasiFeet = GROUND_FEET;
    hopTick = 0;
    climbTick = 0;
    quasiFrame = 4;
    lastFlippedFrame = 255;

    gfx_SetPalette(scene_palette, sizeof_scene_palette, SCENE_BASE);
    gfx_SetTransparentColor(SCENE_BLACK);
    gfx_SetTextBGColor(SCENE_BLACK);
    gfx_SetTextTransparentColor(SCENE_BLACK);
    gfx_SetDrawBuffer();
    gfx_FillScreen(SCENE_BLACK);
    drawHud(credits);
    drawMinimap();

    // Give both buffers the fixed HUD before the first scrolling frame.
    if (presentHud) gfx_BlitBuffer();
}

void openingStartGame(uint8_t credits, bool pair)
{
    beginScene(credits, true);
    paired = pair;
    partner.level = 0;
    partner.lives = pair ? 3 : 0;
    scoringNewGame(&partner.score);
}

// Swap just the progress that survives a lost life. Hazards restart per turn.
bool openingResumePartner(void)
{
    if (!paired || !partner.lives) return false;
    Scoring outgoing = scoring;
    uint8_t outgoingLives = lives, outgoingLevel = level;
    scoring = partner.score;
    lives = partner.lives;
    level = partner.level;
    partner.score = outgoing;
    partner.lives = outgoingLives;
    partner.level = outgoingLevel;
    playerIndex ^= 1;
    cameraX = level * 320;
    gfx_SetDrawBuffer();
    gfx_SetPalette(scene_palette, sizeof_scene_palette, SCENE_BASE);
    gfx_SetTextBGColor(SCENE_BLACK);
    gfx_SetTextTransparentColor(SCENE_BLACK);
    stagePalette(level);
    phase = RESTART_CARD;
    transitionTick = 0;
    scoreDirty = hudRefresh = 2;
    return true;
}

// Test runs get the whole game, but no seat in the league of order.
bool openingScoreEligible(void) { return !developerRun && !attractMode; }
uint32_t openingScoreHundreds(void)
{
    return scoring.hundreds;
}
bool openingDeveloperActive(void) { return developerRun; }
uint8_t openingDeveloperStage(void) { return level + 1; }

void openingDeveloperBegin(uint8_t stage, uint8_t count, uint8_t credits)
{
    // Build the menu backstage. Showing the empty HUD here caused the flicker.
    beginScene(credits, false);
    developerRun = true;
    developerUnlimited = count == 0;
    lives = count ? count : 3;
    level = stage >= 1 && stage <= 32 ? stage - 1 : 0;
    cameraX = level * 320;
    stagePalette(level);
    resetPlayer();
    hudRefresh = scoreDirty = 2;
}

void openingDeveloperPreview(uint8_t stage, uint8_t count, uint8_t row, uint8_t credits)
{
    if (!developerMenu || stage != level + 1)
        openingDeveloperBegin(stage, count, credits);
    developerUnlimited = count == 0;
    lives = count ? count : 3;
    hudRefresh = scoreDirty = 2;
    developerMenu = true;
    developerRow = row;
    openingDraw(credits);
}

static void drawDeveloperLabel(void)
{
    if (!developerRun) return;
    gfx_SetClipRegion(0, 0, 320, 240);
    gfx_SetColor(SCENE_BLACK);
    gfx_FillRectangle(0, 0, 112, 8);
    gfx_SetTextFGColor(SCENE_WHITE);
    gfx_PrintStringXY("DEVELOPER MODE", 0, 0);
}

static void drawDeveloperMenu(void)
{
    gfx_SetColor(SCENE_BLACK);
    gfx_FillRectangle(0, 0, 320, 32);
    gfx_FillRectangle(0, 32, 240, 32);
    gfx_SetTextFGColor(SCENE_WHITE);
    gfx_PrintStringXY("DEVELOPER MODE", 0, 0);
    char stageText[] = "STAGE: 00";
    stageText[7] = level < 9 ? ' ' : '0' + (level + 1) / 10;
    stageText[8] = '0' + (level + 1) % 10;
    gfx_PrintStringXY(stageText, 16, 16);
    char lifeText[] = "LIVES: 0";
    lifeText[7] = developerUnlimited ? '\x80' : '0' + lives;
    gfx_PrintStringXY(lifeText, 16, 24);
    gfx_PrintStringXY("START", 16, 40);
    gfx_PrintStringXY("QUIT", 16, 56);
    static const uint8_t rowY[] = {16, 24, 40, 56};
    unsigned y = rowY[developerRow];
    gfx_SetColor(SCENE_WHITE);
    // A little arrow. No extra font glyph just to point at his bad decisions.
    for (unsigned x = 0; x < 5; ++x) gfx_VertLine(x, y + x / 2, 7 - (x / 2) * 2);
}

void openingAttractBegin(uint8_t credits, bool redCastle)
{
    beginScene(credits, true);
    attractMode = true;
    paradeTick = 0;
    paradeRefresh = 2;
    if (redCastle)
        gfx_palette[SCENE_BASE + 2] = gfx_RGBTo1555(255, 0, 0);
}

// Let Quasi do his entrance. Then his entire extended family turns up.
bool openingAttractUpdate(void)
{
    if (phase != LEVEL_READY) {
        openingUpdate();
        return false;
    }
    ++paradeTick;
    // 608 ticks to cross the wider CE wall; 616 for the jumping/text finale.
    return paradeTick >= 608 + 616;
}

static void advanceHop(void)
{
    hopTick = (hopTick + 1) % HOP_TICKS;
    int height = 4 * HOP_HEIGHT * hopTick * (HOP_TICKS - hopTick)
                 / (HOP_TICKS * HOP_TICKS);
    quasiFeet = GROUND_FEET - height;
    quasiFrame = 4 + hopTick / (HOP_TICKS / 3);
}

bool openingUpdate(void)
{
    // Both blink for 16 ticks per half-cycle. The map dot follows one tick later.
    ++hudTick;
    // F0/F3, F1/F4 and F2/F (frame tiles) are pairs. Those animate independently
    if (fireStage() && phase == LEVEL_READY) {
        if (++flameTick >= 3) {
            flameTick = 0;
            flameRandom = (flameRandom >> 1) ^ ((flameRandom & 1) ? 0xB8 : 0);
            unsigned cell = flameRandom % 15;
            flameFrame[cell] = (flameFrame[cell] + 3) % 6;
        }
    }
    switch (phase) {
        case REVEAL_CASTLE:
            cameraX -= SCROLL_SPEED;
            if (cameraX <= OPENING_CASTLE_WIDTH - GFX_LCD_WIDTH) {
                cameraX = OPENING_CASTLE_WIDTH - GFX_LCD_WIDTH;
                phase = HOP_IN;
            }
            break;

        case HOP_IN:
            quasiX8 -= 10;
            advanceHop();
            if (quasiX8 <= CENTER_X * 8) {
                quasiX8 = CENTER_X * 8;
                phase = SCROLL_TO_START;
            }
            break;

        case SCROLL_TO_START:
            cameraX -= SCROLL_SPEED;
            advanceHop();
            if (cameraX <= 0) {
                cameraX = 0;
                phase = HOP_TO_WALL;
            }
            break;

        case HOP_TO_WALL:
            quasiX8 -= 10;
            if (quasiX8 < START_X * 8)
                quasiX8 = START_X * 8;
            advanceHop();
            // Finish the current hop before jumping real high. Hey why didn't he jump real high to Esmie directly?
            if (quasiX8 == START_X * 8 && hopTick == 0) {
                climbTick = 0;
                phase = JUMP_ONTO_WALL;
            }
            break;

        case JUMP_ONTO_WALL:
            ++climbTick;
            // Quasi leaps onto the wall
            quasiFeet = GROUND_FEET
                - (GROUND_FEET - WALL_TOP) * (int)climbTick / CLIMB_TICKS
                - 160 * (int)climbTick * (CLIMB_TICKS - (int)climbTick)
                  / (CLIMB_TICKS * CLIMB_TICKS);
            quasiFrame = 4 + (climbTick * 3 / (CLIMB_TICKS + 1));
            if (climbTick >= CLIMB_TICKS) {
                resetPlayer();
            }
            break;

        case LEVEL_READY:
            if (rescueStage()) {
                RescueResult result = rescueUpdate(leftDown, rightDown, jumpQueued);
                jumpQueued = false;
                quasiX8 = rescue.x8; feet8 = rescue.feet8; quasiFeet = feet8 / 8;
                quasiFrame = rescue.frame; facingLeft = rescue.facingLeft;
                projectilesAim(rescueFloorAt(feet8), rescue.held);
                projectilesUpdate();
                for (unsigned i = 0; i < 2 && result == RESCUE_PLAYING; ++i) {
                    const Projectile *p = &projectiles[i];
                    if (!p->flags) continue;
                    int ax = p->x8 / 8, ay = projectileY(p), x = quasiX8 / 8;
                    if (x + 12 > ax + 1 && x + 4 < ax + 11
                        && quasiFeet > ay && quasiFeet - 20 < ay + 3) {
                        beginFall(WEAPON_HIT);
                        break;
                    }
                }
                if (result == RESCUE_HOLE) beginFall(PIT_DROP);
                else if (result == RESCUE_MISSED) beginFall(MISSED_JUMP);
                else if (result == RESCUE_TIMEOUT) beginFall(CAUGHT);
                else if (result == RESCUE_COMPLETE) {
                    // ROM $31AC rounds down, then pays each remaining Esmie separately.
                    rescue.progress = (rescue.progress & 0xE0) | 28;
                    scoring.bells = 0;
                    scoringPayStage(&scoring, 50);
                    awardPendingLife();
                    scoreDirty = 2;
                    transitionTick = 0; phase = RESCUE_PAYOUT;
                    gfx_SetClipRegion(0, 0, 320, 240);
                }
            } else {
                knightUpdate();
                if (guardStage()) guardsUpdate();
                updatePlayer();
                if (phase == LEVEL_READY) {
                    if (knightCaught(quasiX8)) beginFall(CAUGHT);
                    else if (guardStage() && guardsHit(quasiX8, quasiFeet))
                        beginFall(WEAPON_HIT);
                }
            }
            break;

        case RESCUE_PAYOUT:
            // ROM $31CC waits sixteen video ticks for each heartbreak frame.
            if (++transitionTick >= 16) {
                transitionTick = 0;
                ++rescue.progress;
                if (rescue.progress >= 160) {
                    phase = RESCUE_CELEBRATE;
                } else if (!(rescue.progress & 31)) {
                    rescue.progress |= 28;
                    scoringPayStage(&scoring, 50);
                    awardPendingLife();
                    scoreDirty = 2;
                }
            }
            break;

        case RESCUE_CELEBRATE:
            if (++transitionTick >= 250) {
                // Save the lovebirds. Then save your own butt all over again.
                level = 16; cameraX = level * 320;
                stagePalette(level); transitionTick = 0; phase = RESTART_SLIDE;
                hudRefresh = scoreDirty = 2;
            }
            break;

        case FALLING:
            if (hitFall) {
                // $08DC holds the hit pose for 50 VBlanks. A pit has no hit pause.
                if (deathTick < 50) { ++deathTick; return false; }
                // $08EC adds a second fall step on alternate frames after a hit.
                if (deathTick >= 52 && !(deathTick & 1)) fallStep();
                ++deathTick;
            }
            fallStep();
            if (quasiFeet >= GFX_LCD_HEIGHT + QUASI_HEIGHT) {
                deathTick = 0;
                if (!developerUnlimited) --lives;
                scoringLoseLife(&scoring);
                phase = FRIED;
            }
            break;

        case FRIED: {
            // Fever dream color flashing once disappears into the bottom of the screen
            static const uint16_t flashes[] = {
                gfx_RGBTo1555(255, 0, 255), gfx_RGBTo1555(0, 255, 255),
                gfx_RGBTo1555(255, 255, 0), gfx_RGBTo1555(255, 85, 85),
                gfx_RGBTo1555(85, 85, 255), gfx_RGBTo1555(0, 255, 0)
            };
            gfx_palette[SCENE_BASE + 4] = flashes[(deathTick / 5) % 6];
            if (++deathTick >= 110) {
                transitionTick = 0;
                phase = DEATH_WIPE;
            }
            break;
        }

        case DEATH_WIPE:
            if (++transitionTick >= 16) {
                gfx_SetPalette(scene_palette, sizeof_scene_palette, SCENE_BASE);
                stagePalette(level);
                transitionTick = 0;
                if (!lives) phase = GAME_OVER;
                else if (!openingResumePartner()) phase = RESTART_CARD;
            }
            break;

        case RESTART_CARD:
            if (++transitionTick >= 60) {
                transitionTick = 0;
                phase = RESTART_SLIDE;
            }
            break;

        case RESTART_SLIDE:
            if (++transitionTick >= 40) {
                resetPlayer();
                hudRefresh = 2;
            }
            break;

        case SUPER_BONUS:
            ++clearTick;
            // 21 flashes, four ticks each. Then cash in one bell every ten ticks.
            gfx_palette[SCENE_YELLOW] = (clearTick / 4) & 1
                ? gfx_RGBTo1555(255, 0, 0) : gfx_RGBTo1555(255, 255, 0);
            if (clearTick >= 84 && clearTick < 134 && (clearTick - 84) % 10 == 0) {
                scoringPayBell(&scoring);
                awardPendingLife();
                hudRefresh = scoreDirty = 2;
            }
            if (clearTick >= 134) {
                scoringPayStage(&scoring, clearAward);
                awardPendingLife();
                gfx_palette[SCENE_YELLOW] = gfx_RGBTo1555(255, 255, 0);
                stagePalette(level + 1);
                hudRefresh = scoreDirty = 2;
                clearTick = 0;
                phase = LEVEL_CLEAR;
            }
            break;

        case LEVEL_CLEAR:
            // Settle onto the wall before walking into the next section.
            if (clearTick >= 98 && quasiFeet < WALL_TOP) {
                quasiFeet += 2;
                if (quasiFeet > WALL_TOP) quasiFeet = WALL_TOP;
            }
            if (++clearTick >= 110) {
                clearStartX8 = quasiX8;
                facingLeft = false;
                onRope = false;
                heldBell = -1;
                phase = LEVEL_SCROLL;
                if (towerStage()) {
                    // The arcade relocates him to the bottom before this handover.
                    quasiFeet = 184; feet8 = 184 * 8;
                    rescueReset();
                }
            }
            break;

        case LEVEL_SCROLL:
            cameraX += SCROLL_SPEED;
            // Walk right in the world while the wall scrolls left underneath him.
            // Finish exactly at START_X: no little teleport at the handover.
            {
                int offset = cameraX - level * GFX_LCD_WIDTH;
                quasiX8 = clearStartX8 + ((320 + (towerStage() ? 72 : START_X)) * 8 - clearStartX8)
                    * offset / 320 - offset * 8;
                quasiFeet = towerStage() ? 184 : WALL_TOP;
                quasiFrame = (++walkTick / 5) % 4;
            }
            if (cameraX >= (level + 1) * GFX_LCD_WIDTH) {
                ++level;
                stagePalette(level);
                cameraX = level * GFX_LCD_WIDTH;
                uint8_t frame = quasiFrame, walking = walkTick;
                resetPlayer();
                quasiFrame = frame;
                walkTick = walking;
            }
            break;

        case GAME_OVER:
            // Three seconds to mourn his posture, then back to the title.
            if (transitionTick < 150)
                ++transitionTick;
            return false;

        case CASTLE_CLEAR:
            return false;
    }
    return true;
}

static void drawQuasi(void)
{
    if (developerMenu) return; // Let the castle pose for the menu on its own.
    gfx_sprite_t *sprite = quasiSprites[quasiFrame];
    if (phase < JUMP_ONTO_WALL || (phase >= LEVEL_READY && facingLeft)) {
        if (lastFlippedFrame != quasiFrame) {
            gfx_FlipSpriteY(sprite, (gfx_sprite_t *)flippedQuasi);
            lastFlippedFrame = quasiFrame;
        }
        sprite = (gfx_sprite_t *)flippedQuasi;
    }
    gfx_TransparentSprite(sprite, quasiX8 / 8, quasiFeet - QUASI_HEIGHT);
}

static void drawBellLadder(int offset)
{
    gfx_SetColor(SCENE_WHITE);
    for (unsigned i = 0; i < 4; ++i) {
        int x = offset + bellX[i];
        gfx_VertLine(x, 48, 40);
        gfx_TransparentSprite(game_bell, x - 8, 32);
    }
    gfx_Sprite(game_bellrope, offset + 104, 32);
    gfx_Sprite(game_bellrope, offset + 128, 32);
}

static void drawFallPatch(void)
{
    uint8_t *pixels = (uint8_t *)gfx_vbuffer;
    int x = quasiX8 / 8;
    for (int row = 0; row < QUASI_HEIGHT; ++row) {
        int y = fallPatchY[fallBuffer] + row;
        if (y >= 0 && y < GFX_LCD_HEIGHT)
            memcpy(pixels + y * GFX_LCD_WIDTH + x,
                   fallPatch[fallBuffer] + row * QUASI_WIDTH, QUASI_WIDTH);
    }
    fallPatchY[fallBuffer] = quasiFeet - QUASI_HEIGHT;
    for (int row = 0; row < QUASI_HEIGHT; ++row) {
        int y = fallPatchY[fallBuffer] + row;
        if (y >= 0 && y < GFX_LCD_HEIGHT)
            memcpy(fallPatch[fallBuffer] + row * QUASI_WIDTH,
                   pixels + y * GFX_LCD_WIDTH + x, QUASI_WIDTH);
    }
    drawQuasi();
    gfx_SwapDraw();
    fallBuffer ^= 1;
}

static void drawAttractBells(void)
{
    // The original teaser hangs bells over the otherwise empty rope pit.
    gfx_SetClipRegion(0, CASTLE_Y, 320, CASTLE_BOTTOM);
    gfx_SetColor(SCENE_WHITE);
    for (unsigned i = 0; i < 5; ++i) {
        int x = 432 + (int)i * 24 - cameraX;
        gfx_VertLine(x + 8, 48, 40);
        gfx_TransparentSprite(game_bell, x, 32);
    }
}

static void drawAttractParade(uint8_t credits)
{
    gfx_SetClipRegion(0, 0, 320, 240);
    if (paradeRefresh) {
        gfx_FillScreen(SCENE_BLACK);
        drawHud(credits);
        drawCastle();
        gfx_SetClipRegion(0, 0, 320, 240);
        drawMinimap();
        for (unsigned i = 1; i < lives && i < 4; ++i)
            gfx_Sprite(game_lificon, 128 + i * 8, 224);
        --paradeRefresh;
    } else {
        // Only restore the seven tile rows behind the little army.
        castle.draw_height = 7;
        drawCastle();
        castle.draw_height = 20;
    }
    gfx_SetClipRegion(0, 0, 320, 192);
    bool returning = paradeTick >= 608;
    unsigned tick = returning ? paradeTick - 608 : paradeTick;
    unsigned revealed = returning ? tick / 40 : 0;
    if (revealed > 9)
        revealed = 9;
    unsigned frame = returning ? 4 + (tick % 49) * 3 / 49 : (tick / 8) & 3;
    gfx_sprite_t *actor = quasiSprites[frame];
    if (returning) {
        gfx_FlipSpriteY(actor, (gfx_sprite_t *)flippedQuasi);
        actor = (gfx_sprite_t *)flippedQuasi;
    }
    if (returning) {
        gfx_SetTextFGColor(SCENE_WHITE);
        gfx_PrintStringXY("FEATURING", 124, 56);
        if (revealed) {
            // THE BELLS, straight from the arcade tiles. 255 is the space.
            static const uint8_t letters[] = {0, 1, 2, 255, 3, 2, 4, 4, 5};
            for (unsigned i = 9 - revealed; i < 9; ++i)
                if (letters[i] != 255)
                    gfx_Sprite((gfx_sprite_t *)theBellsCache[letters[i]],
                               88 + i * 16, 72);
        }
        // Shared blue ink: the wall AND the HUD shadows join the light show.
        if (tick >= 360)
            gfx_palette[SCENE_BASE + 2] = ((tick - 360) / 4) & 1
                ? gfx_RGBTo1555(255, 0, 0) : gfx_RGBTo1555(0, 255, 0);
    }
    // Letters don't get to sacrifice a Quasi. Everyone hops off-screen.
    // Draw the boys last so the letters' black backgrounds cannot eat them.
    for (unsigned i = 0; i < 11; ++i) {
        int x = returning ? 288 - (int)i * 28 - (int)tick / 2
                          : -16 + (int)tick / 2 - (int)i * 28;
        if (x > -16 && x < 320)
            gfx_TransparentSprite(actor, x, WALL_TOP - QUASI_HEIGHT
                                  - (returning ? jumpHeight[tick % 49] : 0));
    }
    gfx_SetClipRegion(0, 0, 320, 240);
    drawCredits(credits);
    gfx_SwapDraw();
}

void openingDraw(uint8_t credits)
{
    gfx_Wait();
    if (phase == RESCUE_CELEBRATE) {
        rescueCelebration(transitionTick);
        quasiX8 = 168 * 8; quasiFeet = 142; quasiFrame = 0; facingLeft = true;
        drawQuasi();
        drawDeveloperLabel();
        gfx_SwapDraw(); return;
    }
    if (phase == FALLING && fallCached) {
        gfx_SetClipRegion(0, 0, GFX_LCD_WIDTH, GFX_LCD_HEIGHT);
        drawFallPatch();
        return;
    }
    if (phase != FALLING) fallCached = false;
    if (attractMode && phase == LEVEL_READY) {
        drawAttractParade(credits);
        return;
    }
    gfx_SetClipRegion(0, 0, GFX_LCD_WIDTH, GFX_LCD_HEIGHT);
    if (phase == RESTART_CARD || phase == GAME_OVER) {
        gfx_FillScreen(SCENE_BLACK);
        gfx_SetTextFGColor(SCENE_BASE + 11);
        char card[] = "PLAYER 1 GAME OVER";
        card[7] += playerIndex;
        if (phase != GAME_OVER) card[8] = 0;
        gfx_PrintStringXY(card,
                          phase == GAME_OVER ? 88 : 128, 116);
        drawCredits(credits);
        drawDeveloperLabel();
        gfx_SwapDraw();
        return;
    }
    if (phase == FALLING || phase == FRIED || phase == DEATH_WIPE
        || phase == RESTART_SLIDE || hudRefresh) {
        gfx_FillScreen(SCENE_BLACK);
        drawHud(credits);
        if (hudRefresh)
            --hudRefresh;
    }
    if (rescueStage() && phase != LEVEL_SCROLL && phase != RESTART_SLIDE) {
        gfx_SetClipRegion(0, 0, 320, CASTLE_BOTTOM);
        rescueDraw(0, true);
        gfx_Sprite(game_hud_super_bonus, 160, 0);
        gfx_SetClipRegion(0, 0, 320, 240);
        // Arrows emerge from behind the border, not through the brick wallpaper.
        gfx_SetClipRegion(32, 0, 280, CASTLE_BOTTOM);
        for (unsigned i = 0; i < 2; ++i) {
            const Projectile *p = &projectiles[i];
            if (!p->flags) continue;
            gfx_sprite_t *arrow = (p->flags & 1) ? (gfx_sprite_t *)leftArrow : game_arrow;
            int x = p->x8 / 8, y = projectileY(p);
            gfx_TransparentSprite(arrow, x, y);
            if (x < 24 && y < 88) {
                gfx_SetClipRegion(0, 0, 24, 88);
                gfx_TransparentSprite(arrow, x, y);
                gfx_SetClipRegion(32, 0, 280, CASTLE_BOTTOM);
            }
        }
        gfx_SetClipRegion(0, 0, 320, 240);
        if (phase == LEVEL_READY || phase == RESCUE_PAYOUT) drawQuasi();
        goto hudFooter;
    }
    // Only this narrow strip lies above the tilemap. Clear it in both buffers
// on each draw, so the rope leaves no trails or stale pixels after a scroll.
    gfx_SetColor(SCENE_BLACK);
    gfx_FillRectangle(112, 0, 88, CASTLE_Y);
    drawCastle();

    if (attractMode)
        drawAttractBells();

    // Fire is a gameplay hazard; the opening keeps its original empty pit.
    if (phase >= LEVEL_READY && phase != LEVEL_SCROLL && phase != RESTART_SLIDE && fireStage()) {
        for (unsigned i = 0; i < 15; ++i)
            gfx_Sprite(fireSprites[flameFrame[i]], 96 + i * 8, 168);
    }

    if ((fireballStage() || arrowStage()) && phase >= LEVEL_READY
        && phase != LEVEL_SCROLL && phase != RESTART_SLIDE) {
        for (unsigned i = 0; i < 2; ++i) {
            const Projectile *p = &projectiles[i];
            if (!p->flags) continue;
            int y = projectileY(p);
            if (fireballStage())
                gfx_TransparentSprite(fireballSprites[fireballAnimation / 5], p->x8 / 8, y);
            else
                gfx_TransparentSprite((p->flags & 1) ? (gfx_sprite_t *)leftArrow
                    : game_arrow, p->x8 / 8, y);
        }
    }
    if (phase == LEVEL_SCROLL) {
        int offset = cameraX - level * 320;
        if (stageHasBells(level)) drawBellLadder(-offset);
        if (stageHasBells(level + 1)) drawBellLadder(320 - offset);
        // Decorations travel with the bricks instead of popping in at the end.
        for (unsigned side = 0; side < 2; ++side) {
            unsigned stage = level + side;
            int origin = (int)side * 320 - offset;
            if (stageHasFire(stage)) {
                for (unsigned i = 0; i < 15; ++i)
                    gfx_Sprite(fireSprites[flameFrame[i]], origin + 96 + i * 8, 168);
            }
            if (stageHasRope(stage)) {
                gfx_SetColor(SCENE_WHITE);
                gfx_Line(origin + ROPE_PIVOT_X, 0,
                         origin + ropeEndX8() / 8, ropeEndY8() / 8);
            }
            if (stageHasGuards(stage)) {
                if (!side) guardsDrawOffset(origin);
                else for (unsigned i = 0; i < 3; ++i)
                    gfx_TransparentSprite((gfx_sprite_t *)pitguardCache[0], origin + 96 + i * 56, 72);
            }
        }
    } else if (bellStage() && phase >= LEVEL_READY) {
        drawBellLadder(phase == RESTART_SLIDE ? 320 - transitionTick * 8 : 0);
    }

    if (ropeStage() && (phase == LEVEL_READY || phase == FALLING
        || phase == FRIED || phase == LEVEL_CLEAR)) {
        gfx_SetClipRegion(0, 0, GFX_LCD_WIDTH, CASTLE_BOTTOM);
        gfx_SetColor(SCENE_WHITE);
        gfx_Line(ROPE_PIVOT_X, 0, ropeEndX8() / 8, ropeEndY8() / 8);
        gfx_SetClipRegion(0, CASTLE_Y, GFX_LCD_WIDTH, CASTLE_BOTTOM);
    }

    if (phase == LEVEL_READY || phase == FALLING || phase == FRIED
        || phase == SUPER_BONUS || phase == LEVEL_CLEAR || phase == CASTLE_CLEAR) {
        if (guardStage())
            guardsDraw();
        knightDraw(level);
    }

    if (phase == HOP_IN || phase == SCROLL_TO_START || phase == HOP_TO_WALL
        || phase == JUMP_ONTO_WALL || phase == LEVEL_READY || phase == SUPER_BONUS || phase == LEVEL_CLEAR
        || phase == LEVEL_SCROLL || phase == CASTLE_CLEAR)
        drawQuasi();

    if (towerStage() && phase != LEVEL_SCROLL && phase != RESTART_SLIDE) {
        gfx_SetClipRegion(0, 0, 320, CASTLE_BOTTOM);
        if (phase == LEVEL_CLEAR || phase == SUPER_BONUS || (hudTick & 16))
            rescueDrawHeart(232, 36, false);
        else
            rescueDrawHelp(232, 32);
    }
hudFooter:
    gfx_SetClipRegion(0, 0, GFX_LCD_WIDTH, GFX_LCD_HEIGHT);
    drawMinimap();
    // Only the number disappears. PLAYER isn't going anywhere.
    gfx_SetColor(SCENE_BLACK);
    gfx_FillRectangle(104, 224, 8, 8);
    if (!attractMode && (phase < LEVEL_READY || !(hudTick & 16))) {
        gfx_SetTextFGColor(SCENE_WHITE);
        gfx_PrintStringXY(playerIndex ? "2" : "1", 104, 224);
    }
    drawCredits(credits);
    if (scoreDirty) {
        drawScore();
        --scoreDirty;
    }
    if (phase == LEVEL_CLEAR || phase == CASTLE_CLEAR
        || (phase == LEVEL_SCROLL && cameraX - level * 320 < 56)) {
        int boxX = phase == LEVEL_SCROLL ? level * 320 - cameraX : 0;
        gfx_SetColor(SCENE_BLACK);
        gfx_FillRectangle(boxX, 88, 56, 32);
        gfx_SetColor(SCENE_YELLOW);
        gfx_Rectangle(boxX, 88, 56, 32);
        gfx_SetColor(SCENE_RED);
        gfx_Rectangle(boxX + 2, 90, 52, 28);
        gfx_SetTextFGColor(SCENE_WHITE);
        gfx_SetTextConfig(gfx_text_clip);
        gfx_PrintStringXY("SCORE", boxX + 8, 93);
        drawAward(boxX);
        gfx_SetTextConfig(gfx_text_noclip);
    }
    gfx_SetColor(SCENE_BLACK);
    gfx_FillRectangle(136, 224, 16, 8);
    for (unsigned i = 1; i < lives && i < 4; ++i)
        gfx_Sprite(game_lificon, 128 + i * 8, 224);
    if (phase == FALLING) {
        drawDeveloperLabel();
        // Show the hit pose before copying the frame. Quasi used to blink out here.
        fallPatchY[0] = fallPatchY[1] = GFX_LCD_HEIGHT;
        fallBuffer = 0;
        fallCached = true;
        drawFallPatch();
        gfx_Wait();
        gfx_BlitScreen();
        fallPatchY[1] = fallPatchY[0];
        memcpy(fallPatch[1], fallPatch[0], sizeof fallPatch[0]);
        return;
    }
    if (phase == DEATH_WIPE) {
        gfx_SetColor(SCENE_BLACK);
        gfx_FillRectangle(320 - transitionTick * 20, 0, transitionTick * 20, 240);
    }
    if (phase == CASTLE_CLEAR) {
        // Sir this is a prototype, as of rn
        gfx_SetTextFGColor(SCENE_WHITE);
        gfx_PrintStringXY(phase == GAME_OVER ? "GAME OVER" : "STAGE 14 CLEAR",
                          phase == GAME_OVER ? 124 : 112, 48);
        gfx_PrintStringXY("PRESS 1 TO START", 96, 64);
    }
    if (developerMenu) drawDeveloperMenu();
    drawDeveloperLabel();
        gfx_SwapDraw();
}
