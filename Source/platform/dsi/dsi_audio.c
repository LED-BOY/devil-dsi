#include <nds.h>
#include <SDL.h>
#include <math.h>
#include <string.h>
#include "dsi_audio.h"
#include "dsi_trace.h"

enum { CLIPS = 32, VOICES = 16, CACHE_LIMIT = 1024 * 1024,
    FILE_LIMIT = 128 * 1024, SAMPLE_LIMIT = 256 * 1024 };
typedef struct {
    char path[256];
    Uint8 *pcm;
    unsigned bytes, rate, duration, age;
} Clip;
typedef struct { const void *owner; Clip *clip; } Voice;
static Clip clips[CLIPS];
static Voice voices[VOICES];
static unsigned cache_bytes, age;
static int ready;
static unsigned logged_plays, logged_skips;

static void collect_finished(void)
{
    if (!ready) return;
    unsigned active = soundGetActiveChannels();
    for (int i = 0; i < VOICES; ++i)
        if (!(active & (1u << i))) { voices[i].owner = NULL; voices[i].clip = NULL; }
}

static int in_use(Clip *clip)
{
    for (int i = 0; i < VOICES; ++i) if (voices[i].clip == clip) return 1;
    return 0;
}

static void evict(Clip *clip)
{
    cache_bytes -= clip->bytes;
    SDL_free(clip->pcm);
    SDL_memset(clip, 0, sizeof(*clip));
}

static Clip *oldest_unused(void)
{
    Clip *oldest = NULL;
    for (int i = 0; i < CLIPS; ++i)
        if (clips[i].pcm && !in_use(&clips[i]) && (!oldest || clips[i].age < oldest->age)) oldest = &clips[i];
    return oldest;
}

static Clip *load_clip(const char *path)
{
    if (!ready || !path || SDL_strlen(path) >= sizeof(clips[0].path)) return NULL;
    collect_finished();
    for (int i = 0; i < CLIPS; ++i)
        if (clips[i].pcm && SDL_strcmp(clips[i].path, path) == 0) { clips[i].age = ++age; return &clips[i]; }
    SDL_RWops *file = DsiOpenAudioAsset(path);
    if (!file) return NULL;
    int size = SDL_RWseek(file, 0, RW_SEEK_END);
    if (size <= 0 || size > FILE_LIMIT || SDL_RWseek(file, 0, RW_SEEK_SET) < 0) { SDL_RWclose(file); return NULL; }
    SDL_AudioSpec spec;
    Uint8 *wave = NULL;
    Uint32 length = 0;
    if (!SDL_LoadWAV_RW(file, 1, &spec, &wave, &length)) return NULL;
    int bits = spec.format & 0xff;
    if ((spec.channels != 1 && spec.channels != 2) || spec.freq < 8000 || spec.freq > 48000
        || (spec.format != AUDIO_U8 && spec.format != AUDIO_S8
            && spec.format != AUDIO_S16LSB && spec.format != AUDIO_S16MSB)) { SDL_FreeWAV(wave); return NULL; }
    unsigned stride = spec.channels * (bits / 8);
    unsigned frames = length / stride;
    unsigned bytes = (frames * 2 + 3) & ~3u;
    if (!frames || bytes > SAMPLE_LIMIT || length % stride) { SDL_FreeWAV(wave); return NULL; }
    Clip *slot = NULL;
    for (int i = 0; i < CLIPS; ++i) if (!clips[i].pcm) { slot = &clips[i]; break; }
    while (!slot || cache_bytes + bytes > CACHE_LIMIT) {
        Clip *oldest = oldest_unused();
        if (!oldest) { SDL_FreeWAV(wave); return NULL; }
        evict(oldest);
        if (!slot) slot = oldest;
    }
    Uint8 *pcm = SDL_calloc(1, bytes);
    if (!pcm) { SDL_FreeWAV(wave); return NULL; }
    for (unsigned i = 0; i < frames; ++i) {
        int total = 0;
        for (unsigned c = 0; c < spec.channels; ++c) {
            unsigned offset = i * stride + c * (bits / 8);
            if (bits == 8) total += ((int)wave[offset] - (spec.format == AUDIO_U8 ? 128 : (wave[offset] >= 128 ? 256 : 0))) * 256;
            else {
                unsigned value = spec.format == AUDIO_S16LSB
                    ? wave[offset] | (unsigned)wave[offset + 1] << 8
                    : (unsigned)wave[offset] << 8 | wave[offset + 1];
                total += value >= 32768 ? (int)value - 65536 : (int)value;
            }
        }
        unsigned value = (unsigned)(total / spec.channels);
        pcm[i * 2] = (Uint8)value;
        pcm[i * 2 + 1] = (Uint8)(value >> 8);
    }
    SDL_FreeWAV(wave);
    SDL_strlcpy(slot->path, path, sizeof(slot->path));
    slot->pcm = pcm; slot->bytes = bytes; slot->rate = spec.freq;
    slot->duration = (unsigned)((Uint64)frames * 1000 / spec.freq);
    slot->age = ++age;
    cache_bytes += bytes;
    DC_FlushRange(pcm, bytes);
    return slot;
}

