#include "rope.h"

static uint8_t tick;
static int endX8, endY8, speedX8;

// One quarter of an 84-pixel rope's arc. Mirror it for the other three.
// No square roots every frame. The eZ80 has enough chores already.
static const uint16_t tipY8[47] = {
    672, 672, 672, 672, 671, 671, 670, 669, 669, 668, 667, 666, 664, 663, 662, 660, 659, 657, 655, 653, 651, 649, 646, 644, 641, 639, 636, 633, 630, 627, 624, 620, 617, 613, 609, 605, 601, 597, 592, 588, 583, 578, 573, 568, 563, 557, 551
};

static void position(void)
{
    unsigned quarter = tick / 46;
    unsigned step = tick % 46;
    if (quarter & 1)
        step = 46 - step;
    // Same steady horizontal travel both ways, no easing or endpoint pause.
    int offset8 = step * (48 * 8) / 46;
    endX8 = ROPE_PIVOT_X * 8 + (quarter < 2 ? -offset8 : offset8);
    endY8 = tipY8[step];
}

void ropeReset(void)
{
    tick = 0;
    position();
    speedX8 = 0;
}

void ropeUpdate(void)
{
    int oldX8 = endX8;
    if (++tick == ROPE_PERIOD)
        tick = 0;
    position();
    speedX8 = endX8 - oldX8;
}

int ropeEndX8(void) { return endX8; }
int ropeEndY8(void) { return endY8; }
int ropeSpeedX8(void) { return speedX8; }
