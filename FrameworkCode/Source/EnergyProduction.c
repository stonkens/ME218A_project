/***************************************************************************
 Module
   EnergyProduction.c

 Revision
   1.0.1

 Description
   This module reads pulses from IR morse led, and characterizes morse string
   posted

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 10/23/18 11:11 ston    First pass
 
****************************************************************************/
//----------------------------- Include Files -----------------------------*/
// the common headers for C99 types 
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
/* include header files for this service
*/
#include "EnergyProduction.h"

/* include header files for hardware access
*/
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"

/* include header files for the framework
*/
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_DeferRecall.h"
#include "ES_ShortTimer.h"

/* include header files for the other modules that are referenced
*/
#include "ShiftRegisterWrite.h"
#include "SunMovement.h"

#define ONE_SEC 1000
#define FIVE_SEC (ONE_SEC*5)
#define V_INCREMENT_FIVE_SEC 342 //To be changed


// Private functions
static uint8_t ReadSolarPanelPosition(void);
static bool ReadSmokeTowerIR(void)

// module level defines
static uint8_t MyPriority;
static EnergyGameState CurrentEnergyState;
static uint16_t LastSolarPanelVoltage;
static bool LastSmokeTowerState;
static uint8_t V_sun;


/****************************************************************************
 Function
     InitEnergyProduction
 Parameters
     uint8_t Priority, priority of SM

 Returns
     bool true in case of no errors

 Description
     Takes service's priority number, initializes TIVA Ports. Posts ES_INIT
     to own service
 Notes

 Author
     Sander Tonkens, 11/1/18, 10:20
****************************************************************************/

bool InitEnergyProduction(uint8_t Priority)
{
  ES_Event_t ThisEvent;
  
  MyPriority = Priority;
  //Define solar panel position TIVA input as an analog input 
  //Define SmokeTowerIR TIVA input as a digital input

  //Sample port line and use it to initialize the LastSolarPanelVoltage variable
  LastSolarPanelVoltage = ReadSolarPanelPosition();
  LastSmokeTowerState = ReadSmokeTowerIR();
  V_sun = 4096; //pick value appropriately based on what the initial A/D value is

  CurrentEnergyState = InitEnergyGame;
  
  //Post Event ES_Init to EnergyProduction queue (this service)
  ThisEvent.EventType = ES_INIT;
  PostEnergyProduction(ThisEvent);
  return true;

}

/****************************************************************************
 Function
     PostEnergyProduction
 Parameters
     ES_Event ThisEvent ,the event to post to the queue

 Returns
     bool false if the Enqueue operation failed, true otherwise

 Description
     Posts an event to this state machine's queue
 Notes

 Author
     Sander Tonkens, 11/1/18, 10:20
****************************************************************************/
bool PostEnergyProduction(ES_Event_t ThisEvent)
{
  return ES_PostToService( MyPriority, ThisEvent);
}


/****************************************************************************
 Function
     RunEnergyProductionSM
 
Parameters
   ES_Event_t : the event to process

 Returns
   ES_Event_t, ES_NO_EVENT if no error ES_ERROR otherwise

 Description
     implements the state machine for Morse Elements
 Notes

 Author
     Sander Tonkens, 11/1/18, 10:20
****************************************************************************/