void DsiAudioInit(void)
{
    if (ready) return;
    soundInit(); soundPowerOn(); soundSetMixerVolume(127);
    soundSetMixerConfig(SoundOutSrc_Mixer, SoundOutSrc_Mixer, 0, 0);
    soundSetAutoUpdate(1);
    ready = 1;
}

void DsiAudioStop(const void *owner)
{
    if (!ready) return;
    unsigned mask = 0;
    for (int i = 0; i < VOICES; ++i) if (voices[i].owner == owner) mask |= 1u << i;
    if (mask) { soundStop(mask); soundSynchronize(); }
    for (int i = 0; i < VOICES; ++i) if (mask & (1u << i)) { voices[i].owner = NULL; voices[i].clip = NULL; }
}

int DsiAudioPlay(const void *owner, const char *path, int attenuation, int pan, int user_volume)
{
    if (!ready || !owner || user_volume <= -1600) return 0;
    Clip *clip = load_clip(path);
    if (!clip) {
        if (logged_skips++ < 4) DsiTrace("SFX unavailable/too large/cache busy: %s", path);
        return 0;
    }
    int channel;
    for (channel = 0; channel < VOICES; ++channel) if (!voices[channel].owner) break;
    if (channel == VOICES) return 0; /* Drop excess effects; never steal a live buffer. */
    int gain = attenuation + user_volume * 4;
    if (gain < -6400) gain = -6400;
    if (gain > 0) gain = 0;
    unsigned volume = (unsigned)(2047.0 * pow(10.0, gain / 2000.0));
    if (pan < -6400) pan = -6400;
    if (pan > 6400) pan = 6400;
    unsigned position = (unsigned)((pan + 6400) * 128 / 12800);
    if (position > 127) position = 127;
    voices[channel].owner = owner; voices[channel].clip = clip;
    soundPreparePcm(channel | SOUND_START, volume, position, soundTimerFromHz(clip->rate),
        SoundMode_OneShot, SoundFmt_Pcm16, clip->pcm, 0, clip->bytes / 4);
    soundSynchronize(); /* ARM7 must see the buffer before it can be retired. */
    if (logged_plays++ < 4) DsiTrace("SFX %s ch=%d rate=%u bytes=%u cache=%u", path, channel, clip->rate, clip->bytes, cache_bytes);
    return 1;
}

int DsiAudioIsPlaying(const void *owner)
{
    if (!owner) return 0;
    collect_finished();
    for (int i = 0; i < VOICES; ++i) if (voices[i].owner == owner) return 1;
    return 0;
}
int DsiAudioLength(const char *path) { Clip *clip = load_clip(path); return clip ? (int)clip->duration : 0; }
unsigned DsiAudioCacheBytes(void) { return cache_bytes; }
void DsiAudioQuit(void)
{
    if (!ready) return;
    soundStop(0xffff); soundSynchronize();
    SDL_memset(voices, 0, sizeof(voices));
    for (int i = 0; i < CLIPS; ++i) if (clips[i].pcm) evict(&clips[i]);
    soundSetAutoUpdate(0); ready = 0;
}
void DsiAudioSetEnabled(int enabled) { if (enabled) DsiAudioInit(); else DsiAudioQuit(); }
