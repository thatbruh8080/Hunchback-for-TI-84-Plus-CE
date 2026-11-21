#include "projectiles.h"
#include "stages.h"

Projectile projectiles[2];
static uint8_t choices, cooldown;
static uint8_t randomState[4] = {0x53, 0x26, 0x50, 0x81};

static uint8_t launchFloor, stageNumber, launchChoices;

void projectilesAim(uint8_t floor, int ropeSide)
{
    launchFloor = floor;
    launchChoices = choices;
    if ((stageNumber & 15) == 15) {
        // $2D77/$2E06: the first rescue reads slot zero's last launch floor.
        // The second rescue gets all four lanes right from the bottom.
        static const uint8_t firstRescueChoices[4] = {0x98, 0x58, 0x78, 0xF8};
        if (stageNumber == 15) launchChoices = firstRescueChoices[projectiles[0].floor];
        // While hanging at an edge, shoot towards Quasi from the other side.
        if (ropeSide >= 0) launchChoices &= ropeSide ? 0x30 : 0xC0;
    }
}

void projectilesSeed(uint32_t seed)
{
    // Don't let an all-zero random generator take a permanent tea break.
    seed ^= 0x81502653UL;
    if (!seed) seed = 1;
    for (unsigned i = 0; i < 4; ++i) {
        randomState[i] = seed;
        seed >>= 8;
    }
}

// The four-byte generator at ROM $24D4, including its carry feedback.
static uint8_t nextRandom(void)
{
    uint8_t a = randomState[0], b = randomState[1];
    uint8_t c = randomState[2], d = randomState[3];
    randomState[1] = (b >> 1) | (a << 7);
    randomState[2] = (c << 1) | (b & 1);
    randomState[3] = (d << 1) | (c >> 7);
    a += (((a >> 4) ^ (d >> 7)) & 1) << 7;
    randomState[0] = (a >> 1) | (a << 7);
    return randomState[0] ^ randomState[3];
}

void projectilesReset(unsigned stage)
{
    stageNumber = stage & 31;
    choices = stageShots[stageNumber];
    launchChoices = choices;
    launchFloor = 0;
    // ROM $07B5: stage 6 gives you 80 ticks before the first arrow.
    cooldown = stage == 5 ? 80 : 0;
    projectiles[0].flags = projectiles[1].flags = 0;
    projectiles[0].x8 = projectiles[1].x8 = 0;
    projectiles[0].floor = projectiles[1].floor = 0;
}

void projectilesUpdate(void)
{
    // ROM $2D52: shared launch timer, slot 0 gets first dibs.
    if (cooldown) --cooldown;
    for (unsigned i = 0; i < 2; ++i) {
        Projectile *p = &projectiles[i];
        if (!p->flags && (launchChoices & 0xF0) && !cooldown && (!i || (launchChoices & 8))) {
            static const uint8_t flags[4] = {1, 0x81, 2, 0x82};
            uint8_t index, tries = 0;
            do {
                // A CE clock seed can get stuck asking for the same forbidden lane.
                // The default stream visits all four within its first seven rolls.
                if (++tries == 64) projectilesSeed(0);
                index = nextRandom() & 3;
            } while (!(launchChoices & (0x80u >> index)));
            // ROM $2DFD-$2E04, with direction translated to CE screen X.
            p->flags = flags[index];
            p->floor = launchFloor;
            // Adapt the entry edges to the CE's wider playfield.
            p->x8 = (p->flags & 1) ? 320 * 8 : -12 * 8;
            cooldown = 80;
        }
    }
    for (unsigned i = 0; i < 2; ++i) {
        Projectile *p = &projectiles[i];
        if (!p->flags) continue;
        // ROM $2D52/$2C88: 24/16 and 30/16 pixels per tick.
        int speed8 = i ? 15 : 12;
        p->x8 += (p->flags & 1) ? -speed8 : speed8;
        if (((p->flags & 1) && p->x8 <= -12 * 8)
            || (!(p->flags & 1) && p->x8 >= 320 * 8)) p->flags = 0;
    }
}
