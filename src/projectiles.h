#ifndef PROJECTILES_H
#define PROJECTILES_H
#include <stdint.h>

typedef struct {
    int x8;
    // Bit 0: travelling left. Bit 7: low shot. Zero: empty slot.
    uint8_t flags;
    uint8_t floor;
} Projectile;

extern Projectile projectiles[2];
void projectilesSeed(uint32_t seed);
void projectilesReset(unsigned stage);
void projectilesUpdate(void);
void projectilesAim(uint8_t floor, int ropeSide);
#endif
