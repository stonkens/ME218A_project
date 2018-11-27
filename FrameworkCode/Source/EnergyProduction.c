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
#include "ADMulti.h"
#include "GameManager.h"
#include "AudioService.h"

#define ONE_SEC 1000
#define FIVE_SEC (ONE_SEC*5)
#define TEN_SEC (ONE_SEC*10)
#define V_INCREMENT_FIVE_SEC 0 //To be changed

// flexibility defines
#define GPIO_PORT_EP GPIO_PORTA_BASE //configure A on electrical design

//readability defines
#define TOWER GPIO_PIN_2
#define TOWER_HI BIT2HI
#define TOWER_LO BIT2LO

//constants
#define V_MEDIUMALIGNED 2000
#define V_WELLALIGNED 1000




// Private functions
static uint32_t ReadSolarPanelPosition(void);
static bool ReadSmokeTowerIR(void);
static void ChangeSunVoltage(void);
static uint8_t EvaluateSolarAlignment(void);

// module level defines
static uint8_t MyPriority;
static EnergyGameState CurrentEnergyState;
static uint32_t LastSolarPanelVoltage;
static bool LastSmokeTowerState;
static uint32_t V_sun = 2000;
static uint32_t V_threshold = 600;



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
  HWREG(GPIO_PORT_EP + GPIO_O_DEN) |= (TOWER_HI);
  HWREG(GPIO_PORT_EP + GPIO_O_DIR) &= (TOWER_LO);

  //Sample port line and use it to initialize the LastSolarPanelVoltage variable
  LastSolarPanelVoltage = ReadSolarPanelPosition();
  LastSmokeTowerState = ReadSmokeTowerIR();
  
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
  ES_Event_t TemperatureChange;


  //Default return event
  ReturnEvent.EventType = ES_NO_EVENT;

  //For posting when sun should move
  ES_Event_t MoveSunEvent;
  //Corresponds to number of LEDs to be on
  uint8_t energy_level;

  //Based on the state of the CurrentEnergyState variable choose one of the 
  //following blocks of code:
  switch(CurrentEnergyState)
    {
    case InitEnergyGame:
    {
      puts("In InitEnergyGame \r\n");
      if(ThisEvent.EventType == ES_INIT)
      {
        CurrentEnergyState = EnergyStandBy;
      }
    break;
    }
    
    case EnergyStandBy:
    {

      if((ThisEvent.EventType == START_GAME) && (ThisEvent.EventParam == 1))
      {
        puts("Energy game started \r\n");
        //post event to game service to 
        //1. Play coalplant audio
        ES_Event_t Event2Post;
        Event2Post.EventType = PLAY_LOOP;
        PostAudioService(Event2Post);
        //2. turn on pollution leds
        SR_WritePollution(6);
        //3. turn on energy leds with parameter all
        SR_WriteEnergy(6);
        //Initiate 5 s timer
        ES_Timer_InitTimer(SUN_POSITION_TIMER, FIVE_SEC);
        ES_Timer_InitTimer(COAL_ACTIVE_TIMER, FIVE_SEC);
        CurrentEnergyState = CoalPowered;

      }
    break;
    }
 
    case CoalPowered:
    {
      puts("The energy grid is coal powered \r\n");
      if((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == SUN_POSITION_TIMER))
      {
        //Move sun to new position by calling that service
        puts("Sun to be moved, tower unplugged \r\n");
        MoveSunEvent.EventType = ES_MOVE_SUN;
        MoveSunEvent.EventParam = 0;
        PostSunMovement(MoveSunEvent);
        //Change V_sun by one iteration
        ChangeSunVoltage();
        //Initiate 5 s timer
        ES_Timer_InitTimer(SUN_POSITION_TIMER, FIVE_SEC);
      }
      else if((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == COAL_ACTIVE_TIMER))
      {
        //Turn 1 temperature LED on
        TemperatureChange.EventType = CHANGE_TEMP;
        TemperatureChange.EventParam = 1;
        puts("Temp up by 1, too long in coal power state\r\n");
        PostGameManager(TemperatureChange);
        ES_Timer_InitTimer(COAL_ACTIVE_TIMER, FIVE_SEC);
        //to add in: Play "sad" audio tune TBD
      }
      else if((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == SOLAR_ACTIVE_TIMER))
      {
        //It could be that we changed to new state already whilst this event was still in the queue
        //Turn 1 temperature LED off Will we keep this? TBD
        TemperatureChange.EventType = CHANGE_TEMP;
        TemperatureChange.EventParam = 0;
        puts("Temp down by 1, enough solar energy produced\r\n");
        PostGameManager(TemperatureChange);
        //to add in: Play "happy" audio tune TBD

      }
      else if(ThisEvent.EventType == ES_TOWER_PLUGGED)
      {
        puts("Tower plugged, move to SolarPowered state \r\n");
        //post event to do the following:
        //1. Stop coalplant audio
        ES_Event_t Event2Post;
        Event2Post.EventType = STOP_LOOP;
        PostAudioService(Event2Post);
        //2. Turn "low" polution leds
        SR_WritePollution(2);
        //3. Turn "low" energy leds
        SR_WriteEnergy(2);
        //Call EvaluateAlignment to check alignment solar panel
        energy_level = EvaluateSolarAlignment();
        SR_WriteEnergy(2*energy_level);
        CurrentEnergyState = SolarPowered;
        ES_Timer_InitTimer(SOLAR_ACTIVE_TIMER, TEN_SEC);
      }

      else if(ThisEvent.EventType == RESET_ALL_GAMES)
      {
      	//1. Stop playing coalplant audio directly
        ES_Event_t Event2Post;
        Event2Post.EventType = STOP_LOOP;
        PostAudioService(Event2Post);
      	//2. Function to reset sun to original position
      	MoveSunEvent.EventType = ES_MOVE_SUN;
      	MoveSunEvent.EventParam = 1;
      	PostSunMovement(MoveSunEvent);
      	CurrentEnergyState = EnergyStandBy;
      }
      
      else if(ThisEvent.EventType == GAME_OVER)
      {
        CurrentEnergyState = EnergyGameOver;
      }
    break;
    }

    case SolarPowered:
    {
      puts("The energy grid is solar powered \r\n");
      if(ThisEvent.EventType == ES_TOWER_UNPLUGGED)
      {
        puts("Tower unplugged, move to CoalPowered state \r\n");
        //1. Play coalplant audio
        ES_Event_t Event2Post;
        Event2Post.EventType = PLAY_LOOP;
        PostAudioService(Event2Post);
        //2. Turn on pollution leds
        SR_WritePollution(6);
        //3. turn on energy leds with parameter all
        SR_WriteEnergy(6);
        CurrentEnergyState = CoalPowered;
        ES_Timer_InitTimer(COAL_ACTIVE_TIMER, FIVE_SEC);
      }
      else if((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == COAL_ACTIVE_TIMER))
      {
        //It could be that we changed to new state already whilst this event was still in the queue
        //Turn 1 temperature LED on
        TemperatureChange.EventType = CHANGE_TEMP;
        TemperatureChange.EventParam = 1;
        puts("Temp up by 1, too long in coal power state\r\n");
        PostGameManager(TemperatureChange);
        //to add in: Play "sad" audio tune TBD
      }
      else if((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == SOLAR_ACTIVE_TIMER))
      {
        //Turn 1 temperature LED off. Do we want to keep this? TBD
        TemperatureChange.EventType = CHANGE_TEMP;
        TemperatureChange.EventParam = 0;
        puts("Temp down by 1, enough solar energy produced\r\n");
        PostGameManager(TemperatureChange);
        ES_Timer_InitTimer(SOLAR_ACTIVE_TIMER, TEN_SEC);
        //to add in: Play "happy" audio tune TBD

      }
      else if((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == SUN_POSITION_TIMER))
      {
        //Initiate 5 s timer
        puts("Sun to be moved, tower plugged \r\n");
        ES_Timer_InitTimer(SUN_POSITION_TIMER, FIVE_SEC);
        //Move sun to new position by calling that service
        MoveSunEvent.EventType = ES_MOVE_SUN;
        MoveSunEvent.EventParam = 0;
        PostSunMovement(MoveSunEvent);
        //Change V_sun by one iteration
        ChangeSunVoltage();
        //Call EvaluateAlignment to check alignment solar panel
        energy_level = EvaluateSolarAlignment();
        //Write new value of EvaluateSolarAlignment to LEDs
        //6 LEDs to represent 3 different levels, so *2
        SR_WriteEnergy(2*energy_level);
        
      }
      else if(ThisEvent.EventType == ES_SOLARPOS_CHANGE)
      {
        puts("Solar panel moved, in tower plugged state \r\n");
        //Call EvaluateAlignment to check alignment solar panel
        energy_level = EvaluateSolarAlignment();
        SR_WriteEnergy(2*energy_level);
      }
      else if(ThisEvent.EventType == RESET_ALL_GAMES)
      {
        //Move sun to initial position by calling that service
        MoveSunEvent.EventType = ES_MOVE_SUN;
        MoveSunEvent.EventParam = 0;
        PostSunMovement(MoveSunEvent);
        CurrentEnergyState = EnergyStandBy;

      }
      else if(ThisEvent.EventType == GAME_OVER)
      {
        CurrentEnergyState = EnergyGameOver;
      }
    break;
    }	
    case EnergyGameOver:
    {
        //Blink final score leds for a couple of seconds TBD
        if(ThisEvent.EventType == RESET_ALL_GAMES)
        {
            //Move sun to initial position by calling that service
            MoveSunEvent.EventType = ES_MOVE_SUN;
            MoveSunEvent.EventParam = 0;
            PostSunMovement(MoveSunEvent);
            CurrentEnergyState = EnergyStandBy;
      }
    }
  }
  return ReturnEvent;
}

