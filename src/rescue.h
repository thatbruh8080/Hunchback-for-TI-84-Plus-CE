#ifndef RESCUE_H
#define RESCUE_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int x8, feet8, velocity8, jumpStart8, grip8;
    uint8_t frame, jumpTick, walkTick, floor, cleared[4];
    uint8_t ropeIndex[2], ropeDelay[2], ropeOffset[2];
    uint8_t clock38, progress;
    int8_t held, lastRope, armed;
    bool airborne, facingLeft;
} Rescue;
extern Rescue rescue;
void rescueArtPrepare(void);
void rescueReset(void);
// 0 keeps playing, 1 is a fatal fall/timeout, 2 completes the rescue.
unsigned rescueUpdate(bool left, bool right, bool jump, const uint8_t *arc);
void rescueDraw(int offset, bool occupants);
void rescueDrawHeart(int x, int y, bool broken);
void rescueDrawEsmie(int x, int y);
void rescueDrawHelp(int x, int y);
void rescueCelebration(unsigned tick);
#endif
