#include "game_gfx.h"
#include "rescue.h"
#include "gfx/gfx.h"

static const uint8_t castletilesColors[] = { 32, 33, 34, 35, 36, 37, 38, 39 };
uint8_t castletilesCache[46][66];
gfx_sprite_t *castletilesSprites[46];
static const uint8_t quasiColors[] = { 32, 35, 40, 41, 42 };
uint8_t quasiCache[8][386];
gfx_sprite_t *quasiSprites[8];
static const uint8_t scoredigitsColors[] = { 32, 39 };
uint8_t scoredigitsCache[10][258];
gfx_sprite_t *scoredigitsSprites[10];
static const uint8_t hud_super_bonusColors[] = { 32, 34, 38 };
uint8_t hud_super_bonusCache[1][2562];
static const uint8_t hud_scoreColors[] = { 32, 34, 38 };
uint8_t hud_scoreCache[1][1282];
static const uint8_t bellColors[] = { 32, 36, 37, 38, 39 };
uint8_t bellCache[1][258];
static const uint8_t bellropeColors[] = { 36, 38 };
uint8_t bellropeCache[1][242];
static const uint8_t lificonColors[] = { 32, 34, 35, 36, 37, 38 };
uint8_t lificonCache[1][66];
static const uint8_t minicastleColors[] = { 32, 37 };
uint8_t minicastleCache[1][1026];
static const uint8_t fireColors[] = { 32, 33, 35, 38, 39 };
uint8_t fireCache[6][66];
gfx_sprite_t *fireSprites[6];
static const uint8_t fireballColors[] = { 32, 35, 42, 44, 45, 46 };
uint8_t fireballCache[5][98];
gfx_sprite_t *fireballSprites[5];

uint8_t knightClimbCache[5][386];
uint8_t knightChaseCache[2][386];
uint8_t knightBridgeCache[66];
uint8_t pitguardCache[3][642];
uint8_t arrowCache[38];
static const uint8_t arrowColors[] = {32, 35, 42, 44};
uint8_t theBellsCache[6][258];
static const uint8_t theBellsColors[] = {32, 34, 38};

// Read packed source pixels high bits first and map them to the scene palette.
static void unpack(const uint8_t *source, unsigned stride, unsigned x, unsigned y,
                   unsigned width, unsigned height, unsigned bpp,
                   const uint8_t *colors, uint8_t *sprite)
{
    *sprite++ = width;
    *sprite++ = height;
    unsigned perByte = 8 / bpp;
    unsigned mask = (1u << bpp) - 1;
    for (unsigned row = 0; row < height; ++row) {
        const uint8_t *line = source + (y + row) * (stride / perByte);
        for (unsigned col = 0; col < width; ++col) {
            unsigned pixel = x + col;
            unsigned shift = 8 - bpp - (pixel % perByte) * bpp;
            *sprite++ = colors[(line[pixel / perByte] >> shift) & mask];
        }
    }
}

