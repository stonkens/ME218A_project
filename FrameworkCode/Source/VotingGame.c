/****************************************************************************************
*   VotingGame.c
*   
* 
*
****************************************************************************************/
// PORT F
#define MOTOR_ON BIT4HI
#define MOTOR_OFF BIT4LO


#include "ES_Framework.h"

static uint8_t MyPriority;

static VotingGameState CurrentState = InitPState;


bool InitVotingGame(uint8_t Priority) {
    MyPriority = Priority;
    // make sure motor is off
    HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) &= MOTOR_OFF;
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
        case InitPState:
            if (ThisEvent.EventType == ES_INIT) {
                puts("Voting game is in standby mode.\r\n");
                CurrentState = Standby;
            }
            break;

        case Standby:
            if (ThisEvent.EventType == START_GAME) && (ThisEvent.EventParam == 2)) {
                // drive motor
                puts("Starting voting game; displaying first question.\r\n");
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) |= MOTOR_ON;                 
                CurrentState = ChangingQuestion;
            }
            break;

        case ChangingQuestion:
            if (ThisEvent.EventType == SWITCH_HIT) {
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) &= MOTOR_OFF;
                ES_Timer_InitTimer(VOTE_TIMER, 5000);
                CurrentState = Waiting4Vote;
                puts("New question is displayed. Waiting for user to vote.\r\n");
            }
            break;

        case Waiting4Vote:
            if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam 
                == VOTE_TIMER) {
                // to be implemented
            }
            else if (ThisEvent.EventType == VOTED_YES) {
                puts("User voted YES.\r\n");
                ES_Timer_StopTimer(VOTE_TIMER);

                // evaluate vote
                
                puts("Changing question.\r\n");
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) |= MOTOR_ON;
                CurrentState = ChangingQuestion;
            }
            // else if user voted NO
            break;
    }
    return ReturnEvent;
}

