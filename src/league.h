#ifndef LEAGUE_H
#define LEAGUE_H
#include <stdbool.h>
#include <stdint.h>

enum { LEAGUE_COUNT = 5, LEAGUE_NAME = 10 };
typedef struct {
    uint32_t hundreds;
    char name[LEAGUE_NAME + 1];
} LeagueEntry;

extern LeagueEntry league[LEAGUE_COUNT];
void leagueLoad(void);
bool leagueSave(void);
unsigned leagueRank(uint32_t hundreds);
void leagueDrawTable(unsigned rank, uint32_t hundreds, const char *name);
void leagueEnter(uint32_t hundreds, uint8_t *credits);
void leagueScoreText(char *out, uint32_t hundreds);
#endif