#ifndef TITLE_PAGES_H
#define TITLE_PAGES_H
#include <stdbool.h>
#include <stdint.h>

void titlePagesInit(void);
void titlePageDraw(bool instructions, uint8_t credits);
void titlePageCredits(uint8_t credits);
void titleHeartsDraw(unsigned first, unsigned end);

#endif
