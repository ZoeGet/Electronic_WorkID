#ifndef TTS_PLAYER_H
#define TTS_PLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

void TTS_Player_Init(void);
bool TTS_Player_IsBusy(void);
bool TTS_Player_PlayStartRecord(void);
bool TTS_Player_PlayStopRecord(void);
bool TTS_Player_PlayTestTone(void);
void TTS_Player_PlayBlockingTestTone(void);
void TTS_Player_Stop(void);

#ifdef __cplusplus
}
#endif

#endif /* TTS_PLAYER_H */
