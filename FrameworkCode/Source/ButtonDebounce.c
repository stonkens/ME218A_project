/****************************************************************************************
*   ButtonDebounce.c
*   
* 
*
****************************************************************************************/

// include header files for this service
#include "ButtonDebounce.h"

// include header files for post function access
#include "VotingGame.h"

// include header files for hardware access
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"

// include header files for the framework
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "ES_ShortTimer.h"

#define DEBOUNCE_TIME 100
#define YES_BUTTON_PORT BIT1HI;
#define NO_BUTTON_PORT BIT0HI;
#define SWITCH_PORT BIT2HI;

static uint8_t MyPriority;
// use lines on Port D
static const NumOfButtons = 3;
static uint8_t ButtonPorts[] = {YES_BUTTON_PORT, NO_BUTTON_PORT, SWITCH_PORT};
static uint8_t ButtonLastStates[3];
static ES_EventType_t Events2Post[] = {VOTED_YES, VOTED_NO, SWITCH_HIT};

bool InitButtonDebounce(uint8_t MyPriority) {
    MyPriority = Priority;
    for (int i=0; i < NumOfButtons; i++) {
        // initialize port as digital
        HWREG(GPIO_PORTD_BASE + GPIO_O_DEN) |= ButtonPorts[i];
        // read initial state of buttons
        ButtonLastStates[i] = HWREG(GPIO_PORTD_BASE + GPIO_O_DATA + ALL_BITS) 
            & ButtonPorts[i];
    }
    return true;
}

bool PostButtonDebounce(ES_Event_t ThisEvent) {
    return ES_PostToService(MyPriority, ThisEvent);
}

ES_Event_t RunButtonDebounce(ES_Event_t ThisEvent) {
    ES_Event_t ReturnEvent;
    ReturnEvent.EventType = ES_NO_EVENT;

    if (ThisEvent.EventType == BUTTON_DOWN) {
        uint8_t ButtonNum = ThisEvent.EventParam;
        printf("Button %d was pressed.\r\n", ButtonNum);
        ES_Timer_InitTimer((BUTTON_TIMER + ButtonNum), DEBOUNCE_TIME);
    }

    else if (ThisEvent.EventType == BUTTON_UP) {
        uint8_t ButtonNum = ThisEvent.EventParam;
        ES_Timer_StopTimer(BUTTON_TIMER + ButtonNum);
        puts("Debounced.\r\n");
    }

    else if (ThisEvent.EventType == ES_TIMEOUT) {
        uint8_t ButtonNum = ThisEvent.EventParam - BUTTON_TIMER;
        if ((TimerNum >= 0) && (TimerNum < NumOfButtons) {
            printf("Button %d pressed; posting to voting game.\r\n", ButtonNum);            
            PostVotingGame(Events2Post[ButtonNum]);
        }
    }
    return ReturnEvent;
}

void CheckButtonPress() {
    for (int i=0; i < NumOfButtons; i++) {
        uint8_t ButtonCurrState = HWREG(GPIO_PORTD_BASE + GPIO_O_DATA + ALL_BITS) 
            & ButtonPorts[i];
        if (ButtonCurrState != ButtonLastStates[i]) {
            ES_Event_t Event2Post;
            if (ButtonCurrState)   
                Event2Post.EventType = BUTTON_DOWN;
            else
                Event2Post.EventType = BUTTON_UP;
            Event2Post.EventParam = i;
            ES_PostToService(MyPriority, Event2Post);
            ButtonLastStates[i] = ButtonCurrState;
        }
    }
}





