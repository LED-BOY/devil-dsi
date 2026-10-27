/* DSi SDL 1.2 video/event driver. SDL core supplies software surfaces and blits. */
#include <nds.h>
#include <calico/system/tick.h>
#include <SDL.h>
#include <SDL_sysvideo.h>
#include <SDL_pixels_c.h>
#include <SDL_events_c.h>
#include <stdio.h>
#include "dsi_video.h"
#include "dsi_trace.h"

struct SDL_PrivateVideoData { void *pixels; int bg; };
static u64 start_ticks;
static enum DsiTopView top_view;
static int top_bg = -1;
static uint16_t console_palette[256], game_palette[256];
static uint8_t map_pixels[DSI_WIDTH * DSI_HEIGHT];
static int reading_x = 320, reading_y = 240, focus_x = -1, focus_y = -1;
static int manual_reading;
static Uint32 next_pan;
static int reading_percent = 200, map_scale = 35;
static int shoulder_action[2] = { DSI_L_RIGHT_MOUSE, DSI_L_CONTROLLER };
static int movie_active;
static enum DsiTopView movie_restore_view;

void DsiConfigureOptions(int zoom, int scale, int l_action, int r_action)
{
    reading_percent = (zoom == 150 || zoom == 250) ? zoom : 200;
    map_scale = (scale == 25 || scale == 50) ? scale : 35;
    shoulder_action[0] = (l_action >= DSI_L_CONTROLLER && l_action <= DSI_L_DISABLED) ? l_action : DSI_L_RIGHT_MOUSE;
    shoulder_action[1] = (r_action >= DSI_L_CONTROLLER && r_action <= DSI_L_DISABLED) ? r_action : DSI_L_CONTROLLER;
}

int DsiTopMapScale(void) { return map_scale; }

uint8_t *DsiMapBuffer(void) { return map_pixels; }

void DsiSetTopView(enum DsiTopView view)
{
    if (movie_active && view != DSI_TOP_MOVIE) { movie_restore_view = view; return; }
    if (top_view == view) return;
    top_view = view;
    focus_x = focus_y = -1;
    manual_reading = 0;
    if (view == DSI_TOP_MENU) { reading_x = 320; reading_y = 240; }
    if (top_bg < 0) return;
    if (view == DSI_TOP_DIAGNOSTICS) {
        bgHide(top_bg);
        for (int i = 0; i < 256; ++i) BG_PALETTE_SUB[i] = console_palette[i];
        bgShow(4); /* consoleDemoInit's sub BG0 */
    } else if (view == DSI_TOP_MOVIE) {
        bgHide(4);
        bgHide(top_bg);
        BG_PALETTE_SUB[0] = 0; /* Black backdrop, independent of movie palette. */
    } else {
        bgHide(4);
        for (int i = 0; i < 256; ++i) BG_PALETTE_SUB[i] = game_palette[i];
        bgShow(top_bg);
    }
}

void DsiSetMoviePlaying(int playing)
{
    if (!!playing == movie_active) return;
    if (playing) {
        movie_restore_view = top_view;
        movie_active = 1;
        DsiSetTopView(DSI_TOP_MOVIE);
    } else {
        movie_active = 0;
        DsiSetTopView(movie_restore_view);
    }
}

void DsiSetReadingFocus(int x, int y)
{
    if (x == focus_x && y == focus_y) return;
    focus_x = x; focus_y = y;
    if (!manual_reading) { reading_x = x; reading_y = y; }
}

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
/* SDL_THREADS_DISABLED leaves EventQ.lock null but still calls these helpers.
 * A null lock is valid in this single-engine-thread build. Rejecting it drops
 * queued events while held-key state continues to change (D-pad only symptom). */
int SDL_mutexP(SDL_mutex *mutex) { (void)mutex; return 0; }
int SDL_mutexV(SDL_mutex *mutex) { (void)mutex; return 0; }

