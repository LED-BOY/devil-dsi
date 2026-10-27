#include "dsi_audio.h"
unsigned host_sound_active, host_sound_stops;
const void *host_sound_pcm[16];
unsigned host_sound_words[16], host_sound_volume[16], host_sound_pan[16], host_sound_rate[16];
static unsigned char fixture[140000];
static unsigned fixture_opens;
static void put16(unsigned off, unsigned value) { fixture[off] = value; fixture[off+1] = value >> 8; }
static void put32(unsigned off, unsigned value) { put16(off,value); put16(off+2,value>>16); }
SDL_RWops *DsiOpenAudioAsset(const char *path)
{
    ++fixture_opens;
    if (strcmp(path,"missing") == 0) return NULL;
    if (strcmp(path,"oversize") == 0) return SDL_RWFromMem(fixture,sizeof(fixture));
    memset(fixture,0,sizeof(fixture));
    memcpy(fixture,"RIFF",4); memcpy(fixture+8,"WAVEfmt ",8);
    unsigned stereo = strcmp(path,"stereo") == 0;
    unsigned ima = strcmp(path,"ima") == 0;
    unsigned frames = strncmp(path,"large",5) == 0 ? 32768 : 16;
    unsigned data_offset = ima ? 60 : 44;
    unsigned bytes = ima ? 8 : frames * (stereo ? 4 : 1);
    put32(4,data_offset+bytes-8); put32(16,ima?20:16);
    put16(20,ima?17:1); put16(22,stereo?2:1);
    put32(24,stereo?22050:11025); put32(28,stereo?88200:11025);
    put16(32,ima?8:(stereo?4:1)); put16(34,ima?4:(stereo?16:8));
    if (ima) {
        put16(36,2); put16(38,9);
        memcpy(fixture+40,"fact",4); put32(44,4); put32(48,9);
        put16(60,1000); /* zero nibbles preserve the predictor at step index 0 */
    }
    memcpy(fixture+data_offset-8,"data",4); put32(data_offset-4,bytes);
    if (!ima) for (unsigned i=0;i<frames;++i) {
        if (stereo) { put16(data_offset+i*4,2000); put16(data_offset+i*4+2,(unsigned)-1000); }
        else fixture[data_offset+i] = (unsigned char)i;
    }
    return SDL_RWFromMem(fixture,data_offset+bytes);
}
static int CheckAudio(void)
{
    static int owners[18];
    DsiAudioInit();
    if (!DsiAudioPlay(&owners[0],"mono",0,0,0)) return 0;
    const unsigned char *mono = host_sound_pcm[0];
    if (mono[0] != 0 || mono[1] != 128 || host_sound_words[0] != 8
        || host_sound_rate[0] != 11025 || host_sound_volume[0] != 2047 || host_sound_pan[0] != 64
        || !DsiAudioIsPlaying(&owners[0])) return 0;
    unsigned opens = fixture_opens;
    if (!DsiAudioPlay(&owners[1],"mono",-2000,6400,0) || fixture_opens != opens
        || host_sound_pan[1] != 127 || host_sound_volume[1] > 205) return 0;
    DsiAudioStop(&owners[0]);
    if (DsiAudioIsPlaying(&owners[0]) || !DsiAudioIsPlaying(&owners[1])) return 0;
    DsiAudioStop(&owners[1]);
    if (!DsiAudioPlay(&owners[0],"stereo",0,-6400,0)) return 0;
    const unsigned char *stereo = host_sound_pcm[0];
    if (stereo[0] != 244 || stereo[1] != 1 || host_sound_pan[0] != 0
        || host_sound_rate[0] != 22050) return 0;
    DsiAudioStop(&owners[0]);
    if (!DsiAudioPlay(&owners[0],"ima",0,0,0)) { puts(SDL_GetError()); return 0; }
    const unsigned char *ima = host_sound_pcm[0];
    if (ima[0] != 232 || ima[1] != 3) return 0;
    DsiAudioStop(&owners[0]);
    if (DsiAudioPlay(&owners[0],"mono",0,0,-1600)
        || DsiAudioPlay(&owners[0],"missing",0,0,0)
        || DsiAudioPlay(&owners[0],"oversize",0,0,0)) return 0;
    for (int i=0;i<16;++i)
        if (!DsiAudioPlay(&owners[i],"mono",0,0,0)) return 0;
    if (host_sound_active != 0xffff || DsiAudioPlay(&owners[16],"mono",0,0,0)) return 0;
    host_sound_active = 0; /* ARM7 has finished the samples. */
    if (DsiAudioIsPlaying(&owners[0])) return 0;
    DsiAudioQuit(); DsiAudioInit();
    for (int i=0;i<16;++i) {
        char name[32]; sprintf(name,"large%d",i);
        if (!DsiAudioPlay(&owners[i],name,0,0,0)) return 0;
    }
    if (DsiAudioCacheBytes() != 1024*1024 || DsiAudioPlay(&owners[16],"stereo",0,0,0)) return 0;
    /* Active voices pin their PCM, even when a new sample cannot fit. */
    if (((const unsigned char *)host_sound_pcm[0])[1] != 128 || host_sound_active != 0xffff) return 0;
    for (int i=0;i<16;++i) DsiAudioStop(&owners[i]);
    for (int i=16;i<80;++i) {
        char name[32]; sprintf(name,"large%d",i);
        if (!DsiAudioPlay(&owners[0],name,0,0,0) || DsiAudioCacheBytes() > 1024*1024) return 0;
        DsiAudioStop(&owners[0]);
    }
    DsiAudioSetEnabled(0);
    if (host_sound_active || DsiAudioCacheBytes() || DsiAudioIsPlaying(NULL)
        || DsiAudioPlay(&owners[0],"mono",0,0,0)) return 0;
    DsiAudioSetEnabled(1);
    if (!DsiAudioPlay(&owners[0],"mono",0,0,0)) return 0;
    DsiAudioQuit();
    puts("PASS: hardware SFX path: SDL PCM8/stereo16/IMA decode, samples/gain/pan, cache reuse/eviction/pinning, voice limit, mute/missing/oversize and shutdown");
    return 1;
}
