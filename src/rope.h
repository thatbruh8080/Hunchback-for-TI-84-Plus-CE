#ifndef ROPE_H
#define ROPE_H

#include <stdint.h>

// All positions use eighths of a pixel. One tick is 1/50 second.
enum { ROPE_PIVOT_X = 156, ROPE_PERIOD = 184 };
void ropeReset(void);
void ropeUpdate(void);
int ropeEndX8(void);
int ropeEndY8(void);
int ropeSpeedX8(void);

#endif
