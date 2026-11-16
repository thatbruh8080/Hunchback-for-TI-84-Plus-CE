#ifndef DEVELOPER_H
#define DEVELOPER_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint8_t stage, lives, row, code; } Developer;
static inline void developerReset(Developer *d)
{
    d->stage = 1; d->lives = 3; d->row = d->code = 0;
}
static inline bool developerLetter(Developer *d, char key)
{
    static const char password[] = "ISUCK";
    if (key == password[d->code]) ++d->code;
    else d->code = 0;
    if (d->code != 5) return false;
    d->code = 0;
    return true;
}
// Return 1 for START, 2 for QUIT. Zero lives means unlimited butt skewering.
static inline unsigned developerControl(Developer *d, char key)
{
    if (key == 'U') d->row = (d->row + 3) & 3;
    if (key == 'D') d->row = (d->row + 1) & 3;
    if (key == 'L' || key == 'R') {
        if (d->row == 0) d->stage = key == 'R' ? d->stage % 32 + 1 : (d->stage + 30) % 32 + 1;
        if (d->row == 1) d->lives = key == 'R' ? (d->lives + 1) % 10 : (d->lives + 9) % 10;
    }
    return key == 'E' && d->row >= 2 ? d->row - 1 : 0;
}
#endif