static int available(void) { return isDSiMode(); }
static int video_init(SDL_VideoDevice *_this, SDL_PixelFormat *format)
{
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    _this->hidden->bg = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    /* Main-engine graphics on the touch LCD; the sub-engine console moves up. */
    lcdMainOnBottom();
    /* Keep console tiles/map in the first 64 KiB of VRAM C. The 8-bit top
     * bitmap uses the other 64 KiB (mapBase 4 = 4 * 16 KiB), without overlap. */
    for (int i = 0; i < 256; ++i) console_palette[i] = BG_PALETTE_SUB[i];
    videoSetModeSub(MODE_5_2D);
    top_bg = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 4, 0);
    bgHide(top_bg);
    bgShow(4);
    top_view = DSI_TOP_DIAGNOSTICS;
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
    /* Advertise the actual indexed hardware palette. Otherwise SDL creates a
     * shadow surface and remaps indices into its initial dither palette. */
    surface->flags = SDL_FULLSCREEN | SDL_SWSURFACE | SDL_HWPALETTE;
    printf("DSi video: %dx%d indexed\n", width, height);
    DsiTrace("video %dx%d indexed with hardware palette", width, height);
    return surface;
}
static void update(SDL_VideoDevice *_this, int count, SDL_Rect *rects)
{
    SDL_Surface *surface = _this->screen;
    if (!surface || !surface->pixels) return;
    dsi_present_index8(surface->pixels, surface->w, surface->h, surface->pitch,
        bgGetGfxPtr(_this->hidden->bg), 128);
    if (top_view == DSI_TOP_MENU)
        dsi_present_reading_index8(surface->pixels, surface->w, surface->h,
            surface->pitch, reading_x, reading_y, reading_percent, bgGetGfxPtr(top_bg), 128);
    else if (top_view == DSI_TOP_MAP)
        dsi_present_index8(map_pixels, DSI_WIDTH, DSI_HEIGHT, DSI_WIDTH,
            bgGetGfxPtr(top_bg), 128);
}
static int set_colors(SDL_VideoDevice *_this, int first, int count, SDL_Color *colors)
{
    if (!colors || first < 0 || count < 0 || first > 256 || count > 256 - first)
        return 0;
    for (int i = 0; i < count && first + i < 256; ++i) {
        uint16_t color = dsi_palette_color(colors[i].r, colors[i].g, colors[i].b);
        game_palette[first + i] = BG_PALETTE[first + i] = color;
        if (top_view != DSI_TOP_DIAGNOSTICS && top_view != DSI_TOP_MOVIE) BG_PALETTE_SUB[first + i] = color;
    }
    static unsigned updates;
    if (count && updates++ < 4)
        DsiTrace("palette %d+%d RGB=%u,%u,%u hw=%04x", first, count,
            colors[0].r, colors[0].g, colors[0].b, BG_PALETTE[first]);
    return 1;
}
static void video_quit(SDL_VideoDevice *_this)
{
    movie_active = 0;
    DsiSetTopView(DSI_TOP_DIAGNOSTICS);
    SDL_free(_this->hidden->pixels); _this->hidden->pixels = NULL;
    if (_this->screen) _this->screen->pixels = NULL;
}
static void delete_device(SDL_VideoDevice *device) { SDL_free(device->hidden); SDL_free(device); }
static int alloc_hw(SDL_VideoDevice *_this, SDL_Surface *surface) { return -1; }
static void free_hw(SDL_VideoDevice *_this, SDL_Surface *surface) { }
static void init_keymap(SDL_VideoDevice *_this) { }
static void pump_events(SDL_VideoDevice *_this)
{
    /* Engine button names use Xbox positions: physical B is bottom/A, physical
     * A is right/B, physical Y is left/X, physical X is top/Y. Never use
     * modifier keys here: SDL treats those as global keyboard modifiers. */
    static const SDLKey keys[12] = { SDLK_F2, SDLK_F1, SDLK_F7, SDLK_F8,
        SDLK_RIGHT, SDLK_LEFT, SDLK_UP, SDLK_DOWN, SDLK_F6, SDLK_F5, SDLK_F4, SDLK_F3 };
    static int touched, pointer_only_touch;
    static int mouse_x, mouse_y;
    static unsigned logged_buttons;
    static u32 delivered_keys, blocked_keys;
    static const u32 shoulder_bits[2] = { KEY_L, KEY_R };
    static int active_action[2], pending[2], mouse_down[2];
    static Uint32 press_time[2];
    scanKeys();
    u32 held = keysHeld();
    const int reading_control = top_view == DSI_TOP_MENU
        && (held & (KEY_L | KEY_R)) == (KEY_L | KEY_R);
    blocked_keys &= held;
    if (reading_control) {
        manual_reading = 1;
        blocked_keys |= held;
        if ((Sint32)(SDL_GetTicks() - next_pan) >= 0) {
            if (held & KEY_LEFT) reading_x -= 12;
            if (held & KEY_RIGHT) reading_x += 12;
            if (held & KEY_UP) reading_y -= 12;
            if (held & KEY_DOWN) reading_y += 12;
            next_pan = SDL_GetTicks() + 40;
        }
        if (_this->screen && (held & KEY_TOUCH)) {
            touchPosition touch; touchRead(&touch);
            reading_x = dsi_source_coordinate(touch.px, DSI_WIDTH, _this->screen->w);
            reading_y = dsi_source_coordinate(touch.py, DSI_HEIGHT, _this->screen->h);
        }
    }
    if (_this->screen) {
        if (reading_x < 0) reading_x = 0;
        if (reading_x > _this->screen->w) reading_x = _this->screen->w;
        if (reading_y < 0) reading_y = 0;
        if (reading_y > _this->screen->h) reading_y = _this->screen->h;
    }
    /* Release the latched action on a live remap, then wait for physical up.
     * Do not reinterpret a held shoulder as a fresh press of its new action. */
    for (int i = 0; i < 2; ++i)
        if ((delivered_keys & shoulder_bits[i]) && active_action[i] != shoulder_action[i])
            blocked_keys |= held & shoulder_bits[i];
    u32 effective_keys = held & ~blocked_keys;
    u32 down = effective_keys & ~delivered_keys, up = delivered_keys & ~effective_keys;
    delivered_keys = effective_keys;
    for (int i = 0; i < 2; ++i)
        if (down & shoulder_bits[i]) active_action[i] = shoulder_action[i];
    if (down & 0xfff) { manual_reading = 0; focus_x = focus_y = -1; }
    for (int i = 0; i < 12; ++i) {
        if (i == 9 && active_action[0] != DSI_L_CONTROLLER) continue;
        if (i == 8 && active_action[1] != DSI_L_CONTROLLER) continue;
        SDL_keysym sym = { .scancode = i, .sym = keys[i], .mod = KMOD_NONE, .unicode = 0 };
        if (down & (1u << i)) {
            int posted = SDL_PrivateKeyboard(SDL_PRESSED, &sym);
            if (logged_buttons++ < 24) DsiTrace("button bit=%d down key=%d posted=%d", i, sym.sym, posted);
        }
        if (up & (1u << i)) {
            int posted = SDL_PrivateKeyboard(SDL_RELEASED, &sym);
            if (logged_buttons++ < 24) DsiTrace("button bit=%d up key=%d posted=%d", i, sym.sym, posted);
        }
    }
    if (_this->screen && (effective_keys & KEY_TOUCH)) {
        touchPosition touch; touchRead(&touch);
        mouse_x = dsi_source_coordinate(touch.px, 256, _this->screen->w);
        mouse_y = dsi_source_coordinate(touch.py, 192, _this->screen->h);
        SDL_PrivateMouseMotion(0, 0, mouse_x, mouse_y);
    }
    if (!(held & KEY_TOUCH)) pointer_only_touch = 0;
    if ((held & KEY_TOUCH) && (reading_control
            || ((effective_keys & KEY_L) && active_action[0] == DSI_L_RIGHT_MOUSE)
            || ((effective_keys & KEY_R) && active_action[1] == DSI_L_RIGHT_MOUSE)))
        pointer_only_touch = 1;
    const int left_touch = _this->screen && (effective_keys & KEY_TOUCH) && !pointer_only_touch;
    if (left_touch && !touched) {
        SDL_PrivateMouseButton(SDL_PRESSED, SDL_BUTTON_LEFT, mouse_x, mouse_y);
        touched = 1;
    } else if (!left_touch && touched) {
        SDL_PrivateMouseButton(SDL_RELEASED, SDL_BUTTON_LEFT, mouse_x, mouse_y);
        touched = 0;
    }
    /* Both shoulders share one SDL mouse button. Keep it down until the last
     * owner releases; menu taps wait briefly so L+R can cancel the click. */
    SDL_GetMouseState(&mouse_x, &mouse_y);
    int was_down = mouse_down[0] || mouse_down[1], tap = 0;
    for (int i = 0; i < 2; ++i) {
        u32 bit = shoulder_bits[i];
        if ((down & bit) && active_action[i] == DSI_L_RIGHT_MOUSE) {
            pending[i] = 1;
            press_time[i] = SDL_GetTicks();
        }
        if (up & bit) {
            if (pending[i] && !(blocked_keys & bit)) tap = 1;
            pending[i] = mouse_down[i] = 0;
        }
        if (reading_control) pending[i] = 0;
        if (pending[i] && (effective_keys & bit)
            && (top_view != DSI_TOP_MENU || SDL_GetTicks() - press_time[i] >= 120)) {
            mouse_down[i] = 1;
            pending[i] = 0;
        }
    }
    int is_down = mouse_down[0] || mouse_down[1];
    if (!was_down && (is_down || tap))
        SDL_PrivateMouseButton(SDL_PRESSED, SDL_BUTTON_RIGHT, mouse_x, mouse_y);
    if (!is_down && (was_down || tap))
        SDL_PrivateMouseButton(SDL_RELEASED, SDL_BUTTON_RIGHT, mouse_x, mouse_y);
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
