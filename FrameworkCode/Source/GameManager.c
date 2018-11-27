/****************************************************************************************
*   GameManager.c
*   
*   to do's: modify LEAF event checker to be analog; game over event; change temp event
*
****************************************************************************************/
// PORT D
#define MAX_TEMP 8

#include "GameManager.h"
#include "ES_Framework.h"
#include "ADMulti.h"

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
#include "AudioService.h"

#define ONE_SEC 1000

#define TEN_SEC (ONE_SEC*10)
#define ONE_MINUTE (ONE_SEC*60)
#define HALF_MINUTE (ONE_SEC*30)

#define AD_VOLTAGE(x) (int)(x*4095/3.3)

#define REF_STATE1_HI 1.0
#define REF_STATE2_LO 1.8
#define REF_STATE2_HI 2.3
#define REF_STATE3_LO 2.8


#define REF_STATE1_HI_AD AD_VOLTAGE(REF_STATE1_HI) 
#define REF_STATE2_LO_AD AD_VOLTAGE(REF_STATE2_LO) 
#define REF_STATE2_HI_AD AD_VOLTAGE(REF_STATE2_HI) 
#define REF_STATE3_LO_AD AD_VOLTAGE(REF_STATE3_LO)

#define INSERT 1
#define REMOVE 2
#define BLINK_TIME 1000

#define LEAF_LED_PORT_BASE GPIO_PORTC_BASE
#define LEAF_LED_TOP BIT4LO
#define LEAF_LED_MID BIT5LO
#define LEAF_LED_BOT BIT6LO


/****************************** Private Functions & Variables **************************/
static uint8_t LEAFLastState;
static uint8_t MyPriority;
static GameManagerState CurrentState = InitGState;

static uint32_t ReadLEAFState(void);
static void BlinkNextLED(void);
static uint8_t BlinkLEAFLights = 0;

