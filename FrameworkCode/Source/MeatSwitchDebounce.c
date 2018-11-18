/***************************************************************************
 Module
   MeatSwitchDebounce.c

 Revision
   1.0.1

 Description
   This module posts events to Game Manager SM when MeatTracker microswitch's
   state has changed

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 11/12/18 11:11 ston    First pass
 
****************************************************************************/
//----------------------------- Include Files -----------------------------*/
// the common headers for C99 types 
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* include header files for this service
*/
#include "MeatSwitchDebounce.h"

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
//#include "GameManager.h"

// flexibility defines
#define GPIO_PORT GPIO_PORTB_BASE //configure B on electrical design

//readability defines
#define MEAT GPIO_PIN_3
#define MEAT_HI BIT3HI
#define MEAT_LO BIT3LO

static bool ReadMeatSwitchPress(void);

// Private variables
static uint8_t MyPriority;
static bool LastSwitchState;
static MeatSwitchDebounceState CurrentState;

// module level defines
static uint8_t DebounceTime=250;

/****************************************************************************
 Function
     InitMeatSwitchDebounce
 Parameters
     unit8_t Priority, priority of the state machine

 Returns
     bool true in case of no errors

 Description
     Takes service's priority number, initializes TIVA and starts debounce
     timer
 Notes
	 State machine starts in debouncing state, Ready2Sample is the default 
	 state if no meat switch presses have occured in the recent history
 Author
	 Sander Tonkens, 11/1/18, 10:20
****************************************************************************/
bool InitMeatSwitchDebounce(uint8_t Priority)
{
  ES_Event_t ThisEvent;

	MyPriority = Priority;

   //Set MicroSwitch as digital input
  HWREG(GPIO_PORT + GPIO_O_DEN) |= (MEAT_HI);
  HWREG(GPIO_PORT + GPIO_O_DIR) &= (MEAT_LO);

	//Initialize LastSwitchState
	LastSwitchState = ReadMeatSwitchPress();

	CurrentState = Debouncing;
	//Start debounce timer (timer posts to MeatSwitchDebounceSM when timeout)
	ES_Timer_InitTimer(DEBOUNCE_TIMER, DebounceTime); 
  ThisEvent.EventType = ES_INIT;
	return true;
}

/****************************************************************************
 Function
     PostMeatSwitchDebounce
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
bool PostMeatSwitchDebounce(ES_Event_t ThisEvent)
{
	return ES_PostToService(MyPriority, ThisEvent);
}


/****************************************************************************
 Function
     RunMeatSwitchDebounceSM
 
Parameters
   ES_Event_t : the event to process

 Returns
   ES_Event_t, ES_NO_EVENT if no error ES_ERROR otherwise

 Description
     Implements the state machine for Meat Switch Debounce
 Notes

 Author
     Sander Tonkens, 11/1/18, 10:20
****************************************************************************/
ES_Event_t RunMeatSwitchDebounceSM(ES_Event_t ThisEvent)
{
	ES_Event_t ReturnEvent;
	ReturnEvent.EventType = ES_NO_EVENT;
	
	switch(CurrentState)
	{
		case Debouncing:
		{
			//	If EventType is ES_TIMEOUT & parameter is debounce timer number
    	if((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == DEBOUNCE_TIMER))
			{
				CurrentState = Ready2Sample;
			}
			break;
		}
		
		case Ready2Sample:
		{
			if(ThisEvent.EventType == DB_MEAT_SWITCH_UP)
			{
				ES_Timer_InitTimer(DEBOUNCE_TIMER, DebounceTime); 
				CurrentState = Debouncing;
        //Post this event to Game Manager SM
			}
	    else if(ThisEvent.EventType == DB_MEAT_SWITCH_DOWN)
	    {
				ES_Timer_InitTimer(DEBOUNCE_TIMER, DebounceTime); 
        CurrentState = Debouncing;
	      //Post this event to Game Manager SM
			}        
			break;
		}
	}
	return ReturnEvent;
}


/****************************************************************************
 Function
     CheckMeatSwitchEvents
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
bool CheckMeatSwitchEvents(void)
{
	ES_Event_t ThisEvent;
	ES_Event_t AnyEvent;
	bool ReturnVal = false;
	bool CurrentSwitchState;

	CurrentSwitchState = ReadMeatSwitchPress();
	
	//If the CurrentSwitchState is different from the LastSwitchState
	if(CurrentSwitchState != LastSwitchState)
	{
		ReturnVal = true;
		//If the CurrentSwitchState is down		
		if (CurrentSwitchState == 0)
		{
			ThisEvent.EventType = DB_MEAT_SWITCH_DOWN;
			PostMeatSwitchDebounce(ThisEvent);

			AnyEvent.EventType = ES_USERMVT_DETECTED;
  		//Post ES_USERMVT_DETECTED to game manager //Connie
  		//PostGameManager(AnyEvent);
		}
		else
		{		
			ThisEvent.EventType = DB_MEAT_SWITCH_UP;
			PostMeatSwitchDebounce(ThisEvent);
		}
	}	
	LastSwitchState = CurrentSwitchState;
	return ReturnVal;
}

static bool ReadMeatSwitchPress(void)
{
	bool SwitchState;
	SwitchState = HWREG(GPIO_PORT + GPIO_O_DATA + ALL_BITS) & MEAT_HI;
	return SwitchState;
}
