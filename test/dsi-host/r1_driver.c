/* DSi SDL 1.2 video/event driver. SDL core supplies software surfaces and blits. */
#include <nds.h>
#include <calico/system/tick.h>
#include <SDL.h>
#include <SDL_sysvideo.h>
#include <SDL_pixels_c.h>
#include <SDL_events_c.h>
#include <stdio.h>
#include "dsi_video.h"

struct SDL_PrivateVideoData { void *pixels; int bg; };
static u64 start_ticks;

void SDL_StartTicks(void) { start_ticks = tickGetCount(); }
Uint32 SDL_GetTicks(void) { return (Uint32)((tickGetCount() - start_ticks) * 1000 / TICK_FREQ); }
void SDL_Delay(Uint32 ms) { threadSleep(ms > 4000000 ? 4000000000u : ms * 1000); }
int SDL_SYS_TimerInit(void) { return 0; }
void SDL_SYS_TimerQuit(void) { }
int SDL_SYS_StartTimer(void) { SDL_SetError("Asynchronous timers unavailable"); return -1; }
void SDL_SYS_StopTimer(void) { }

/* Single-thread build: mutexes have lifetime semantics, with no competing tasks. */
struct SDL_mutex { int placeholder; };
SDL_mutex *SDL_CreateMutex(void) { return SDL_calloc(1, sizeof(SDL_mutex)); }
void SDL_DestroyMutex(SDL_mutex *mutex) { SDL_free(mutex); }
int SDL_mutexP(SDL_mutex *mutex) { return mutex ? 0 : -1; }
int SDL_mutexV(SDL_mutex *mutex) { return mutex ? 0 : -1; }

static int available(void) { return isDSiMode(); }
static int video_init(SDL_VideoDevice *_this, SDL_PixelFormat *format)
{
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    _this->hidden->bg = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    format->BitsPerPixel = 8;
    format->BytesPerPixel = 1;
    return 0;
}
static SDL_Rect **list_modes(SDL_VideoDevice *_this, SDL_PixelFormat *format, Uint32 flags)
{ return (SDL_Rect **)-1; }
static SDL_Surface *set_mode(SDL_VideoDevice *_this, SDL_Surface *surface, int width, int height, int bpp, Uint32 flags)
{
    if (bpp != 8 || width <= 0 || height <= 0 || width > 1024 || height > 768) {
        SDL_SetError("DSi requires an indexed mode at most 1024x768"); return NULL;
    }
    void *pixels = SDL_calloc((size_t)width, (size_t)height);
    if (!pixels) { SDL_OutOfMemory(); return NULL; }
    if (!SDL_ReallocFormat(surface, 8, 0, 0, 0, 0)) { SDL_free(pixels); return NULL; }
    SDL_free(_this->hidden->pixels);
    _this->hidden->pixels = pixels;
    surface->pixels = pixels;
    surface->w = width; surface->h = height; surface->pitch = width;
    surface->flags = SDL_FULLSCREEN | SDL_SWSURFACE;
    printf("DSi video: %dx%d indexed\n", width, height);
    return surface;
}
static void update(SDL_VideoDevice *_this, int count, SDL_Rect *rects)
{
    SDL_Surface *surface = _this->screen;
    if (!surface || !surface->pixels) return;
    dsi_present_index8(surface->pixels, surface->w, surface->h, surface->pitch,
        bgGetGfxPtr(_this->hidden->bg), 128);
}
static int set_colors(SDL_VideoDevice *_this, int first, int count, SDL_Color *colors)
{
    for (int i = 0; i < count && first + i < 256; ++i)
        BG_PALETTE[first + i] = dsi_palette_color(colors[i].r, colors[i].g, colors[i].b);
    return 1;
}
static void video_quit(SDL_VideoDevice *_this)
{
    SDL_free(_this->hidden->pixels); _this->hidden->pixels = NULL;
    if (_this->screen) _this->screen->pixels = NULL;
}
static void delete_device(SDL_VideoDevice *device) { SDL_free(device->hidden); SDL_free(device); }
static int alloc_hw(SDL_VideoDevice *_this, SDL_Surface *surface) { return -1; }
static void free_hw(SDL_VideoDevice *_this, SDL_Surface *surface) { }
static void init_keymap(SDL_VideoDevice *_this) { }
static void pump_events(SDL_VideoDevice *_this)
{
    static const SDLKey keys[12] = { SDLK_LALT, SDLK_LCTRL, SDLK_ESCAPE, SDLK_RETURN,
        SDLK_RIGHT, SDLK_LEFT, SDLK_UP, SDLK_DOWN, SDLK_BACKSPACE, SDLK_TAB, SDLK_LSHIFT, SDLK_SPACE };
    static int touched;
    static int mouse_x, mouse_y;
    scanKeys();
    u32 down = keysDown(), up = keysUp();
    for (int i = 0; i < 12; ++i) {
        SDL_keysym sym = { .scancode = i, .sym = keys[i], .mod = KMOD_NONE, .unicode = 0 };
        if (down & (1u << i)) SDL_PrivateKeyboard(SDL_PRESSED, &sym);
        if (up & (1u << i)) SDL_PrivateKeyboard(SDL_RELEASED, &sym);
    }
    if (_this->screen && (keysHeld() & KEY_TOUCH)) {
        touchPosition touch; touchRead(&touch);
        mouse_x = dsi_source_coordinate(touch.px, 256, _this->screen->w);
        mouse_y = dsi_source_coordinate(touch.py, 192, _this->screen->h);
        SDL_PrivateMouseMotion(0, 0, mouse_x, mouse_y);
        if (!touched) SDL_PrivateMouseButton(SDL_PRESSED, SDL_BUTTON_LEFT, mouse_x, mouse_y);
        touched = 1;
    } else if (touched) {
        SDL_PrivateMouseButton(SDL_RELEASED, SDL_BUTTON_LEFT, mouse_x, mouse_y);
        touched = 0;
    }
    if (!pmMainLoop()) SDL_PrivateQuit();
}
static SDL_VideoDevice *create_device(int index)
{
    SDL_VideoDevice *device = SDL_calloc(1, sizeof(*device));
    if (!device) return NULL;
    device->hidden = SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) { SDL_free(device); return NULL; }
    device->VideoInit = video_init; device->ListModes = list_modes;
    device->SetVideoMode = set_mode; device->UpdateRects = update;
    device->SetColors = set_colors; device->VideoQuit = video_quit;
    device->AllocHWSurface = alloc_hw; device->FreeHWSurface = free_hw;
    device->InitOSKeymap = init_keymap; device->PumpEvents = pump_events;
    device->free = delete_device;
    return device;
}
VideoBootStrap NDS_bootstrap = { "nds", "Nintendo DSi indexed framebuffer", available, create_device };
