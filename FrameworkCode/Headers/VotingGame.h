/****************************************************************************************
*   VotingGame.h
*   
* 
*
****************************************************************************************/

#include "ES_Events.h"
#include "ES_Types.h"
#include "ES_Configure.h"

typedef enum {
    InitPState,
    Standby,
    ChangingQuestion,
    Waiting4Vote
} VotingGameState;

bool InitVotingGame(uint8_t Priority);
bool PostVotingGame(ES_Event_t ThisEvent);
ES_Event_t RunVotingGame(ES_Event_t ThisEvent);

void CheckButtonInput(void);