ES_Event_t RunEnergyProductionSM(ES_Event_t ThisEvent)
{
  ES_Event_t ReturnEvent;

  //Default return event
  ReturnEvent.EventType = ES_NO_EVENT;
  ES_Event_t MoveSunEvent;
  //Corresponds to number of LEDs to be on
  uint8_t energy_level;

  //Based on the state of the CurrentEnergyState variable choose one of the 
  //following blocks of code:
  switch(CurrentEnergyState)
    {
    case InitEnergyGame:
    {
      
      if(ThisEvent.EventType == ES_INIT)
      {
		CurrentEnergyState = EnergyStandBy;
      }
    break;
    }
    
    case EnergyStandBy:
    {
      if(ThisEvent.EventType == ES_ENERGY_GAME_START)
      {
        //post event to game service to 
        //1. Play coalplant audio
        //2. turn on pollution leds
        //3. turn on energy leds with parameter all
        //Initiate 5 s timer
        CurrentEnergyState = CoalPowered;

      }
    break;
    }
 
    case CoalPowered:
    {
      if((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == SUN_POSITION_TIMER))
      {
        //Move sun to new position by calling that service
        MoveSunEvent.EventType = ES_MOVE_SUN;
        MoveSunEvent.EventParam = 0;
        PostSunMovement(MoveSunEvent);
        //Change V_sun by one iteration
        ChangeSunVoltage();
        //Initiate 5 s timer
        ES_Timer_InitTimer(SUN_POSITION_TIMER, FIVE_SEC);
      }
      else if(ThisEvent.EventType == ES_TOWER_PLUGGED)
      {
        //post event to do the following:
        //1. Stop coalplant audio
        //2. Turn "low" polution leds
        SR_WritePollution(2*1);
        //3. Turn "low" energy leds
        SR_WriteEnergy(2*1);
        //Call EvaluateAlignment to check alignment solar panel
        energy_level = EvaluateSolarAlignment();
        SR_WriteEnergy(2*energy_level);
        CurrentEnergyState = SolarPowered;
      }
      else if((ThisEvent.EventType == ES_AUDIO_END) && (ThisEvent.EventParam == COAL_AUDIO))
      {
        //1. Play coalplant audio
      }
      else if((ThisEvent.EventType == ES_RESET_ALL_GAMES))
      {
      	//1. Stop playing coalplant audio directly
      	//2. Function to reset sun to original position
      	MoveSunEvent.EventType = ES_MOVE_SUN;
      	MoveSunEvent.EventParam = 1;
      	PostSunMovement(MoveSunEvent);
      	CurrentEnergyState = EnergyStandBy;
      	//CurrentEnergyState = InitEnergyGame
      }
    break;
    }

    case SolarPowered:
    {
      if(ThisEvent.EventType == ES_TOWER_UNPLUGGED)
	  {
        //1. Play coalplant audio
        //2. Turn on polution leds
        //3. Turn on energy leds with parameter all 
        CurrentEnergyState = CoalPowered;
	  }
      else if((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == SUN_POSITION_TIMER))
      {
        //Initiate 5 s timer
        //Move sun to new position by calling that service
        //Change V_sun by one iteration

        //Call EvaluateAlignment to check alignment solar panel
        energy_level = EvaluateSolarAlignment();
        //Write new value of EvaluateSolarAlignment to LEDs
        //6 LEDs to represent 3 different levels, so *2
        SR_WriteEnergy(2*energy_level);
        
      }
      else if((ThisEvent.EventType == ES_SOLARPOS_CHANGE))
      {
        //Call EvaluateAlignment to check alignment solar panel
        energy_level = EvaluateSolarAlignment();
        SR_WriteEnergy(2*energy_level);
      }
      else if((ThisEvent.EventType == ES_RESET_ALL_GAMES))
      {
        //Move sun to initial position by calling that service
        CurrentEnergyState = EnergyStandBy;
        //EnergyStandBy or InitEnergyGame

      }
    break;
    }	
  }
  return ReturnEvent;
}
/****************************************************************************
 Function
     CheckSolarPanelPosition
 Parameters
     None

 Returns
     bool, true if an event is posted

 Description
     Checks if pin state has changed and posts event if change detected
 Notes

 Author
     Sander Tonkens, 11/1/18, 10:20
****************************************************************************/

bool CheckSolarPanelPosition(void)
{
  ES_Event_t ThisEvent;
  ES_Event_t AnyEvent;
  bool ReturnVal = false;
  static uint16_t CurrentSolarPanelVoltage;

  //Get CurrentSolarPanelVoltage from input line
  CurrentSolarPanelVoltage = ReadSolarPanelPosition();
  
  //Analog signal, to be changed accordingly
  //If the state of the Morse input line has changed
  if(abs(CurrentSolarPanelVoltage-ReferenceLastSolarPanelVoltage) >= V_threshold)
  {
  	ThisEvent.EventType = ES_SOLARPOS_CHANGE;
  	PostEnergyProduction(ThisEvent);
  	AnyEvent.EventType = ES_USERMVT_DETECTED;
  	//Post ES_USERMVT_DETECTED to game manager
    ReturnVal = true;
  }
  LastSolarPanelVoltage = CurrentSolarPanelVoltage;
  return ReturnVal;
}

