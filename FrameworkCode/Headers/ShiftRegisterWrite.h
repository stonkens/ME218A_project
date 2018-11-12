// create your own header file comment block
// and protect against multiple inclusions
#ifndef SHIFTREGISTERWRITE_H
#define SHIFTREGISTERWRITE_H
// the common headers for C99 types 
#include <stdint.h>
#include <stdbool.h>

// the headers to access the GPIO subsystem
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"

// the headers to access the TivaWare Library
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"

#include "BITDEFS.H"

// flexibility defines
#define GPIO_PORT GPIO_PORTB_BASE //configure B on electrical design
#define PORT_HI BIT1HI //configure 1 (=B) on electrical design
#define PORT_LO BIT1LO //configure 1 (=B) on electrical design

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

#define GET_LSB(x) (x & 0x0001)
void SR_Init(void);
uint32_t SR_GetCurrentRegister(void);
void SR_Write(uint32_t NewValue);
void SR_WritePollution(uint32_t NewValue)
void SR_WriteSun(uint32_t NewValue)
void SR_WriteEnergy(uint32_t NewValue)
void SR_WriteTemperature(uint32_t NewValue)

#endif 