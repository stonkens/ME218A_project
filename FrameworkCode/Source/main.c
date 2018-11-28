/****************************************************************************
 Module
     main.c
 Description
     starter main() function for Events and Services Framework applications
 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 08/21/17 12:53 jec     added this header as part of coding standard and added
                        code to enable as GPIO the port poins that come out of
                        reset locked or in an alternate function.
*****************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// the headers to access the GPIO subsystem
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"

#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"


// the headers to access the TivaWare Library
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"

#include "ES_Configure.h"
#include "PWM16Tiva.h"
#include "ADMulti.h"
#include "ES_Framework.h"
#include "ES_Port.h"
#include "termio.h"
#include "EnablePA25_PB23_PD7_PF0.h"

#include "EnergyProduction.h"
#include "ShiftRegisterWrite.h"
#include "BITDEFS.H"

#define clrScrn() printf("\x1b[2J")
#define goHome() printf("\x1b[1,1H")
#define clrLine() printf("\x1b[K")

int main(void)
{
  ES_Return_t ErrorType;

  // Set the clock to run at 40MhZ using the PLL and 16MHz external crystal
  SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN
      | SYSCTL_XTAL_16MHZ);
  TERMIO_Init();
  clrScrn();

  puts("\r Running Events and Services Framework \r\n");

  // reprogram the ports that are set as alternate functions or
  // locked coming out of reset. (PA2-5, PB2-3, PD7, PF0)
  // After this call these ports are set
  // as GPIO inputs and can be freely re-programmed to change config.
  // or assign to alternate any functions available on those pins
  PortFunctionInit();

  // hardware initialization
  HWREG(SYSCTL_RCGCGPIO) |= BIT2HI; // Port C
  while (!(HWREG(SYSCTL_PRGPIO) & BIT2HI));
  HWREG(SYSCTL_RCGCGPIO) |= BIT3HI; // Port D
  while (!(HWREG(SYSCTL_PRGPIO) & BIT3HI));
  HWREG(SYSCTL_RCGCGPIO) |= BIT5HI; // Port F
  while (!(HWREG(SYSCTL_PRGPIO) & BIT5HI));

  HWREG(SYSCTL_RCGCGPIO) |= BIT1HI; // Port B
  while (!(HWREG(SYSCTL_PRGPIO) & BIT1HI));
  ADC_MultiInit(2); //to be placed in main.c
  PWM_TIVA_Init(2); //2 servos: PB6=0, PB7=1
  // now initialize the Events and Services Framework and start it running
  ErrorType = ES_Initialize(ES_Timer_RATE_1mS);
  if (ErrorType == Success)
  {
    ErrorType = ES_Run();
  }
  //if we got to here, there was an error
  switch (ErrorType)
  {
    case FailedPost:
    {
      printf("Failed on attempt to Post\n");
    }
    break;
    case FailedPointer:
    {
      printf("Failed on NULL pointer\n");
    }
    break;
    case FailedInit:
    {
      printf("Failed Initialization\n");
    }
    break;
    default:
    {
      printf("Other Failure\n");
    }
    break;
  }
  for ( ; ;)
  {
    ;
  }
}

