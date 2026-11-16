#ifndef OPENING_H
#define OPENING_H

#include <stdbool.h>
#include <stdint.h>

// The main loop calls openingUpdate() at 50 ticks per second.
void openingBegin(uint8_t credits);
void openingInput(bool left, bool right, bool jumpPressed);
bool openingGameOver(void);
bool openingReturnToTitle(void);
bool openingUpdate(void);
void openingDraw(uint8_t credits);

// Developer sessions never qualify for score persistence.
bool openingScoreEligible(void);
uint32_t openingScoreHundreds(void);
bool openingDeveloperActive(void);
uint8_t openingDeveloperStage(void);
void openingDeveloperBegin(uint8_t stage, uint8_t lives, uint8_t credits);
void openingDeveloperPreview(uint8_t stage, uint8_t lives, uint8_t row, uint8_t credits);

// The attract parade shares the opening, but never runs gameplay or scoring.
void openingAttractBegin(uint8_t credits, bool redCastle);
bool openingAttractUpdate(void);

#endif
