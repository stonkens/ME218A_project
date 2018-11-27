/****************************************************************************************
*   VotingGame.c
*   
* 
*
****************************************************************************************/
// PORT F
#define MOTOR_ON BIT4LO
#define MOTOR_OFF BIT4HI

#include "VotingGame.h"
#include "ES_Framework.h"
#include "GameManager.h"

// the headers to access the GPIO subsystem
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"


static uint8_t MyPriority;

static VotingGameState CurrentState = InitVState;
static int8_t QuestionList[6] = {1, 2, 3, 4, 5, 6};
static int8_t QuestionYes[6] = {1, -1, 1, 1, 1, -1};
static int8_t QuestionNo[6] = {-1, 1, -1, -1, -1, 1};


bool InitVotingGame(uint8_t Priority) {
    MyPriority = Priority;
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
                ES_Timer_InitTimer(VOTE_TIMER, 5000);
                CurrentState = Waiting4Vote;
                puts("New question is displayed. Waiting for user to vote.\r\n");
            }
            else if ((ThisEvent.EventType == VOTED_YES) || (ThisEvent.EventType == VOTED_NO)) {
                ES_Event_t Event2Post;
                Event2Post.EventType = USERMVT_DETECTED;
                PostGameManager(Event2Post);
            }
            break;

        case Waiting4Vote:
            if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam 
                == VOTE_TIMER)) {
                // to be implemented
            }
            else if (ThisEvent.EventType == VOTED_YES) {
                puts("User voted YES.\r\n");
                ES_Timer_StopTimer(VOTE_TIMER);
                ES_Event_t Event2Post;
                Event2Post.EventType = USERMVT_DETECTED;
                PostGameManager(Event2Post);

                // evaluate vote
                
                puts("Changing question.\r\n");
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) &= MOTOR_ON;
                CurrentState = ChangingQuestion;
            }
            // else if user voted NO
            else if (ThisEvent.EventType == VOTED_NO) {
                puts("User voted NO.\r\n");
                ES_Timer_StopTimer(VOTE_TIMER);
                ES_Event_t Event2Post;
                Event2Post.EventType = USERMVT_DETECTED;
                PostGameManager(Event2Post);

                // evaluate vote
                
                puts("Changing question.\r\n");
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) &= MOTOR_ON;
                CurrentState = ChangingQuestion;
            }
            break;
    }
    return ReturnEvent;
}

