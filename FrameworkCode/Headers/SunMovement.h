/****************************************************************************

Header file for Sun Movement 
based on the Gen 2 Events and Service Framework
****************************************************************************/

#ifndef SunMovement_H
#define SunMovement_H

//Event definitions
#include "ES_Configure.h"
#include "ES_Types.h"
#include "ES_Events.h"

//Public Function Prototypes
bool InitSunMovement(uint8_t Priority);
bool PostSunMovement(ES_Event_t ThisEvent);
ES_Event_t RunSunMovement(ES_Event_t ThisEvent);

#endif /* SunMovement_H */