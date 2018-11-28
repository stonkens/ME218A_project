/****************************************************************************
 Module
   ShiftRegisterWrite.c

 Revision
   1.0.2

 Description
   This module acts as the low level interface to a write only shift register.

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 10/23/18 10:11 ston    implemented comments
 10/11/15 19:55 jec     first pass
 
****************************************************************************/
// the common headers for C99 types 
#include <stdint.h>
#include <stdbool.h>

// the headers to access the GPIO subsystem
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"
#include "termio.h"

/* include header files for this service
*/
#include "ShiftRegisterWrite.h"

// the headers to access the TivaWare Library
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"

#include "BITDEFS.H"

// flexibility defines
#define GPIO_PORT GPIO_PORTB_BASE //configure B on electrical design

// readability defines
#define DATA GPIO_PIN_0
#define DATA_HI BIT0HI
#define DATA_LO BIT0LO

#define SCLK GPIO_PIN_1
#define SCLK_HI BIT1HI
#define SCLK_LO BIT1LO

#define RCLK GPIO_PIN_2
#define RCLK_LO BIT2LO
#define RCLK_HI BIT2HI

#define GET_LOWEST(x) (~x & 0x000001)
#define ALL_BITS (0xff<<2)

#define SUN_MASK 0x0fffff
#define ENERGY_MASK 0xf03fff
#define POLLUTION_MASK 0xffc0ff
#define TEMPERATURE_MASK 0xffff00

#define SUN_POS 20
#define ENERGY_POS 14
#define POLLUTION_POS 8
#define TEMPERATURE_POS 0


//Private functions
static uint32_t SetXBits(uint8_t NumberOfBits);

// image of the last 32 bits written to SR
static uint32_t LocalRegisterImage=0;
static uint32_t UpdatedRegisterImage=0;



/****************************************************************************
 Function
     SR_Init

 Parameters
    Nothing

 Returns
    Nothing

 Description
    Initializes shift register by enabling peripheral clock and sets data 
    & Serial Clock low and sets Register Clock high
 Notes
      
 Author
    Sander Tonkens, 10/23/18, 10:32
****************************************************************************/
void SR_Init(void)
{

  // sets direction PB0, PB1 & PB2 as digital and output
  HWREG(GPIO_PORT + GPIO_O_DEN) |= (DATA_HI | SCLK_HI | RCLK_HI);
  HWREG(GPIO_PORT + GPIO_O_DIR) |= (DATA_HI | SCLK_HI | RCLK_HI);
  
  // sets data & SCLK line low and RCLK line high
  HWREG(GPIO_PORT + (GPIO_O_DATA + ALL_BITS)) &= (DATA_LO & SCLK_LO);
  HWREG(GPIO_PORT + (GPIO_O_DATA + ALL_BITS)) |= RCLK_HI;
  SR_Write(0);
  return;
}

// returns last 8 bits sent to shift register
/****************************************************************************
 Function
     SR_GetCurrentRegister

 Parameters
    Nothing

 Returns
    uint32_t returns last 8 values of NewValue

 Description
    Returns last 32 values of NewValue. Called by LCD_Write functions to 
    retrieve previous NewValue, and then modifies it if necessary
 Notes
      
 Author
    Sander Tonkens, 10/23/18, 10:32
****************************************************************************/
uint32_t SR_GetCurrentRegister(void)
{ 
  // LocalRegisterImage is static so gets last 8 values of NewValue
  return LocalRegisterImage;
}

/****************************************************************************
 Function
     SR_WriteSun

 Parameters
    uint8_t NewValue, value to be written into Serial in/Parallel Out shift register

 Returns
    Nothing

 Description
    Updates LED values associated with Sun LEDs
 Notes
    Calls SR_Write to execute toggling
 Author
    Sander Tonkens, 10/23/18, 10:32
****************************************************************************/
void SR_WriteSun(uint8_t NewValue)
{
  //translate NewValue into bit state
  uint32_t SunBinary;
  SunBinary = SetXBits(NewValue);
  //update LEDs associated with Sun display+call SR_Write
  //UpdatedRegisterImage = ((LocalRegisterImage & SUN_MASK) | (BitState<<x)
  UpdatedRegisterImage = ((LocalRegisterImage & SUN_MASK)) | (SunBinary<<SUN_POS);

  //SR_Write(UpdatedRegisterImage);
  return;
}
/****************************************************************************
 Function
     SR_WritePollution

 Parameters
    uint8_t, value to be written into Serial in/Parallel Out shift register

 Returns
    Nothing

 Description
    Updates LED values associated with Pollution LEDs
 Notes
    Calls SR_Write to execute toggling
 Author
    Sander Tonkens, 10/23/18, 10:32
****************************************************************************/
void SR_WritePollution(uint8_t NewValue)
{
  uint32_t PollutionBinary;
  PollutionBinary = SetXBits(NewValue);
  //update LEDs associated with Pollution display+call SR_Write
  //LocalRegisterImage = ((LocalRegisterImage & POLLUTION_MASK) | (BitState<<x)
  UpdatedRegisterImage = ((LocalRegisterImage & POLLUTION_MASK)) | (PollutionBinary<<POLLUTION_POS);
  SR_Write(UpdatedRegisterImage);
  return;
}

