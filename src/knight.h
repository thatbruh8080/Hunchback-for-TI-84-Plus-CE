#ifndef KNIGHT_H
#define KNIGHT_H

#include <stdbool.h>
#include <stdint.h>

void knightReset(void);
void knightSetHard(bool hard);
void knightUpdate(void);
void knightDraw(uint8_t level);
bool knightPassed(unsigned x);
bool knightCaught(int playerX8);

#endif
