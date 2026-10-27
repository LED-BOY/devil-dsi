#pragma once
#include <stdint.h>
typedef uint64_t u64;
typedef uint32_t u32;
typedef struct { uint16_t px, py; } touchPosition;
extern volatile uint16_t host_palette[256], host_vram[32768];
extern uint64_t host_ticks;
extern u32 host_key_state, host_previous_keys, host_scanned_keys;
extern touchPosition host_touch;
extern int host_main_on_bottom;
#define BG_PALETTE host_palette
#define MODE_5_2D 0
#define VRAM_A_MAIN_BG_0x06000000 0
#define BgType_Bmp8 0
#define BgSize_B8_256x256 0
#define KEY_TOUCH (1u << 14)
static inline int isDSiMode(void) { return 1; }
static inline void videoSetMode(int mode) { }
static inline void vramSetBankA(int mode) { }
static inline int bgInit(int a, int b, int c, int d, int e) { return 0; }
static inline volatile uint16_t *bgGetGfxPtr(int bg) { return host_vram; }
static inline void lcdMainOnBottom(void) { host_main_on_bottom = 1; }
static inline void scanKeys(void) { host_previous_keys = host_scanned_keys; host_scanned_keys = host_key_state; }
static inline u32 keysDown(void) { return host_scanned_keys & ~host_previous_keys; }
static inline u32 keysUp(void) { return host_previous_keys & ~host_scanned_keys; }
static inline u32 keysHeld(void) { return host_scanned_keys; }
static inline void touchRead(touchPosition *touch) { *touch = host_touch; }
static inline int pmMainLoop(void) { return 1; }
static inline void threadSleep(u32 usec) { host_ticks += usec; }
