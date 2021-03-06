/****************************************************************************************
*   GameManager.c
*   
* 
*
****************************************************************************************/
// PORT D
#define LEAF_DETECTOR_PORT0 BIT6HI
#define LEAF_DETECTOR_PORT1 BIT7HI
#define TEMP_LED_NUM 8

#include "GameManager.h"
#include "ES_Framework.h"


// the headers to access the GPIO subsystem
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"

// #include "ShiftRegisterWrite.h"
// #include "AudioService.h"
// include the header files of all games (need access to post functions)
#include "VotingGame.h"
#include "EnergyProduction.h"
#include "ShiftRegisterWrite.h"
#include "MeatSwitchDebounce.h"


/****************************** Private Functions & Variables **************************/
static uint8_t LEAF0LastState;
static uint8_t LEAF1LastState;
static uint8_t MyPriority;
static GameManagerState CurrentState = InitGState;

bool InitGameManager(uint8_t Priority) {
    // initialize ports (already set to input by default)
    HWREG(GPIO_PORTD_BASE + GPIO_O_DEN) |= (LEAF_DETECTOR_PORT0|LEAF_DETECTOR_PORT1);
    LEAF0LastState = HWREG(GPIO_PORTD_BASE + GPIO_O_DATA + ALL_BITS) & 
        LEAF_DETECTOR_PORT0;
    LEAF1LastState = HWREG(GPIO_PORTD_BASE + GPIO_O_DATA + ALL_BITS) & 
        LEAF_DETECTOR_PORT1;
    SR_Init();
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
    static uint8_t NumOfActiveGames = 0;
    
    ES_Event_t ReturnEvent;
    ReturnEvent.EventType = ES_NO_EVENT;

    switch (CurrentState) {
        case InitGState:
            if (ThisEvent.EventType == ES_INIT) {
                puts("GameManager in standby mode.\r\n");
                CurrentState = Standby;
            }
            else 
                puts("Error: did not receive ES_INIT event.\r\n");
            break;


        case Standby:
            if (ThisEvent.EventType == LEAF_IN_INCORRECT) {
                puts("Reflectivity too high, LEAF inserted incorrectly.\r\n");
                ES_Event_t Event2Post;
                // Event2Post.EventType = PLAY_LEAF_ERROR_AUDIO;
                // PostAudioService(Event2Post);
            }
            else if (ThisEvent.EventType == LEAF_IN_CORRECT) {
                // play welcoming audio
                ES_Event_t Event2Post;
                Event2Post.EventType = PLAY_WELCOMING_AUDIO;
                // PostAudioService(Event2Post);
                // turn on thermometer LEDs
                SR_WriteTemperature(Temperature);
                puts("LEAF inserted correctly. Going into welcome mode.\r\n");
                CurrentState = WelcomeMode;                
            }
            break;

        case WelcomeMode:
            if (ThisEvent.EventType == WELCOMING_AUDIO_DONE) {
                ES_Event_t Event2Post;
                Event2Post.EventType = START_GAME;
                Event2Post.EventParam = 1;
                PostEnergyProduction(Event2Post);
                NumOfActiveGames ++;
                puts("Starting first game.\r\n");

                // start timers
                ES_Timer_InitTimer(NEXT_GAME_TIMER, 10000);
                ES_Timer_InitTimer(USER_INPUT_TIMER, 30000);
                ES_Timer_InitTimer(GAME_END_TIMER, 60000);
                CurrentState = GameActive;
            }
            else if (ThisEvent.EventType == LEAF_REMOVED) {
                
                // maybe add turn off all LED function to SR? --> Adds
                SR_Write(0);
                // SR_WriteTemperature(0);

                ES_Event_t Event2Post;
                // Event2Post.EventType = STOP_WELCOMING_AUDIO;
                // PostAudioService(Event2Post);
                CurrentState = Standby;
                puts("LEAF removed; going back to standby.\r\n");
            }
            break;

        case GameActive:
            if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam ==
                NEXT_GAME_TIMER)) {
                if (NumOfActiveGames < 3) {
                    NumOfActiveGames ++;
                    ES_Event_t Event2Post;
                    Event2Post.EventType = START_GAME;
                    if (NumOfActiveGames == 2) {
                        ES_Timer_InitTimer(NEXT_GAME_TIMER, 10000);
                        Event2Post.EventParam = 2;
                        PostMeatSwitchDebounce(Event2Post);
                        puts("10s timer expired: starting second game.\r\n");
                    }
                    else if (NumOfActiveGames == 3) {
                        Event2Post.EventParam = 3;
                        PostVotingGame(Event2Post);
                        puts("10s timer expired: starting third game.\r\n");
                    }
                }
            }

            /* else if (((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam ==
                USR_INPUT_TIMER)) || (ThisEvent.EventType == LEAF_REMOVED)) { */
            else if (((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam ==
                USER_INPUT_TIMER))) {
                puts("No user input detected for 30s, or LEAF removed. Resetting all games.\r\n");
                SR_WriteTemperature(0);
                ES_Event_t Event2Post;
                Event2Post.EventType = RESET_ALL_GAMES;
                // post event to distribution list
                // ES_PostList00(Event2Post);
                CurrentState = Standby;
            }
                
            else if (ThisEvent.EventType == USERMVT_DETECTED) {
                ES_Timer_InitTimer(USER_INPUT_TIMER, 30000);
                puts("Resetting 30s timer.\r\n");
            }
            // else if 60s timer expires
            // else if change_temp event is posted
            break;

        default:
            puts("Error: GameManager entered unknown state.\r\n");
    }

    return ReturnEvent;
}

