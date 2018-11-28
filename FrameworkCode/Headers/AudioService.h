/****************************************************************************************
*   AudioService.h
*   
*   
*
****************************************************************************************/

#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#define WELCOMING_TRACK 0
#define GAME2_INSTRUCTIONS 1
#define GAME3_INSTRUCTIONS 2
#define VOTED_RIGHT 3
#define VOTED_WRONG 4
#define FINAL_TEMP_4 5 
#define FINAL_TEMP_3 6
#define FINAL_TEMP_2 7
#define FINAL_TEMP_1 8

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

