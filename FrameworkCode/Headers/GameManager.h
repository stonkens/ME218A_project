/******************************************************************************
*   GameManager.h
*   
* 
*
******************************************************************************/
#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H


#include "ES_Configure.h"
#include "ES_Types.h" 

typedef enum {
    InitPState,
    Standby,
    LEAFInserted,
    Startup,
    GameActive
} GameManagerState;

/**************************** Public Functions *******************************/

bool InitGameManager(uint8_t Priority);
bool PostGameManager(ES_Event_t ThisEvent);
ES_Event_t RunGameManager(ES_Event_t ThisEvent);

void CheckLEAFSwitch(void);


#endif