/****************************************************************************
 Function
     CheckSmokeTowerEvents
 Parameters
     None

 Returns
     bool, true if an event is posted

 Description
     Checks if pin state has changed and posts event if change detected
 Notes
   Returns specific event that has occured (pressing or releasing)
 Author
     Sander Tonkens, 11/1/18, 10:20
****************************************************************************/
bool CheckSmokeTowerEvents(void)
{
  ES_Event_t ThisEvent;
  ES_Event_t AnyEvent;
  bool ReturnVal = false;
  bool CurrentSmokeTowerState;

  CurrentSmokeTowerState = ReadSmokeTowerIR();
  
  //If the CurrentSmokeTowerState is different from the LastSmokeTowerState
  if(CurrentSmokeTowerState != LastSmokeTowerState)
  {
    ReturnVal = true;
    //If the CurrentSmokeTowerState is down   
    if (CurrentSmokeTowerState == 0)
    {
      ThisEvent.EventType = ES_TOWER_PLUGGED;
      PostEnergyProduction(ThisEvent);
      AnyEvent.EventType = ES_USERMVT_DETECTED;
      //Post ES_USERMVT_DETECTED to game manager
    }
    else
    {   
      ThisEvent.EventType = ES_TOWER_UNPLUGGED;
      PostEnergyProduction(ThisEvent);
      AnyEvent.EventType = ES_USERMVT_DETECTED;
      //Post ES_USERMVT_DETECTED to game manager
    }
  } 
  LastSmokeTowerState = CurrentSmokeTowerState;
  return ReturnVal;
}

//***************************************************************************

//********************************
// These functions are private to the module
//********************************

/****************************************************************************
 Function
     ReadSolarPanelPosition

 Parameters
    Nothing

 Returns
    uint16_t returns state of solar panel

 Description
    Returns analog input pin of TIVA
 Notes
      
 Author
    Sander Tonkens, 11/1/18, 10:32
****************************************************************************/
static uint8_t ReadSolarPanelPosition(void)
{
  uint8_t SolarPanelPosition=0;
  //Read analog input pin
  //SolarPanelPosition = HWREG(GPIO_PORTB_BASE + (GPIO_O_DATA + ALL_BITS)) & BIT7HI;
  return SolarPanelPosition;
}

/****************************************************************************
 Function
     V_sun

 Parameters
    Nothing

 Returns
    Nothing

 Description
    Function is called every time the sun is moved position
 Notes
      
 Author
    Sander Tonkens, 11/1/18, 10:32
****************************************************************************/
static void ChangeSunVoltage(void)
{
	V_sun = V_sun + V_INCREMENT_FIVE_SEC;
	return;
}

/****************************************************************************
 Function
     EvaluateSolarAlignment

 Parameters
    Nothing

 Returns
    uint8_t Alignment_param, corresponds to number of energy leds to be 
    turned on

 Description
    Function is called every time the sun is moved position
 Notes
      
 Author
    Sander Tonkens, 11/1/18, 10:32
****************************************************************************/
static uint8_t EvaluateSolarAlignment(void)
{
	uint8_t Alignment_param
	uint8_t V_solar = ReadSolarPanelPosition();
	if(abs(V_sun - V_solar)<V_wellaligned)
	{
		Alignment_param = 3;
	}
	else if(abs(V_sun - V_solar)<V_mediumaligned)
	{
		Alignment_param = 2;
	}
	else
	{
		Alignment_param = 1;
	}
	return Alignment_param;
}

/****************************************************************************
 Function
     ReadSmokeTowerIR

 Parameters
    Nothing

 Returns
    bool returns state of SmokeTowerIR

 Description
    Returns bit 3 of the TIVA Port B, which reads state of morse coder
 Notes
      
 Author
    Sander Tonkens, 11/1/18, 10:32
****************************************************************************/
static bool ReadSmokeTowerIR(void)
{
  bool SmokeTowerIRState=0;
  //Read digital input pin (as in Morselements)
  //SmokeTowerIRState = HWREG(GPIO_PORTB_BASE + (GPIO_O_DATA + ALL_BITS)) & BIT3HI;
  return SmokeTowerIRState;
}







