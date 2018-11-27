/****************************************************************************************
*   GameManager.c
*   
*   to do's: modify LEAF event checker to be analog; game over event; change temp event
*
****************************************************************************************/
// PORT D
#define TEMP_LED_NUM 8

#include "GameManager.h"
#include "ES_Framework.h"

// the headers to access the GPIO subsystem
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"

// include the header files of all games (need access to post functions)
#include "VotingGame.h"
#include "EnergyProduction.h"
#include "ShiftRegisterWrite.h"
#include "MeatSwitchDebounce.h"
#include "ADMulti.h"

#include "AudioService.h"

#define AD_VOLTAGE(x) (int)(x*4095/3.3)

#define REF_STATE1_HI 1.0
#define REF_STATE2_LO 1.5
#define REF_STATE2_HI 2.0
#define REF_STATE3_LO 2.5


#define REF_STATE1_HI_AD AD_VOLTAGE(REF_STATE1_HI) 
#define REF_STATE2_LO_AD AD_VOLTAGE(REF_STATE2_LO) 
#define REF_STATE2_HI_AD AD_VOLTAGE(REF_STATE2_HI) 
#define REF_STATE3_LO_AD AD_VOLTAGE(REF_STATE3_LO)

/****************************** Private Functions & Variables **************************/
static uint8_t LEAFLastState;
static uint8_t MyPriority;
static GameManagerState CurrentState = InitGState;

static uint32_t ReadLEAFState(void);

bool InitGameManager(uint8_t Priority) {
    // initialize ports (already set to input by default)
    // ports initialized in ADCMultiInit
    // HWREG(GPIO_PORTD_BASE + GPIO_O_DEN) |= LEAF_DETECTOR_PORT;
    // LEAFSwitchLastState = HWREG(GPIO_PORTD_BASE + GPIO_O_DATA + ALL_BITS) & 
    //    LEAF_DETECTOR_PORT;
    SR_Init();
    printf("REF_STATE1_HI_AD = %d", REF_STATE1_HI_AD);
    printf("REF_STATE2_LO_AD = %d", REF_STATE2_LO_AD);
    printf("REF_STATE2_HI_AD = %d", REF_STATE2_HI_AD);
    printf("REF_STATE3_LO_AD = %d", REF_STATE3_LO_AD);
  
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
                LEAFLastState = 1; //Corresponds to no LEAF
                CurrentState = Standby;
                // Change EventType to light leds indicating direction to insert
            }
            else 
                puts("Error: did not receive ES_INIT event.\r\n");
            break;


        case Standby:
            if ((ThisEvent.EventType == LEAF_CHANGED) && (ThisEvent.EventParam == 2)) 
            {
                puts("Reflectivity too high, LEAF inserted incorrectly.\r\n");
                ES_Event_t Event2Post;
                //Change EventType to light leds indicating direction
                // Event2Post.EventType = PLAY_LEAF_ERROR_AUDIO;
                // PostAudioService(Event2Post);
            }
            else if ((ThisEvent.EventType == LEAF_CHANGED) && (ThisEvent.EventParam == 1)) 
            {
                puts("No leaf inserted. Please insert leaf\r\n");
                ES_Event_t Event2Post;
                // Change EventType to light leds indicating direction to insert
                
            }
            else if ((ThisEvent.EventType == LEAF_CHANGED) && (ThisEvent.EventParam == 3))  
            {
                // play welcoming audio
                ES_Event_t Event2Post;
                Event2Post.EventType = PLAY_AUDIO;
                Event2Post.EventParam = WELCOMING_TRACK;
                printf("Posting audio event, param: %d\r\n", Event2Post.EventParam);
                PostAudioService(Event2Post);
                // turn on thermometer LEDs
                SR_WriteTemperature(Temperature);
                puts("LEAF inserted correctly. Going into welcome mode.\r\n");
                CurrentState = WelcomeMode;                
            }
            break;

        case WelcomeMode:
            if ((ThisEvent.EventType == AUDIO_DONE) && 
                (ThisEvent.EventParam == WELCOMING_TRACK)) {
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
            else if ((ThisEvent.EventType == LEAF_CHANGED) && (ThisEvent.EventParam == 1)) {
                
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

            else if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam ==
                USER_INPUT_TIMER))  
            {
            /* else if (((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam ==
                USER_INPUT_TIMER))) { */
                puts("No user input detected for 30s, Resetting all games.\r\n");
                SR_WriteTemperature(0);
                ES_Event_t Event2Post;
                Event2Post.EventType = RESET_ALL_GAMES;
                // post event to distribution list
                // ES_PostList00(Event2Post);
                // Change EventType to light leds indicating direction to take out
                CurrentState = Standby;
            }

            else if ((ThisEvent.EventType == LEAF_CHANGED) && (ThisEvent.EventParam == 1))
            {
                puts("User removed leaf, Resetting all games.\r\n");
                SR_WriteTemperature(0);
                ES_Event_t Event2Post;
                Event2Post.EventType = RESET_ALL_GAMES;
                // post event to distribution list
                // ES_PostList00(Event2Post);
                // Change EventType to light leds indicating direction to put in
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

//To be changed by Sander
//3 events: LEAF_CORRECT, LEAF_WRONG, LEAF_REMOVED
//4 thresholds V_lowmed_lo, V_lowmed_hi, V_medhi_lo, V_medhi_hi
bool CheckLEAFInsertion() 
{
    ES_Event_t LeafEvent;
    bool ReturnVal = false;
    uint32_t LEAFCurrentValue;
    static uint8_t LEAFCurrentState;

    LEAFCurrentValue = ReadLEAFState();                                                                                                                                            
    if(LEAFCurrentValue < REF_STATE1_HI_AD)
    {
        LEAFCurrentState = 1;
    }
    else if((LEAFCurrentValue > REF_STATE2_LO_AD) && (LEAFCurrentValue < REF_STATE2_HI_AD))
    {
        LEAFCurrentState = 2;
    }
    else if(LEAFCurrentValue > REF_STATE3_LO_AD)
    {
        LEAFCurrentState = 3;
    }
    if (LEAFCurrentState != LEAFLastState)
    {
        ReturnVal = true;
        LeafEvent.EventType = LEAF_CHANGED;
        LeafEvent.EventParam = LEAFCurrentState;
        printf("Leaf changed, state = %d", LEAFCurrentState); 
        PostGameManager(LeafEvent);
        LEAFLastState = LEAFCurrentState;
    }

    return ReturnVal;
}

/****************************************************************************
 Function
     ReadLEAFState

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
static uint32_t ReadLEAFState(void)
{
  uint32_t LEAF_Position[2];
  //Read analog input pin 
  ADC_MultiRead(LEAF_Position); 
  //printf("LEAF position: %d \r\n", LEAF_Position[1]);
  //Leaf position corresponds to PE1 (2nd ADC channel)
  return LEAF_Position[1];
}
