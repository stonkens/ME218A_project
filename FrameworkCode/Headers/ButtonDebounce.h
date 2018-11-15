/****************************************************************************************
*   ButtonDebounce.h
*   
* 
*
****************************************************************************************/

#ifndef BUTTON_DEBOUNCE_H
#define BUTTON_DEBOUNCE_H

bool InitButtonDebounce(uint8_t Priority);
bool PostButtonDebounce(ES_Event_t ThisEvent);
ES_Event_t RunButtonDebounce(ES_Event_t ThisEvent);

#endif