// void CheckLEAFSwitch() {
//     uint8_t LEAFSwitchCurrentState = HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + 
//         ALL_BITS) & LEAF_SWITCH_PORT;
//     if (LEAFSwitchCurrentState != LEAFSwitchLastState) {
//         ES_Event_t Event2Post;
//         // input is pulled HI when the switch is open
//         if (LEAFSwitchCurrentState)
//             Event2Post.EventType = LEAF_SWITCH_OPEN;
//         else
//             Event2Post.EventType = LEAF_SWITCH_CLOSED;
//         ES_PostToService(MyPriority, Event2Post);
//         LEAFSwitchLastState = LEAFSwitchCurrentState;
//     }
// }


bool CheckLEAFInsertion() {
    uint8_t LEAF0CurrState = HWREG(GPIO_PORTD_BASE + GPIO_O_DATA + 
        ALL_BITS) & LEAF_DETECTOR_PORT0;
    if (LEAF0CurrState != LEAF0LastState) {
      ES_Event_t Event2Post;  
      if (LEAF0CurrState)
          Event2Post.EventType = LEAF_REMOVED;
      else
          Event2Post.EventType = LEAF_IN_CORRECT;
      ES_PostToService(MyPriority, Event2Post);
      LEAF0LastState = LEAF0CurrState;
      return true;
    }
  
/*  
    uint8_t LEAF1CurrState = HWREG(GPIO_PORTD_BASE + GPIO_O_DATA + 
        ALL_BITS) & LEAF_DETECTOR_PORT1;
    if ((LEAF0CurrState != LEAF0LastState) || (LEAF1CurrState != LEAF1LastState)) {
        ES_Event_t Event2Post;
        if (LEAF0CurrState && LEAF1CurrState) {
            // background is white
            Event2Post.EventType = LEAF_IN_INCORRECT;
        }
        else if (!LEAF0CurrState && !LEAF1CurrState) {
            // background is black and leaf is inserted correctly
            LEAF0LastState = LEAF0CurrState;
            LEAF1LastState = LEAF1CurrState;
            Event2Post.EventType = LEAF_IN_CORRECT;
        }
        else {
            if (LEAF0CurrState) {
                // assuming LEAF_DETECTOR_PORT0 sets the lower threshold
                puts("Error: something went wrong with LEAF detection.\r\n");
                return false;
            }
            else {
                // background is gray
                Event2Post.EventType = LEAF_REMOVED;
            }
        }
        ES_PostToService(MyPriority, Event2Post);
        LEAF0LastState = LEAF0CurrState;
        LEAF1LastState = LEAF1LastState;
        return true;
      }
*/
    return false;
}
