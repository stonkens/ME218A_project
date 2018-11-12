/****************************************************************************************
*   GameManager.c
*   
* 
*
****************************************************************************************/
#define LEAF_SWITCH_PORT BIT0HI
#define IR_LED_ON BIT1HI
#define IR_LED_OFF BIT1LO
#define PHOTOTRANSISTOR_PORT BIT2HI
#define TEMP_LED_NUM 8

#include "GameManager.h"
#include "ES_Framework.h"
#include "ShiftRegisterWrite.h"
// include the header files of all games (need access to post functions)


/****************************** Private Functions & Variables **************************/
static uint8_t LEAFSwitchLastState;
static uint8_t MyPriority;
static GameManagerState CurrentState = InitPState;

bool InitGameManager(uint8_t Priority) {
    LEAFSwitchLastState = HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + ALL_BITS) 
        & LEAF_SWITCH_PORT;

    ES_Event_t InitEvent;
    InitEvent.EventType = ES_INIT;
    if (ES_PostToService(MyPriority, InitEvent) == true) {
        return true;
    }
    return false;
}

bool PostGameManager(ES_Event_t ThisEvent) {
    return ES_PostToService(MyPriority, ThisEvent);
}

ES_Event_t RunGameManager(ES_Event_t ThisEvent) {

    static uint8_t Temperature = TEMP_LED_NUM;
    
    ES_Event_t ReturnEvent;
    ReturnEvent.EventType = ES_NO_EVENT;

    switch (CurrentState) {
        case InitPState:
            if (ThisEvent.EventType == ES_INIT)
                CurrentState = Standby;
            else 
                puts("Error: did not receive ES_INIT event.\r\n");
            break;


        case Standby:
            if (ThisEvent.EventType == LEAF_SWITCH_CLOSED) {
                // turn on IR LED
                HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + ALL_BITS) |= IR_LED_ON;
                CurrentState = LEAFInserted;
            }
            break;

        case LEAFInserted:

            // will probably have to implement 1s timer

            ES_Event_t Event2Post;
            // if phototransistor reads HI, play audio and go back to standby mode
            if (HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + ALL_BITS) & PHOTOTRANSISTOR_PORT) {
                puts("reflectivity too high, LEAF inserted incorrectly.\r\n");
                HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + ALL_BITS) &= IR_LED_OFF;
                Event2Post.EventType = PLAY_LEAF_ERROR_AUDIO;
                PostAudioService(Event2Post);
                CurrentState = Standby;
            }
            // otherwise turn off LED and go into startup mode
            else {
                HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + ALL_BITS) &= IR_LED_OFF;
                Event2Post.EventType = PLAY_WELCOMING_AUDIO;
                PostAudioService(Event2Post);
                SR_WriteTemperature(Temperature);
                CurrentState = Startup;
            }
            break;


        case Startup:
            if (ThisEvent.EventType == WELCOMING_AUDIO_DONE) {
                ES_Event_t Event2Post;
                Event2Post.EventType = START_ENERGY_GAME;
                PostEnergyGame(Event2Post);

                // start timers

                CurrentState = GameActive;
            }


        default:
            puts("Error: GameManager entered unknown state.\r\n");
    }
}

void CheckLEAFSwitch() {
    uint8_t LEAFSwitchCurrentState = HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + 
        ALL_BITS) & LEAF_SWITCH_PORT;
    if (LEAFSwitchCurrentState != LEAFSwitchLastState) {
        ES_Event_t Event2Post;
        // input is pulled HI when the switch is open
        if (LEAFSwitchCurrentState)
            Event2Post.EventType = LEAF_SWITCH_OPEN;
        else
            Event2Post.EventType = LEAF_SWITCH_CLOSED;
        ES_PostToService(MyPriority, Event2Post);
        LEAFSwitchLastState = LEAFSwitchCurrentState;
    }
}