bool InitGameManager(uint8_t Priority) {
    // LEAF IR detector port initialized in ADCMultiInit
    HWREG(LEAF_LED_PORT_BASE + GPIO_O_DEN) |= ~(LEAF_LED_TOP & LEAF_LED_MID & LEAF_LED_BOT);
    HWREG(LEAF_LED_PORT_BASE + GPIO_O_DIR) |= ~(LEAF_LED_TOP & LEAF_LED_MID & LEAF_LED_BOT);
    SR_Init();
    MyPriority = Priority;
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

    static int8_t Temperature = MAX_TEMP - 1;
    static uint8_t NumOfActiveGames = 0;
    
    ES_Event_t ReturnEvent;
    ReturnEvent.EventType = ES_NO_EVENT;

    switch (CurrentState) {
        case InitGState:
            if (ThisEvent.EventType == ES_INIT) {
                puts("GameManager in standby mode.\r\n");
                LEAFLastState = 1; //Corresponds to no LEAF
                CurrentState = Standby;
                // light leds indicating direction to insert
                BlinkLEAFLights = INSERT;
                BlinkNextLED();
                ES_Timer_InitTimer(BLINK_TIMER, BLINK_TIME);
            }
            else 
                puts("Error: did not receive ES_INIT event.\r\n");
            break;


        case Standby:
            if ((ThisEvent.EventType == LEAF_CHANGED) && (ThisEvent.EventParam == 2)) 
            {
                puts("Reflectivity too high, LEAF inserted incorrectly.\r\n");
                BlinkLEAFLights = REMOVE;
                BlinkNextLED();
                ES_Timer_InitTimer(BLINK_TIMER, BLINK_TIME);
            }
            
            else if ((ThisEvent.EventType == LEAF_CHANGED) && (ThisEvent.EventParam == 1)) 
            {
                puts("No leaf inserted. Please insert leaf\r\n");
                BlinkLEAFLights = INSERT;
                BlinkNextLED();
                ES_Timer_InitTimer(BLINK_TIMER, BLINK_TIME);
            }
                
            
            else if ((ThisEvent.EventType == LEAF_CHANGED) && (ThisEvent.EventParam == 3))  
            {
                BlinkLEAFLights = 0;
                BlinkNextLED();
                // play welcoming audio
                ES_Event_t Event2Post;
                Event2Post.EventType = PLAY_AUDIO;
                Event2Post.EventParam = WELCOMING_TRACK;
                printf("Posting audio event, param: %d\r\n", Event2Post.EventParam);
                PostAudioService(Event2Post);
                // turn on thermometer LEDs (starting at 7)
                SR_WriteTemperature(Temperature);
                puts("LEAF inserted correctly. Going into welcome mode.\r\n");
                CurrentState = WelcomeMode;                
            }
            else if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == BLINK_TIMER) 
                && BlinkLEAFLights) {
                BlinkNextLED();
                ES_Timer_InitTimer(BLINK_TIMER, BLINK_TIME);
            }
            break;

        case WelcomeMode:
            if ((ThisEvent.EventType == AUDIO_DONE) && 
                (ThisEvent.EventParam == WELCOMING_TRACK)) {
                ES_Event_t Event2Post;
                Event2Post.EventType = START_GAME;
                Event2Post.EventParam = 1;
                PostEnergyProduction(Event2Post);
                // play audio instructions for game 1?
                NumOfActiveGames ++;
                puts("Starting first game.\r\n");

                // start timers
                ES_Timer_InitTimer(NEXT_GAME_TIMER, TEN_SEC);
                ES_Timer_InitTimer(USER_INPUT_TIMER, HALF_MINUTE);
                ES_Timer_InitTimer(GAME_END_TIMER, ONE_MINUTE);
                CurrentState = GameActive;
            }
            else if ((ThisEvent.EventType == LEAF_CHANGED) && (ThisEvent.EventParam == 1)) {
                // turn all lights off
                SR_Write(0);
                ES_Event_t Event2Post;
                Event2Post.EventType = STOP_AUDIO;
                PostAudioService(Event2Post);

                BlinkLEAFLights = INSERT;
                BlinkNextLED();
                ES_Timer_InitTimer(BLINK_TIMER, BLINK_TIME);

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
                        ES_Timer_InitTimer(NEXT_GAME_TIMER, TEN_SEC);
                        Event2Post.EventParam = 2;
                        PostMeatSwitchDebounce(Event2Post);
                        // play audio instructions for game 2?
                        puts("10s timer expired: starting second game.\r\n");
                    }
                    else if (NumOfActiveGames == 3) {
                        Event2Post.EventParam = 3;
                        PostVotingGame(Event2Post);
                        // play audio instructions for game 3?
                        puts("10s timer expired: starting third game.\r\n");
                    }
                }
            }

            else if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam ==
                USER_INPUT_TIMER)) {
                puts("No user input detected for 30s; Resetting all games.\r\n");
                SR_Write(0);
                ES_Event_t Event2Post;
                Event2Post.EventType = RESET_ALL_GAMES;
                // post event to distribution list
                ES_PostList00(Event2Post);
                
                BlinkLEAFLights = REMOVE;
                BlinkNextLED();
                ES_Timer_InitTimer(BLINK_TIMER, BLINK_TIME);
                CurrentState = Standby;
            }

            else if ((ThisEvent.EventType == LEAF_CHANGED) && (ThisEvent.EventParam == 1))
            {
                puts("User removed leaf, Resetting all games.\r\n");
                SR_Write(0);// SR_WriteTemperature(0);
                ES_Event_t Event2Post;
                Event2Post.EventType = RESET_ALL_GAMES;
                // post event to distribution list
                ES_PostList00(Event2Post);
                BlinkLEAFLights = INSERT;
                BlinkNextLED();
                ES_Timer_InitTimer(BLINK_TIMER, BLINK_TIME);
                CurrentState = Standby;
            }

            else if (ThisEvent.EventType == USERMVT_DETECTED) {
                ES_Timer_InitTimer(USER_INPUT_TIMER, HALF_MINUTE);
                puts("Resetting 30s timer.\r\n");
            }

            else if (ThisEvent.EventType == CHANGE_TEMP) {
                Temperature += 2 * ThisEvent.EventParam - 1;
                if (Temperature > MAX_TEMP)
                    Temperature = MAX_TEMP;
                else if (Temperature < 0)
                    Temperature = 0;            
                SR_WriteTemperature(Temperature);
            }

            else if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == GAME_END_TIMER)) {
                puts("Game over.\r\n");
                ES_Event_t Event2Post;
                Event2Post.EventType = GAME_OVER;
                ES_PostList00(Event2Post);
                Event2Post.EventType = PLAY_CLOSING_AUDIO;
                Event2Post.EventParam = Temperature;
                PostAudioService(Event2Post);
                // stamp LEAF TBD
                CurrentState = GameOver;
            }
            break;

        case GameOver:
            if ((ThisEvent.EventType == AUDIO_DONE) && (ThisEvent.EventParam == CLOSING_TRACK)) {
                puts("Closing track done. \r\n");
                // light up LEDs to indicate user to remove LEAF
                BlinkLEAFLights = REMOVE;
                BlinkNextLED();
                ES_Timer_InitTimer(BLINK_TIMER, BLINK_TIME);            
            }
            else if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam == BLINK_TIMER) 
                && BlinkLEAFLights) {
                BlinkNextLED();
                ES_Timer_InitTimer(BLINK_TIMER, BLINK_TIME);
            }
            else if ((ThisEvent.EventType == LEAF_CHANGED) && (ThisEvent.EventParam == 1)) {
                puts("LEAF removed. Resetting all games and going back to standby.");
                SR_Write(0);
                ES_Event_t Event2Post;
                Event2Post.EventType = RESET_ALL_GAMES;
                ES_PostList00(Event2Post);

                BlinkLEAFLights = INSERT;
                BlinkNextLED();
                ES_Timer_InitTimer(BLINK_TIMER, BLINK_TIME);

                CurrentState = Standby;
            }

        default:
            puts("Error: GameManager entered unknown state.\r\n");
    }
    return ReturnEvent;
}

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
static uint32_t ReadLEAFState()
{
  uint32_t LEAF_Position[2];
  //Read analog input pin 
  ADC_MultiRead(LEAF_Position); 
  //printf("LEAF position: %d \r\n", LEAF_Position[1]);
  //Leaf position corresponds to PE1 (2nd ADC channel)
  return LEAF_Position[1];
}

static void BlinkNextLED() {
    static uint8_t i = 0;
    if (!BlinkLEAFLights) {
        // turn off all LEDs
        HWREG(LEAF_LED_PORT_BASE + GPIO_O_DATA + ALL_BITS) |= ~(LEAF_LED_BOT & LEAF_LED_MID & LEAF_LED_TOP);
        i = 0;
        return;
    }
    puts("Blinking next light.\r\n");
    uint8_t LEDSequence[3] = {LEAF_LED_TOP, LEAF_LED_MID, LEAF_LED_BOT};
    if (BlinkLEAFLights == REMOVE) {
        LEDSequence[0] = LEAF_LED_BOT;
        LEDSequence[2] = LEAF_LED_TOP;
    }
    if (i == 3) {
        // turn off all LEDs
        HWREG(LEAF_LED_PORT_BASE + GPIO_O_DATA + ALL_BITS) |= ~(LEAF_LED_BOT & LEAF_LED_MID & LEAF_LED_TOP);
        i = 0;
    }
    else {
        HWREG(LEAF_LED_PORT_BASE + GPIO_O_DATA + ALL_BITS) &= LEDSequence[i];
        i++;
    }
}
