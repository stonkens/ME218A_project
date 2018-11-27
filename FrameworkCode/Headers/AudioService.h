/****************************************************************************************
*   AudioService.h
*   
*   
*
****************************************************************************************/

#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#define WELCOMING_TRACK 1
#define COAL_AUDIO 2
#define CLOSING_TRACK 3

#include "ES_Events.h"
#include "ES_Types.h"

typedef enum {
    InitAudioState,
    NoAudio,
    PlayingAudio,
    PlayingLoop,
    Resetting
} AudioState;

bool InitAudioService(uint8_t Priority);
bool PostAudioService(ES_Event_t Event);
ES_Event_t RunAudioService(ES_Event_t ThisEvent);

bool CheckAudioStatus(void);

#endif

