/****************************************************************************************
*   GameManager.c
*   
* 
*
****************************************************************************************/
#define LEAF_DETECTOR_PORT0 BIT0HI
#define LEAF_DETECTOR_PORT1 BIT1HI
#define TEMP_LED_NUM 8

#include "GameManager.h"
#include "ES_Framework.h"
#include "ShiftRegisterWrite.h"
#include "AudioService.h"
// include the header files of all games (need access to post functions)


/****************************** Private Functions & Variables **************************/
static uint8_t LEAF0LastState;
static uint8_t LEAF1LastState;
static uint8_t MyPriority;
static GameManagerState CurrentState = InitPState;

bool InitGameManager(uint8_t Priority) {
    // initialize ports (already set to input by default)
    HWREG(GPIO_PORTB_BASE + GPIO_O_DEN) |= (LEAF_DETECTOR_PORT0|LEAF_DETECTOR_PORT1);
    LEAF0LastState = HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + ALL_BITS) & 
        LEAF_DETECTOR_PORT0;
    LEAF1LastState = HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + ALL_BITS) & 
        LEAF_DETECTOR_PORT1;

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
        case InitPState:
            if (ThisEvent.EventType == ES_INIT)
                CurrentState = Standby;
            else 
                puts("Error: did not receive ES_INIT event.\r\n");
            break;


        case Standby:
            if (ThisEvent.EventType == LEAF_IN_INCORRECT) {
                puts("Reflectivity too high, LEAF inserted incorrectly.\r\n");
                ES_Event_t Event2Post;
                Event2Post.EventType = PLAY_LEAF_ERROR_AUDIO;
                PostAudioService(Event2Post);
            }
            else if (ThisEvent.EventType == LEAF_IN_CORRECT) {
                // play welcoming audio
                ES_Event_t Event2Post;
                Event2Post.EventType = PLAY_WELCOMING_AUDIO;
                PostAudioService(Event2Post);
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
                PostEnergyGame(Event2Post);
                NumOfActiveGames ++;
                puts("Starting first game.\r\n");

                // start timers
                ES_Timer_InitTimer(10S_TIMER, 10000);
                ES_Timer_InitTimer(30S_TIMER, 30000);
                ES_Timer_InitTimer(60S_TIMER, 60000);
                CurrentState = GameActive;
            }
            else if (ThisEvent.EventType == LEAF_REMOVED) {
                
                // maybe add turn off all LED function to SR?
                SR_WriteTemperature(0);

                ES_Event_t Event2Post;
                Event2Post.EventType = STOP_WELCOMING_AUDIO;
                PostAudioService(Event2Post);
                CurrentState = Standby;
            }
            break;

        case GameActive:
            if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam ==
                10S_TIMER)) {
                if (NumOfActiveGames < 3) {
                    NumOfActiveGames ++;
                    ES_Event_t Event2Post;
                    Event2Post.EventType = START_GAME;
                    if (NumOfActiveGames == 2) {
                        ES_Timer_InitTimer(10S_TIMER, 10000);
                        Event2Post.EventParam = 2;
                        PostMeatGame(Event2Post)
                        puts("Starting second game.\r\n");
                    }
                    else if (NumOfActiveGames == 3) {
                        Event2Post.EventParam = 3;
                        PostVotingGame(Event2Post);
                        puts("Starting third game.\r\n");
                    }
                }
            }

            else if (((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam ==
                30S_TIMER)) || (ThisEvent.EventType == LEAF_REMOVED)) {
                SR_WriteTemperature(0);
                ES_Event_t Event2Post;
                Event2Post.EventType = RESET_ALL_GAMES;
                // post event to distribution list
                ES_PostList00(ThisEvent);
            }

            // else if 60s timer expires
            // else if change_temp event is posted

        default:
            puts("Error: GameManager entered unknown state.\r\n");
    }
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


void CheckLEAFInsertion() {
    uint8_t LEAF0CurrState = HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + 
        ALL_BITS) & LEAF_DETECTOR_PORT0;
    uint8_t LEAF1CurrState = HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + 
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
                return;
            }
            else {
                // background is gray
                Event2Post.EventType = LEAF_REMOVED;
            }
        }
        ES_PostToService(MyPriority, Event2Post);
        LEAF0LastState = LEAF0CurrState;
        LEAF1LastState = LEAF1LastState;
    }
}
