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

#define SR_PORT_BASE GPIO_PORTA_BASE
#define DATA BIT3HI
#define RCLK BIT4HI
#define SCLK BIT5HI

// #define TRACK0 BIT4LO
// #define TRACK1 BIT3LO
// #define TRACK2 BIT3LO
// #define TRACK3 BIT4LO
// #define TRACK4 BIT5LO

// port F
#define ACTIVITY_PIN BIT3LO
#define RST BIT2LO
#define LOOP_TRACK BIT1LO
#define NO_TRACKS 20

#define DEBOUNCE_TIME 300

static void AudioSRWriteLO(uint8_t Track);
static void AudioSRWriteHI(uint8_t Track);

static uint8_t MyPriority;
// static const uint8_t TrackPorts[] = {TRACK0, TRACK1, TRACK2, TRACK3, TRACK4};
// static const uint8_t ALL_TRACKS = ~(TRACK0 & TRACK1 & TRACK2 & TRACK3 & TRACK4); // all HI
static AudioState CurrentState = InitAudioState;
static uint8_t CurrentTrack = 0;
static uint8_t ActivityPinLastState;

bool InitAudioService(uint8_t Priority) {
    MyPriority = Priority;
    // enable all digital pins and set as output
    // HWREG(PORT_BASE + GPIO_O_DEN) |= ALL_TRACKS;
    // HWREG(PORT_BASE + GPIO_O_DIR) |= ALL_TRACKS;

    // enable digital pins and set as output
    HWREG(GPIO_PORTB_BASE + GPIO_O_DEN) |= (DATA | RCLK | SCLK);
    HWREG(GPIO_PORTB_BASE + GPIO_O_DIR) |= (DATA | RCLK | SCLK);
    HWREG(GPIO_PORTF_BASE + GPIO_O_DEN) |= ~(RST & LOOP_TRACK);
    HWREG(GPIO_PORTF_BASE + GPIO_O_DIR) |= ~(RST & LOOP_TRACK);
    HWREG(GPIO_PORTF_BASE + GPIO_O_DEN) |= ~ACTIVITY_PIN;
    ActivityPinLastState = HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) & (~ACTIVITY_PIN);
    
    // start with the data & sclk lines low and the RCLK line high
    HWREG(GPIO_PORTB_BASE + GPIO_O_DATA + ALL_BITS) = (DATA & SCLK) | RCLK;

    // // all tracks initially in OFF state
    AudioSRWriteLO(NO_TRACKS);
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
                AudioSRWriteLO(CurrentTrack);
                // HWREG(PORT_BASE + GPIO_O_DATA + ALL_BITS) &= TrackPorts[CurrentTrack];

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
                // HWREG(PORT_BASE + GPIO_O_DATA + ALL_BITS) |= (~TrackPorts[CurrentTrack]);
                printf("debounce timer expired, pulling track %d hi\r\n", CurrentTrack);
                AudioSRWriteHI(CurrentTrack);
            }
            else if (ThisEvent.EventType == AUDIO_DONE) {
                puts("Track finished playing.\r\n");
                CurrentTrack = 0;
                CurrentState = NoAudio;
            }
            else if (ThisEvent.EventType == STOP_AUDIO) {
                // pulse reset line
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) &= RST;
                ES_Timer_InitTimer(AUDIO_DEBOUNCE_TIMER, DEBOUNCE_TIME);
                CurrentState = Resetting;
            }
            break;

        case Resetting:
            if ((ThisEvent.EventType == ES_TIMEOUT) && (ThisEvent.EventParam
                == AUDIO_DEBOUNCE_TIMER)) {
                HWREG(GPIO_PORTF_BASE + GPIO_O_DATA + ALL_BITS) |= (~RST);
                // HWREG(PORT_BASE + GPIO_O_DATA + ALL_BITS) |= ALL_TRACKS;
                AudioSRWriteLO(NO_TRACKS);
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
    //if (!ActivityPinCurrState) 
      //puts("act curr state lo");
    if (ActivityPinCurrState != ActivityPinLastState) {
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

static bool AudioSRWriteLO(uint8_t Track) {
    HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) &= ~RCLK;
    for (int i=0; i < 16; i++) {
        if (i == Track) {
            // set bit corresponding to Track LO
            HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) &= ~DATA;
        }
        else {
            HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) |= DATA;
        }
        // pulse shift clock
        HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) |= SCLK;
        HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) &= ~SCLK;
    }
    HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) |= RCLK;
}

static bool AudioSRWriteHI(uint8_t Track) {
    HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) &= ~RCLK;
    for (int i=0; i < 16; i++) {
        if (i == Track) {
            // set bit corresponding to Track HI
            HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) |= DATA;
        }
        else {
            HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) &= ~DATA;
        }
        // pulse shift clock
        HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) |= SCLK;
        HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) &= ~SCLK;
    }
    HWREG(SR_PORT_BASE + GPIO_O_DATA + ALL_BITS) |= RCLK;

}