/****************************************************************************
 Function
     SR_WriteEnergy

 Parameters
    uint8_t, value to be written into Serial in/Parallel Out shift register

 Returns
    Nothing

 Description
    Updates LED values associated with Energy LEDs
 Notes
    Calls SR_Write to execute toggling
 Author
    Sander Tonkens, 10/23/18, 10:32
****************************************************************************/
void SR_WriteEnergy(uint8_t NewValue)
{
  //translate NewValue into bit state
  uint32_t EnergyBinary;
  EnergyBinary = SetXBits(NewValue);

  //update LEDs associated with Energy display+call SR_Write
  UpdatedRegisterImage = ((LocalRegisterImage & ENERGY_MASK)) | (EnergyBinary<<ENERGY_POS);
  SR_Write(UpdatedRegisterImage);
  return;
}
/****************************************************************************
 Function
     SR_WriteTemperature

 Parameters
    uint8_t, value to be written into Serial in/Parallel Out shift register

 Returns
    Nothing

 Description
    Updates LED values associated with Energy LEDs
 Notes
    Calls SR_Write to execute toggling
 Author
    Sander Tonkens, 10/23/18, 10:32
****************************************************************************/
void SR_WriteTemperature(uint8_t NewValue)
{
  //translate NewValue into bit state
  uint32_t TemperatureBinary;
  TemperatureBinary = SetXBits(NewValue);
  
  
  //update LEDs associated with Temperature display+call SR_Write
  UpdatedRegisterImage = ((LocalRegisterImage & TEMPERATURE_MASK)) | (TemperatureBinary<<TEMPERATURE_POS);
  printf("In SR_WriteTemperature, Value to write SR_WRITE = %d \r\n", UpdatedRegisterImage);
  SR_Write(UpdatedRegisterImage);
  return;
}
/****************************************************************************
 Function
     SR_Write

 Parameters
    uint8_t, value to be written into Serial in/Parallel Out shift register

 Returns
    Nothing

 Description
    Shifts out 24 bit input value into parellel output lines, starting with 
    MSB to ensure MSB is shifted out in Q7 ("highest" output line)
 Notes
    Calls macro to transfer MSB to LSB in order to shift in reverse wise
 Author
    Sander Tonkens, 10/23/18, 10:32
****************************************************************************/

void SR_Write(uint32_t NewValue){
  uint8_t BitCounter;
  // save local copy of NewValue, avoids potential errors when
  // NewValue gets updated whilst running function
  LocalRegisterImage = NewValue;

  // lower the RCLK
  HWREG(GPIO_PORT+(GPIO_O_DATA+ALL_BITS)) &= RCLK_LO;
 
  uint8_t CurrentBitEntry;
  // shift out data while pulsing SCLK  
  for(BitCounter = 0; BitCounter < 24; ++BitCounter)
  {
    // isolate LSB of NewValue and output to port
    CurrentBitEntry = GET_LOWEST(NewValue); //to be tested with ~, 0 should be lights on
    HWREG(GPIO_PORT+(GPIO_O_DATA+ALL_BITS)) &= DATA_LO;
    HWREG(GPIO_PORT+(GPIO_O_DATA+ALL_BITS)) |= CurrentBitEntry;
    // raise SCLK
    HWREG(GPIO_PORT+(GPIO_O_DATA+ALL_BITS)) |= SCLK_HI;
    // lower SCLK
    HWREG(GPIO_PORT+(GPIO_O_DATA+ALL_BITS)) &= SCLK_LO;
    // loop through bits in NewValue
    NewValue = (NewValue>>1);

  }
  // raise the register clock to latch the new data
  HWREG(GPIO_PORT+(GPIO_O_DATA+ALL_BITS)) |= RCLK_HI;
  return;
  
}

/****************************************************************************
 Function
     SetXBits

 Parameters
    uint8_t x, number of bits (starting from LSB) to be set

 Returns
    uint32_t, first (starting from LSB) bits set to 1

 Description
    Will produce an error if NumberOfBits >=32
 Notes
    
 Author
    Sander Tonkens, 10/23/18, 10:32
****************************************************************************/
static uint32_t SetXBits(uint8_t NumberOfBits)
{
  uint32_t BinaryRep = 0;
  BinaryRep = ~(~0 << NumberOfBits);
  return BinaryRep;
}
