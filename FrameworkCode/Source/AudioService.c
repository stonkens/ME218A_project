/****************************************************************************************
*   AudioService.c
*   
*   
*
****************************************************************************************/

#include "AudioService.h"
#include "ES_Events.h"
#include "ES_Framework.h"
#include "ES_Configure.h"
#include "BITDEFS.H"

#include "GameManager.h"

// the headers to access the GPIO subsystem
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_sysctl.h"

#define PORT_BASE GPIO_PORTE_BASE
#define TRACK0 BIT1LO
#define TRACK1 BIT2LO
#define TRACK2 BIT3LO
#define TRACK3 BIT4LO
#define TRACK4 BIT5LO

// port F
#define ACTIVITY_PIN BIT2LO
#define LOOP_TRACK BIT4LO
#define RST BIT3LO

#define DEBOUNCE_TIME 500


static uint8_t MyPriority;
static const uint8_t TrackPorts[] = {TRACK0, TRACK1, TRACK2, TRACK3, TRACK4};
static const uint8_t ALL_TRACKS = ~(TRACK0 & TRACK1 & TRACK2 & TRACK3 & TRACK4); // all HI
static AudioState CurrentState = InitAudioState;
static uint8_t CurrentTrack = 0;
static uint8_t ActivityPinLastState = 1;

bool InitAudioService(uint8_t Priority) {
    MyPriority = Priority;
    // enable all digital pins and set as output
    HWREG(PORT_BASE + GPIO_O_DEN) |= ALL_TRACKS;
    HWREG(PORT_BASE + GPIO_O_DIR) |= ALL_TRACKS;
    HWREG(GPIO_PORTF_BASE + GPIO_O_DEN) |= ~(RST & LOOP_TRACK);
    HWREG(GPIO_PORTF_BASE + GPIO_O_DIR) |= ~(RST & LOOP_TRACK);
    HWREG(GPIO_PORTF_BASE + GPIO_O_DEN) |= ACTIVITY_PIN;

    // all tracks initially in OFF state
    HWREG(PORT_BASE + GPIO_O_DATA + ALL_BITS) |= ALL_TRACKS;
    HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) |= ~(RST & LOOP_TRACK);    

    ES_Event_t InitEvent;
    InitEvent.EventType = ES_INIT;
    return ES_PostToService(MyPriority, InitEvent);
}

bool PostAudioService(ES_Event_t ThisEvent) {
    return ES_PostToService(MyPriority, ThisEvent);
}

ES_Event_t RunAudioService(ES_Event_t ThisEvent) {
    ES_Event_t ReturnEvent;
    ReturnEvent.EventType = ES_NO_EVENT;
    switch (CurrentState) {
        case InitAudioState:
            if (ThisEvent.EventType == ES_INIT)
                CurrentState = NoAudio;
            break;

        case NoAudio:
            puts("waiting for audio instructions\r\n");
            if (ThisEvent.EventType == PLAY_AUDIO) {
                CurrentTrack = ThisEvent.EventParam;
                HWREG(PORT_BASE + GPIO_O_DATA + ALL_BITS) &= TrackPorts[CurrentTrack];
                // audio board has built-in debounce so trigger has to be pulled 
                // LO for a certain amount of time
                printf("Playing audio track %d\r\n", CurrentTrack);
                ES_Timer_InitTimer(AUDIO_DEBOUNCE_TIMER, DEBOUNCE_TIME);
                CurrentState = PlayingAudio;
            }
            else if (ThisEvent.EventType == PLAY_LOOP) {
                HWREG(PORT_BASE + GPIO_O_DATA + ALL_BITS) &= LOOP_TRACK;
                CurrentState = PlayingLoop;
                CurrentTrack = LOOP_TRACK;
            }
            break;

        case PlayingAudio:
            if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam 
                == AUDIO_DEBOUNCE_TIMER)) {
                HWREG(PORT_BASE + GPIO_O_DATA + ALL_BITS) |= (~TrackPorts[CurrentTrack]);
            }
            else if (ThisEvent.EventType == AUDIO_DONE) {
                puts("Track finished playing.\r\n");
                CurrentTrack = 0;
                CurrentState = NoAudio;
            }
            else if (ThisEvent.EventType == STOP_AUDIO) {
                // pulse reset line
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_TRACKS) &= RST;
                ES_Timer_InitTimer(AUDIO_DEBOUNCE_TIMER, DEBOUNCE_TIME);
                CurrentState = Resetting;
            }
            break;

        case Resetting:
            if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam
                == AUDIO_DEBOUNCE_TIMER)) {
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_TRACKS) |= (~RST);
                HWREG(PORT_BASE + GPIO_O_DATA + ALL_BITS) |= ALL_TRACKS;
                puts("Reset audio board.\r\n");
                CurrentState = NoAudio;
                CurrentTrack = 0;
            }
            break;

        case PlayingLoop:
            if (ThisEvent.EventType == STOP_LOOP) {
                HWREG(PORT_BASE + GPIO_O_DATA + ALL_BITS) |= ~LOOP_TRACK;
                CurrentState = NoAudio;
                CurrentTrack = 0;
            }
            break;
    }
    return ReturnEvent;
}

bool CheckAudioStatus() {
    bool ReturnValue = false;
    uint8_t ActivityPinCurrState = HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) & ~ACTIVITY_PIN;
    // printf("acitivity pin curr state: %d\r\n", ActivityPinCurrState);
    if (ActivityPinCurrState != ActivityPinLastState) {
        puts("act pin changed.\r\n");
        if (ActivityPinCurrState) {
            // activity pin HI means track is done playing
            ES_Event_t Event2Post;
            Event2Post.EventType = AUDIO_DONE;
            Event2Post.EventParam = CurrentTrack;
            PostAudioService(Event2Post);
            PostGameManager(Event2Post);
            ReturnValue = true;
            puts("posting AUDIO DONE event.\r\n");
        }
        ActivityPinLastState = ActivityPinCurrState;
    }
    return ReturnValue;
}

