/***************************************************************************
 Module
   SunMovement.c

 Revision
   1.0.1

 Description
   This service regulates the movement of the sun using PWM with servo

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
#include "SunMovement.h"

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
#include "PWM16Tiva.h"

#define PULSEWIDTH 25000
#define SERVO_CHANNEL 0
#define MAX_RANGE 130
#define NO_OF_INCREMENTS 12

// module level defines
static uint8_t MyPriority;

/****************************************************************************
 Function
     InitSunMovement
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

bool InitSunMovement(uint8_t Priority)
{
  
  MyPriority = Priority;
  //Define PWM TIVA output to sun as PWM output
  ES_Event_t InitEvent;
  InitEvent.EventType = ES_INIT;
  return ES_PostToService(MyPriority, InitEvent);
}

/****************************************************************************
 Function
     PostSunMovement
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
bool PostSunMovement(ES_Event_t ThisEvent)
{
  return ES_PostToService( MyPriority, ThisEvent);
}


/****************************************************************************
 Function
     RunSunMovement
 
Parameters
   ES_Event_t : the event to process

 Returns
   ES_Event_t, ES_NO_EVENT if no event

 Description
     implements the service to move the sun around
 Notes

 Author
     Sander Tonkens, 11/1/18, 10:20
****************************************************************************/

ES_Event_t RunSunMovement(ES_Event_t ThisEvent)
{
  // Default return event
  ES_Event_t ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT;

  static uint8_t SunPosition = (1000/180)*(180 - MAX_RANGE)/2;

  if (ThisEvent.EventType == ES_INIT) {
    PWM_TIVA_SetPulseWidth((1000 + SunPosition)/0.8, SERVO_CHANNEL);
  }
  else if (ThisEvent.EventType == ES_MOVE_SUN)
  {
    if (ThisEvent.EventParam == 0) // Move every 5 seconds
    {
      // Move sun by 1/12th of a day
      PWM_TIVA_SetPeriod(PERIOD, SERVO_CHANNEL);
      SunPosition += (1000/180)*(MAX_RANGE/NO_OF_INCREMENTS);
      if (SunPosition > (1000 * MAX_RANGE/180)) {
        ReturnEvent.EventParam = ES_ERROR;
        puts("Error: sun position attempting to exceed max range.\r\n");
        return ReturnEvent;
      }
      printf("SunPosition in us is: %d\r\n", SunPosition);
      PWM_TIVA_SetPulseWidth((1000 + SunPosition)/0.8, SERVO_CHANNEL);
    }
    else if (ThisEvent.EventParam == 1) //Reset all games
    {
      // Return sun to its initial position
      SunPosition = (1000/180)*(180 - MAX_RANGE)/2;
      PWM_TIVA_SetPulseWidth((1000 + SunPosition)/0.8, SERVO_CHANNEL);
      puts("Sun reset to initial position.\r\n");
    }
  }
  return ReturnEvent;
}
