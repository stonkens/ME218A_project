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

#define ONE_SEC 1000
#define FIVE_SEC (ONE_SEC*5)

// Private functions
static void TestCalibration(void);
static void CharacterizeSpace(void);
static uint8_t ReadSolarPanelPosition(void);
static bool ReadSmokeTowerIR(void)

// module level defines
static uint8_t MyPriority;
static EnergyGameState CurrentEnergyState;
static uint16_t TimeOfLastRise;
static uint16_t TimeOfLastFall;
static uint16_t LengthOfDot;
static uint16_t FirstDelta;


static uint16_t LastSolarPanelVoltage;
static bool LastSmokeTowerState;



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
        //Initiate 5 s timer
        //Move sun to new position by calling that service
        //Change V_sun by one iteration
      }
      else if(ThisEvent.EventType == ES_TOWER_PLUGGED)
      {
        //post event to do the following:
        //1. Stop coalplant audio
        //2. Turn off polution leds
        //3. Turn off energy leds with parameter all
        //Call EvaluateAlignment to check alignment solar panel
        CurrentEnergyState = SolarPowered
      }
      else if((ThisEvent.EventType == ES_AUDIO_END) && (ThisEvent.EventParam == COAL_AUDIO))
      {
        //1. Play coalplant audio
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
      }
      else if((ThisEvent.EventType == ES_SOLARPOS_CHANGE))
      {
        //Call EvaluateAlignment to check alignment solar panel
      }
      else if((ThisEvent.EventType == ES_RESET_ALL_GAMES))
      {
        //Move sun to initial position by calling that service
        CurrentEnergyState = EnergyStandBy
        //EnergyStandBy or InitEnergyGame
      }
    break;
    }

    case EOC_WaitFall:
    {
      if(ThisEvent.EventType == ES_FALLING_EDGE)
      {
        TimeOfLastFall = ThisEvent.EventParam;
        CurrentEnergyState = EOC_WaitRise;
      }
      else if(ThisEvent.EventType == DB_BUTTON_DOWN)
      {
        CurrentEnergyState = CalWaitForRise;
        FirstDelta = 0;
      }
      else if(ThisEvent.EventType == ES_EOC_DETECTED)
      {
        CurrentEnergyState = DecodeWaitFall;
      }
    break;
    }

    case DecodeWaitRise:
    {
      if(ThisEvent.EventType == ES_RISING_EDGE)
      {
        TimeOfLastRise = ThisEvent.EventParam;
        CurrentEnergyState = DecodeWaitFall;
        CharacterizeSpace();          
      }
      else if (ThisEvent.EventType == DB_BUTTON_DOWN)
      {
        CurrentEnergyState = CalWaitForRise;
        FirstDelta = 0;
      }
    break;
    }

    case DecodeWaitFall:
    {
      if(ThisEvent.EventType == ES_FALLING_EDGE)
      {
        TimeOfLastFall = ThisEvent.EventParam;
        CurrentEnergyState = DecodeWaitRise;
        CharacterizePulse();         
      }
      else if(ThisEvent.EventType == DB_BUTTON_DOWN)
      {
        CurrentEnergyState = CalWaitForRise;
        FirstDelta = 0;
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
  bool ReturnVal = false;
  uint16_t CurrentSolarPanelVoltage;

  //Get CurrentSolarPanelVoltage from input line
  CurrentSolarPanelVoltage = ReadSolarPanelPosition();
  
  //Analog signal, to be changed accordingly
  //If the state of the Morse input line has changed
  if(abs(CurrentSolarPanelVoltage-ReferenceLastSolarPanelVoltage) <= V_threshold)
  {
    //If the current state of the input line is high
    if(CurrentSolarPanelVoltage !=0)
    {
      //PostEvent RisingEdge with parameter of the Current Time
      ThisEvent.EventType = ES_RISING_EDGE;
      ThisEvent.EventParam = ES_Timer_GetTime();
      PostEnergyProduction(ThisEvent);
      //printf("R");
    }

    else
    {
      //PostEvent FallingEdge with parameter of the Current Time
      ThisEvent.EventType = ES_FALLING_EDGE;
      ThisEvent.EventParam = ES_Timer_GetTime();
      PostEnergyProduction(ThisEvent);
      //printf("F");
    }
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
    }
    else
    {   
      ThisEvent.EventType = ES_TOWER_UNPLUGGED;
      PostEnergyProduction(ThisEvent);
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
   TestCalibration
 Parameters
   Nothing
 Returns
   Nothing
 Description
   Checks whether an acceptable dot length has been registered, posts event
 Notes
   This implementation posts an event to PostEnergyProduction when Calibration is
   completed
 Author
   Sander Tonkens, 11/2/18, 19:39
****************************************************************************/
static void TestCalibration(void){

	uint16_t SecondDelta;
	ES_Event_t ThisEvent;
  
  //If calibration is just starting (FirstDelta is 0)
	if(FirstDelta == 0)
	{
    //Set FirstDelta to most recent pulse width
		FirstDelta = TimeOfLastFall - TimeOfLastRise;
	}

	else
	{
    //Set SecondDelta to most recent pulse width
		SecondDelta = TimeOfLastFall - TimeOfLastRise;
    
    //FirstDelta represents valid dot length
		if ((100.0*FirstDelta / SecondDelta) <= 33.33)
		{
      //Save FirstDelta as LengthOfDot
			LengthOfDot = FirstDelta;
      //CalCompleted, Post to EnergyProductionSM
			ThisEvent.EventType = ES_CAL_COMPLETED;
			PostEnergyProduction(ThisEvent);
		}
    
		//SecondDelta represents valid dot length
		else if ((100.0 * FirstDelta / SecondDelta) > 300.0)
		{
      //Save SecondDelta as LengthOfDot
			LengthOfDot = SecondDelta;
      //CalCompleted, Post to EnergyProductionSM
			ThisEvent.EventType = ES_CAL_COMPLETED;
			PostEnergyProduction(ThisEvent);
		}
    //Else (prepare for next pulse)
		else
		{
      //Reiterate, until valid dot length has been identified
			FirstDelta = SecondDelta;
		}
	}
	return;
}

/****************************************************************************
 Function
   CharacterizeSpace
 Parameters
   Nothing
 Returns
   Nothing
 Description
   Checks for character spaces and end of word spaces
 Notes
   This implementation posts an event to PostEnergyProduction when a valid or 
   invalid space is detected
 Author
   Sander Tonkens, 11/2/18, 19:39
****************************************************************************/
static void CharacterizeSpace(void){
  uint16_t LastInterval;
  ES_Event_t Event2Post;
  
  LastInterval = TimeOfLastRise - TimeOfLastFall;
  
  //LastInterval does not correspond to a dot space
  if (LastInterval > 2 * LengthOfDot)
  {
    
    //LastInterval corresponds to Character Space
    if ((LastInterval >= (3*(LengthOfDot - 10))) && (LastInterval <= (3*(LengthOfDot +10))))
    {
      //printf("EOC");
      Event2Post.EventType = ES_EOC_DETECTED;
      PostEnergyProduction(Event2Post);
      PostMorseDecode(Event2Post);
    }
    else
    {
      //LastInterval corresponds to word space
      if((LastInterval >=(7*(LengthOfDot - 10))) && (LastInterval <= (7*(LengthOfDot +10))))
      {
        //printf("EOW");
        Event2Post.EventType = ES_EOW_DETECTED;
        PostEnergyProduction(Event2Post);
        PostMorseDecode(Event2Post);
      }
      //LastInterval does not satisfy any requirements
      else
      {
        Event2Post.EventType = ES_BAD_SPACE;
        PostEnergyProduction(Event2Post);
        PostMorseDecode(Event2Post); 
      }
    }
  }
  return;
}  

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
     ReadSmokeTowerIR

 Parameters
    Nothing

 Returns
    uint8_t returns state of 

 Description
    Returns bit 3 of the TIVA Port B, which reads state of morse coder
 Notes
      
 Author
    Sander Tonkens, 11/1/18, 10:32
****************************************************************************/
static bool ReadSmokeTowerIR(void)
{
  uint8_t SmokeTowerIRState=0;
  //Read digital input pin (as in Morselements)
  //SmokeTowerIRState = HWREG(GPIO_PORTB_BASE + (GPIO_O_DATA + ALL_BITS)) & BIT3HI;
  return SmokeTowerIRState;
}





