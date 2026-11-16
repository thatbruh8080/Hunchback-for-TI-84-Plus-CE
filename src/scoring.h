#ifndef SCORING_H
#define SCORING_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    // One unit is 100 points, just like the arcade's score accumulator.
    unsigned hundreds;
    uint8_t frames, periods, bells;
    bool extraLifeAwarded, extraLifePending;
} Scoring;

void scoringNewGame(Scoring *s);
void scoringStartStage(Scoring *s);
void scoringTick(Scoring *s);
void scoringLoseLife(Scoring *s);
// Completed stages 1-15, then 17-31 on subsequent circuits.
// Returns the ordinary stage award in hundreds, excluding the super bonus.
unsigned scoringClearStage(Scoring *s, uint8_t stage);
// At five bells, pay each bell during the animation, then the stage award.
void scoringPayBell(Scoring *s);
void scoringPayStage(Scoring *s, unsigned award);
// For the eventual stage-16 rescue: pass the arcade knight's progress, 0-159.
// Returns the rescue award in hundreds. This does not add an ordinary bonus.
unsigned scoringRescue(Scoring *s, uint8_t knightProgress);

#endif
