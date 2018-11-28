/****************************************************************************************
*   VotingGame.c
*   
* 
*
****************************************************************************************/
// PORT F
#define MOTOR_ON BIT4LO
#define MOTOR_OFF BIT4HI
#define ONE_SEC 1000
#define FIVE_SEC (ONE_SEC*5)

#include "VotingGame.h"
#include "ES_Framework.h"
#include "GameManager.h"
#include "ES_DeferRecall.h"

// the headers to access the GPIO subsystem
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"


static uint8_t MyPriority;
static ES_Event_t DeferralQueue[3];

static VotingGameState CurrentState = InitVState;
//static int8_t QuestionList[6] = {1, 2, 3, 4, 5, 6};
static int8_t QuestionYes[6] = {1, 0, 1, 1, 1, 0};
static int8_t QuestionNo[6] = {0, 1, 0, 0, 0, 1};
static uint8_t CurrentQuestion = 0;

bool InitVotingGame(uint8_t Priority) {
    MyPriority = Priority;
    ES_InitDeferralQueueWith(DeferralQueue, ARRAY_SIZE(DeferralQueue));

    // make sure motor is off
    HWREG(GPIO_PORTF_BASE + GPIO_O_DEN) |= MOTOR_OFF;
    HWREG(GPIO_PORTF_BASE + GPIO_O_DIR) |= MOTOR_OFF;
    HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) |= MOTOR_OFF;

    // post ES_INIT event
    ES_Event_t InitEvent;
    InitEvent.EventType = ES_INIT;
    if (ES_PostToService(MyPriority, InitEvent) == true) {
        return true;
    }
    return false;
}

bool PostVotingGame(ES_Event_t ThisEvent) {
    return ES_PostToService(MyPriority, ThisEvent);
}

ES_Event_t RunVotingGame(ES_Event_t ThisEvent) {
    ES_Event_t ReturnEvent;
    ReturnEvent.EventType = ES_NO_EVENT;

    switch(CurrentState) {
        case InitVState:
            if (ThisEvent.EventType == ES_INIT) {
                puts("Voting game is in standby mode.\r\n");
                CurrentState = VStandby;
            }
            break;

        case VStandby:
            if ((ThisEvent.EventType == START_GAME) && (ThisEvent.EventParam == 3)) {
                // drive motor
                puts("Starting voting game; changing question.\r\n");
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) &= MOTOR_ON;                 
                CurrentState = ChangingQuestion;
            }
            break;

        case ChangingQuestion:
            if (ThisEvent.EventType == SWITCH_HIT) {
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) |= MOTOR_OFF;
                ES_Timer_InitTimer(VOTE_TIMER, FIVE_SEC);
                CurrentState = Waiting4Vote;
                ES_RecallEvents(MyPriority, DeferralQueue);
                puts("New question is displayed. Waiting for user to vote.\r\n");
            }
            else if ((ThisEvent.EventType == VOTED_YES) || (ThisEvent.EventType == VOTED_NO)) {
                ES_Event_t Event2Post;
                Event2Post.EventType = USERMVT_DETECTED;
                PostGameManager(Event2Post);
            }
            else if ((ThisEvent.EventType == RESET_ALL_GAMES) || (ThisEvent.EventType == GAME_OVER)) {
                ES_DeferEvent(DeferralQueue, ThisEvent);
            }
            break;

        case Waiting4Vote:
            if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam 
                == VOTE_TIMER)) {
                puts("No vote; changing question.\r\n");
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) &= MOTOR_ON; 
                CurrentQuestion += 1;
                if (CurrentQuestion >= 6)
                {
                    CurrentQuestion = 0;
                }
                CurrentState = ChangingQuestion;
                
            }
            else if (ThisEvent.EventType == VOTED_YES) {
                puts("User voted YES.\r\n");
                ES_Timer_StopTimer(VOTE_TIMER);
                ES_Event_t Event2Post;
                Event2Post.EventType = USERMVT_DETECTED;
                PostGameManager(Event2Post);

                // evaluate vote
                ES_Event_t TemperatureEvent;
                TemperatureEvent.EventType = CHANGE_TEMP;
                TemperatureEvent.EventParam = QuestionYes[CurrentQuestion];
                PostGameManager(TemperatureEvent);
                if(QuestionYes[CurrentQuestion] == 0)
                {
                  puts("Wrong answer, increasing temperature by 1 \r \n");
                  //Post play sad audio to audioservice TBD
                }
                else
                {
                  puts("Correct answer, decreasing temperature by 1 \r \n");
                  //Post play happy audio to audioservice TBD
                }
              
                puts("Changing question.\r\n");
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) &= MOTOR_ON;
                CurrentState = ChangingQuestion;
                CurrentQuestion += 1;
                if (CurrentQuestion >= 6)
                {
                    CurrentQuestion = 0;
                }
            }
            // else if user voted NO
            else if (ThisEvent.EventType == VOTED_NO) {
                puts("User voted NO.\r\n");
                ES_Timer_StopTimer(VOTE_TIMER);
                ES_Event_t Event2Post;
                Event2Post.EventType = USERMVT_DETECTED;
                PostGameManager(Event2Post);

                // evaluate vote
                ES_Event_t TemperatureEvent;
                TemperatureEvent.EventType = CHANGE_TEMP;
                TemperatureEvent.EventParam = QuestionNo[CurrentQuestion];
                PostGameManager(TemperatureEvent);
                if(QuestionNo[CurrentQuestion] == 0)
                {
                  puts("Wrong answer, increasing temperature by 1 \r \n");
                  //Post play sad audio to audioservice
                }
                else
                {
                  puts("Correct answer, decreasing temperature by 1 \r \n");
                  //Post play happy audio to audioservice
                }
                puts("Changing question.\r\n");
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) &= MOTOR_ON;
                CurrentState = ChangingQuestion;
                CurrentQuestion += 1;
                if (CurrentQuestion >= 6)
                    CurrentQuestion = 0;
            }

            else if (ThisEvent.EventType == RESET_ALL_GAMES) { 
                CurrentState = VStandby;
                puts("Voting game going back to standby.\r\n");
            }

            break;
    }

    return ReturnEvent;
}

