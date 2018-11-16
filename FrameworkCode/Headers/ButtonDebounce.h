/****************************************************************************************
*   ButtonDebounce.h
*   
* 
*
****************************************************************************************/

#ifndef BUTTON_DEBOUNCE_H
#define BUTTON_DEBOUNCE_H

#include "ES_Types.h"
#include "ES_Events.h"
#include "ES_Configure.h"


bool InitButtonDebounce(uint8_t Priority);
bool PostButtonDebounce(ES_Event_t ThisEvent);
ES_Event_t RunButtonDebounce(ES_Event_t ThisEvent);

bool CheckButtonPress(void);

#endif
