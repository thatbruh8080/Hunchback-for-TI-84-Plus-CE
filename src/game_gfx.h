#ifndef GAME_GFX_H
#define GAME_GFX_H

#include <graphx.h>
#include <stdint.h>

// Unpack once while PLAYER 1 is visible, before starting the scene.
void gameGfxPrepare(void);

extern uint8_t castletilesCache[46][66];
extern gfx_sprite_t *castletilesSprites[46];
extern uint8_t quasiCache[8][386];
extern gfx_sprite_t *quasiSprites[8];
extern uint8_t scoredigitsCache[10][258];
extern gfx_sprite_t *scoredigitsSprites[10];
extern uint8_t hud_super_bonusCache[1][2562];
#define game_hud_super_bonus ((gfx_sprite_t *)hud_super_bonusCache[0])
extern uint8_t hud_scoreCache[1][1282];
#define game_hud_score ((gfx_sprite_t *)hud_scoreCache[0])
extern uint8_t bellCache[1][258];
#define game_bell ((gfx_sprite_t *)bellCache[0])
extern uint8_t bellropeCache[1][242];
#define game_bellrope ((gfx_sprite_t *)bellropeCache[0])
extern uint8_t lificonCache[1][66];
#define game_lificon ((gfx_sprite_t *)lificonCache[0])
extern uint8_t minicastleCache[1][1026];
#define game_minicastle ((gfx_sprite_t *)minicastleCache[0])
extern uint8_t fireCache[6][66];
extern gfx_sprite_t *fireSprites[6];
extern uint8_t fireballCache[5][98];
extern gfx_sprite_t *fireballSprites[5];

// Original arcade knight blocks, unpacked once alongside the other graphics.
extern uint8_t knightClimbCache[5][386];
extern uint8_t knightChaseCache[2][386];
extern uint8_t knightBridgeCache[66];

extern uint8_t pitguardCache[3][642];

// Six unique letters; E and L get reused.
extern uint8_t theBellsCache[6][258];

extern uint8_t arrowCache[38];
#define game_arrow ((gfx_sprite_t *)arrowCache)

#endif
