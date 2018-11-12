/****************************************************************************

Header file for Meat Switch Debounce 
based on the Gen 2 Events and Service Framework
****************************************************************************/

#ifndef MeatSwitchDebounce_H
#define MeatSwitchDebounce_H

//Event definitions
#include "ES_Configure.h"
#include "ES_Types.h"
#include "ES_Events.h"

typedef enum {Debouncing, Ready2Sample} MeatSwitchDebounceState;

//Public Function Prototypes
bool InitMeatSwitchDebounce(uint8_t Priority);
bool PostMeatSwitchDebounce(ES_Event_t ThisEvent);
bool CheckMeatSwitchEvents(void);
ES_Event_t RunMeatSwitchDebounceSM(ES_Event_t ThisEvent);

#endif /* MeatSwitchDebounce_H */
