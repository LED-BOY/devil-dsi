#pragma once
#include <SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
/* All calls run on the engine thread. ARM7 performs sample playback. */
void DsiAudioInit(void);
void DsiAudioSetEnabled(int enabled);
void DsiAudioQuit(void);
int DsiAudioPlay(const void *owner, const char *path, int attenuation, int pan, int user_volume);
void DsiAudioStop(const void *owner);
int DsiAudioIsPlaying(const void *owner);
int DsiAudioLength(const char *path);
unsigned DsiAudioCacheBytes(void);
/* Engine adapter opens MPQ-backed WAV files; the decoder closes the result. */
SDL_RWops *DsiOpenAudioAsset(const char *path);
#ifdef __cplusplus
}
#endif
