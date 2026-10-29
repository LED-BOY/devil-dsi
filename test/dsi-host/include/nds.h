#pragma once
#include <stdint.h>
#include <stddef.h>
typedef uint64_t u64;
typedef uint32_t u32;
typedef struct { uint16_t px, py; } touchPosition;
extern volatile uint16_t host_palette[256], host_vram[32768];
extern volatile uint16_t host_sub_palette[256], host_sub_vram[32768];
extern unsigned host_visible_backgrounds;
extern uint64_t host_ticks;
extern u32 host_key_state, host_previous_keys, host_scanned_keys;
extern touchPosition host_touch;
extern int host_main_on_bottom;
extern unsigned host_sound_active, host_sound_stops;
extern const void *host_sound_pcm[16];
extern unsigned host_sound_words[16], host_sound_volume[16], host_sound_pan[16], host_sound_rate[16];
typedef enum { SoundOutSrc_Mixer } SoundOutSrc;
typedef enum { SoundMode_OneShot } SoundMode;
typedef enum { SoundFmt_Pcm16 } SoundFmt;
#define SOUND_START (1u << 4)
static inline void soundInit(void) { }
static inline void soundPowerOn(void) { }
static inline void soundSetMixerVolume(unsigned volume) { }
static inline void soundSetMixerConfig(SoundOutSrc a, SoundOutSrc b, int c, int d) { }
static inline void soundSetAutoUpdate(int enabled) { }
static inline unsigned soundGetActiveChannels(void) { return host_sound_active; }
static inline void soundSynchronize(void) { }
static inline void soundStop(unsigned mask) { host_sound_active &= ~mask; ++host_sound_stops; }
static inline unsigned soundTimerFromHz(unsigned rate) { return rate; }
static inline void DC_FlushRange(const void *data, unsigned bytes) { }
static inline void soundPreparePcm(unsigned ch, unsigned volume, unsigned pan, unsigned timer,
    SoundMode mode, SoundFmt fmt, const void *data, unsigned loop, unsigned words) {
    ch &= 15;
    host_sound_pcm[ch] = data; host_sound_words[ch] = words;
    host_sound_volume[ch] = volume; host_sound_pan[ch] = pan; host_sound_rate[ch] = timer;
    host_sound_active |= 1u << ch;
}
#define BG_PALETTE host_palette
#define BG_PALETTE_SUB host_sub_palette
#define MODE_5_2D 0
#define VRAM_A_MAIN_BG_0x06000000 0
#define BgType_Bmp8 0
#define BgSize_B8_256x256 0
#define KEY_TOUCH (1u << 14)
#define KEY_R (1u << 8)
#define KEY_L (1u << 9)
#define KEY_LEFT (1u << 5)
#define KEY_RIGHT (1u << 4)
#define KEY_UP (1u << 6)
#define KEY_DOWN (1u << 7)
static inline int isDSiMode(void) { return 1; }
static inline void videoSetMode(int mode) { }
static inline void videoSetModeSub(int mode) { }
static inline void vramSetBankA(int mode) { }
static inline int bgInit(int a, int b, int c, int d, int e) { return 0; }
static inline int bgInitSub(int a, int b, int c, int d, int e) { return 7; }
static inline void bgShow(int bg) { host_visible_backgrounds |= 1u << bg; }
static inline void bgHide(int bg) { host_visible_backgrounds &= ~(1u << bg); }
static inline volatile uint16_t *bgGetGfxPtr(int bg) { return bg == 7 ? host_sub_vram : host_vram; }
static inline void lcdMainOnBottom(void) { host_main_on_bottom = 1; }
static inline void scanKeys(void) { host_previous_keys = host_scanned_keys; host_scanned_keys = host_key_state; }
static inline u32 keysDown(void) { return host_scanned_keys & ~host_previous_keys; }
static inline u32 keysUp(void) { return host_previous_keys & ~host_scanned_keys; }
static inline u32 keysHeld(void) { return host_scanned_keys; }
static inline void touchRead(touchPosition *touch) { *touch = host_touch; }
static inline int pmMainLoop(void) { return 1; }
static inline void threadSleep(u32 usec) { host_ticks += usec; }
