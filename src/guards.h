#ifndef GUARDS_H
#define GUARDS_H

#include <stdbool.h>
void guardsReset(void);
void guardsUpdate(void);
void guardsDraw(void);
void guardsDrawOffset(int offset);
bool guardsHit(int playerX8, int playerFeet);

#endif
