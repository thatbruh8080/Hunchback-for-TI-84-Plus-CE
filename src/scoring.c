#include "scoring.h"

static void addHundreds(Scoring *s, unsigned amount)
{
    // The original accumulator is three bytes. Keep the host tests equivalent.
    s->hundreds = (s->hundreds + amount) & 0xFFFFFFu;

    // Default arcade DIP setting: one extra life at 10,000 points.
// The ROM tests the ten-thousands digit, not the complete score.
    if (!s->extraLifeAwarded && (s->hundreds / 100) % 10 >= 1) {
        s->extraLifeAwarded = true;
        s->extraLifePending = true;
    }
}

void scoringStartStage(Scoring *s)
{
    s->frames = 0;
    s->periods = 0;
}

void scoringNewGame(Scoring *s)
{
    s->hundreds = 0;
    s->bells = 0;
    s->extraLifeAwarded = false;
    s->extraLifePending = false;
    scoringStartStage(s);
}

void scoringTick(Scoring *s)
{
    // ROM $2E5D: 0..56 inclusive. Four periods reduce the bonus by one.
    if (++s->frames == 57) {
        s->frames = 0;
        ++s->periods;
    }
}

void scoringLoseLife(Scoring *s)
{
    // Score and the one-time extra-life flag survive; the bell streak does not.
    s->bells = 0;
}

unsigned scoringClearStage(Scoring *s, uint8_t stage)
{
    unsigned lost = s->periods / 4;
    unsigned award = stage * (lost < 10 ? 10 - lost : 0);

    // The fifth bell waits for the on-screen payout. No instant jackpot now.
    if (++s->bells < 5)
        addHundreds(s, award);
    return award;
}

void scoringPayBell(Scoring *s)
{
    if (s->bells) {
        --s->bells;
        addHundreds(s, 50);
    }
}

void scoringPayStage(Scoring *s, unsigned award)
{
    addHundreds(s, award);
}

unsigned scoringRescue(Scoring *s, uint8_t knightProgress)
{
    // ROM $31AC: round down to a 32-step rung, then pay for remaining rungs.
    unsigned rungs = knightProgress < 160 ? 5 - knightProgress / 32 : 0;
    s->bells = 0;
    for (unsigned i = 0; i < rungs; ++i)
        addHundreds(s, 50);
    return rungs * 50;
}