/****************************************************************************
EVENT CHECKING ROUTINES
****************************************************************************/

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
  static uint32_t CurrentSolarPanelVoltage;

  //Get CurrentSolarPanelVoltage from input line
  CurrentSolarPanelVoltage = ReadSolarPanelPosition();
  

  if(abs(CurrentSolarPanelVoltage-LastSolarPanelVoltage) >= V_threshold)
  {
    puts("Solarpanel position changed by threshold \r\n");
  	ThisEvent.EventType = ES_SOLARPOS_CHANGE;
  	PostEnergyProduction(ThisEvent);
  	AnyEvent.EventType = USERMVT_DETECTED;
    PostGameManager(AnyEvent);
    ReturnVal = true;
    LastSolarPanelVoltage = CurrentSolarPanelVoltage;
  }
  
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
      AnyEvent.EventType = USERMVT_DETECTED;
      PostGameManager(AnyEvent);
    }
    else
    {   
      ThisEvent.EventType = ES_TOWER_UNPLUGGED;
      PostEnergyProduction(ThisEvent);
      AnyEvent.EventType = USERMVT_DETECTED;
      PostGameManager(AnyEvent);
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
static uint32_t ReadSolarPanelPosition(void)
{
  uint32_t SolarPanelPosition[2];
  //Read analog input pin 
  ADC_MultiRead(SolarPanelPosition); 
  //printf("Solar panel position: %d \r\n", SolarPanelPosition[0]);
  return SolarPanelPosition[0];
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
  puts("Varying V_sun \r\n");
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
  //puts("Varying V_sun \r\n");
	uint8_t Alignment_param;
	uint32_t V_solar = ReadSolarPanelPosition();
	if(abs(V_sun - V_solar)<V_WELLALIGNED)
	{
		Alignment_param = 3;
	}
	else if(abs(V_sun - V_solar)<V_MEDIUMALIGNED)
	{
		Alignment_param = 2;
	}
	else
	{
		Alignment_param = 1;
	}
  printf("Good alignment: %d \r\n", Alignment_param);
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
  bool SmokeTowerIRState;
  //Read digital input pin 
  SmokeTowerIRState = HWREG(GPIO_PORT_EP + (GPIO_O_DATA + ALL_BITS)) & TOWER_HI;
  return SmokeTowerIRState;
}







