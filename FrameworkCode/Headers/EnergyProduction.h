/****************************************************************************

Header file for Morse Elements 
based on the Gen 2 Events and Service Framework
****************************************************************************/

#ifndef EnergyProduction_H
#define EnergyProduction_H

//Event definitions
#include "ES_Configure.h"
#include "ES_Types.h"
#include "ES_Events.h"

//TypeDefs  for the states
typedef enum {InitEnergyGame, EnergyStandBy, CoalPowered, SolarPowered} EnergyGameState;

//Public Function Prototypes
bool InitEnergyProduction(uint8_t Priority);
bool PostEnergyProduction(ES_Event_t ThisEvent);
ES_Event_t RunEnergyProductionSM(ES_Event_t ThisEvent);
bool CheckSolarPanelPosition(void);
bool CheckSmokeTowerEvents(void)

#endif /* EnergyProduction_H */