void gameGfxPrepare(void)
{
    rescueArtPrepare();

    // castletiles: 4 bpp in storage, 8 bpp in the rendering cache.
    for (unsigned i = 0; i < 46; ++i) {
        unpack(castletiles_data, 368, (i % 46) * 8, (i / 46) * 8,
               8, 8, 4, castletilesColors, castletilesCache[i]);
        castletilesSprites[i] = (gfx_sprite_t *)castletilesCache[i];
    }
    // quasi: 4 bpp in storage, 8 bpp in the rendering cache.
    for (unsigned i = 0; i < 8; ++i) {
        unpack(quasi_data, 64, (i % 4) * 16, (i / 4) * 24,
               16, 24, 4, quasiColors, quasiCache[i]);
        quasiSprites[i] = (gfx_sprite_t *)quasiCache[i];
    }
    // scoredigits: 1 bpp in storage, 8 bpp in the rendering cache.
    for (unsigned i = 0; i < 10; ++i) {
        unpack(scoredigits_data, 160, (i % 10) * 16, (i / 10) * 16,
               16, 16, 1, scoredigitsColors, scoredigitsCache[i]);
        scoredigitsSprites[i] = (gfx_sprite_t *)scoredigitsCache[i];
    }
    // hud_super_bonus: 2 bpp in storage, 8 bpp in the rendering cache.
    for (unsigned i = 0; i < 1; ++i) {
        unpack(hud_super_bonus_data, 80, (i % 1) * 80, (i / 1) * 32,
               80, 32, 2, hud_super_bonusColors, hud_super_bonusCache[i]);
    }
    // hud_score: 2 bpp in storage, 8 bpp in the rendering cache.
    for (unsigned i = 0; i < 1; ++i) {
        unpack(hud_score_data, 80, (i % 1) * 80, (i / 1) * 16,
               80, 16, 2, hud_scoreColors, hud_scoreCache[i]);
    }
    // The bare rope is 1 bpp; earned bells are separate 4-bpp sprites.
    unpack(bellrope_data, 80, 0, 0, 80, 3, 1, bellropeColors, bellropeCache[0]);
    unpack(bell_data, 16, 0, 0, 16, 16, 4, bellColors, bellCache[0]);
    // lificon: 4 bpp in storage, 8 bpp in the rendering cache.
    for (unsigned i = 0; i < 1; ++i) {
        unpack(lificon_data, 8, (i % 1) * 8, (i / 1) * 8,
               8, 8, 4, lificonColors, lificonCache[i]);
    }
    // minicastle: 1 bpp in storage, 8 bpp in the rendering cache.
    for (unsigned i = 0; i < 1; ++i) {
        unpack(minicastle_data, 64, (i % 1) * 64, (i / 1) * 16,
               64, 16, 1, minicastleColors, minicastleCache[i]);
    }
    // Arcade flame tiles F0-F5, stored as a 4-bpp horizontal strip.
    for (unsigned i = 0; i < 6; ++i) {
        unpack(fire_data, 48, i * 8, 0, 8, 8, 4, fireColors, fireCache[i]);
        fireSprites[i] = (gfx_sprite_t *)fireCache[i];
    }
    // Five video-derived tumble frames, packed at 4 bpp.
    for (unsigned i = 0; i < 5; ++i) {
        unpack(fireball_data, 60, i * 12, 0, 12, 8, 4,
               fireballColors, fireballCache[i]);
        fireballSprites[i] = (gfx_sprite_t *)fireballCache[i];
    }
    // The knight shares the castle's eight colours and 4-bpp palette mapping.
    for (unsigned i = 0; i < 5; ++i)
        unpack(knight_climb_data, 80, i * 16, 0, 16, 24, 4,
               castletilesColors, knightClimbCache[i]);
    for (unsigned i = 0; i < 2; ++i)
        unpack(knight_chase_data, 48, i * 24, 0, 24, 16, 4,
               castletilesColors, knightChaseCache[i]);
    unpack(knight_bridge_data, 8, 0, 0, 8, 8, 4,
           castletilesColors, knightBridgeCache);

    // Full guards this time. The ROM only animates their left halves.
    // Keep the static right halves too. The boys deserve both legs.
    for (unsigned i = 0; i < 3; ++i)
        unpack(pitguard_data, 48, i * 16, 0, 16, 40, 4,
               castletilesColors, pitguardCache[i]);

    // Original ROM letters. E and L don't need stunt doubles in RAM.
    static const uint8_t letterColumn[] = {0, 1, 2, 4, 6, 8};
    for (unsigned i = 0; i < 6; ++i)
        unpack(the_bells_data, 144, letterColumn[i] * 16, 0, 16, 16, 2,
               theBellsColors, theBellsCache[i]);

    unpack(arrow_data, 12, 0, 0, 12, 3, 2, arrowColors, arrowCache);

}
