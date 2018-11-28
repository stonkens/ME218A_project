/****************************************************************************************
*   VotingGame.h
*   
* 
*
****************************************************************************************/
#ifndef VOTING_GAME_H
#define VOTING_GAME_H

#include "ES_Events.h"
#include "ES_Types.h"
#include "ES_Configure.h"

typedef enum {
    InitVState,
    VStandby,
    ChangingQuestion,
    Waiting4Vote,
    VotingGameOver
} VotingGameState;

bool InitVotingGame(uint8_t Priority);
bool PostVotingGame(ES_Event_t ThisEvent);
ES_Event_t RunVotingGame(ES_Event_t ThisEvent);

void CheckButtonInput(void);
#endif
