#include <nds.h>
#include <SDL.h>
#include <SDL_sysvideo.h>
#include <stdio.h>
#include <string.h>
#include "dsi_video.h"
#undef main
#include "r5_settings_test.h"
#include "r6_test.h"
#include "audio_test.h"
#include "potion_test.h"
#include "performance_test.h"
#include "render_kernel_test.h"
#include "scene_options_test.h"

volatile uint16_t host_palette[256], host_vram[32768];
volatile uint16_t host_sub_palette[256], host_sub_vram[32768];
unsigned host_visible_backgrounds;
uint64_t host_ticks;
u32 host_key_state, host_previous_keys, host_scanned_keys;
touchPosition host_touch;
int host_main_on_bottom;
void DsiTrace(const char *format, ...) { }
size_t strlcpy(char *destination, const char *source, size_t size)
{
    size_t length = strlen(source);
    if (size) {
        size_t copied = length < size - 1 ? length : size - 1;
        memcpy(destination, source, copied);
        destination[copied] = 0;
    }
    return length;
}

int main(void)
{
    for (int i = 0; i < 256; ++i) host_sub_palette[i] = (uint16_t)(0x7000 + i);
    if (DsiTopMapScale() != 35) { puts("FAIL: default wider map scale"); return 1; }
    DsiConfigureOptions(250, 50, DSI_L_CONTROLLER, DSI_L_CONTROLLER);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { puts(SDL_GetError()); return 1; }
    SDL_Surface *screen = SDL_SetVideoMode(640, 480, 8, SDL_FULLSCREEN | SDL_HWPALETTE);
    if (!screen) { puts(SDL_GetError()); return 1; }
    if (!host_main_on_bottom) { puts("FAIL: main display was not routed to bottom LCD"); return 1; }
    if (screen != current_video->screen) {
        puts("FAIL: unexpected SDL shadow surface remaps the game's indexed palette");
        return 1;
    }
    if (!(screen->flags & SDL_HWPALETTE)) { puts("FAIL: missing hardware palette flag"); return 1; }
    SDL_Color colors[256];
    for (int i = 0; i < 256; ++i) {
        colors[i].r = (unsigned char)i;
        colors[i].g = (unsigned char)(255 - i);
        colors[i].b = (unsigned char)(i ^ 0x55);
        colors[i].unused = 0;
    }
    SDL_SetColors(screen, colors, 0, 256);
    for (int i = 0; i < 256; ++i)
        if (host_palette[i] != dsi_palette_color(colors[i].r, colors[i].g, colors[i].b)) {
            printf("FAIL: palette index %d\n", i); return 1;
        }
    SDL_Surface *image = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 8, 0, 0, 0, 0);
    if (!image) { puts(SDL_GetError()); return 1; }
    SDL_SetPalette(image, SDL_LOGPAL, colors, 0, 256);
    for (int y = 0; y < image->h; ++y)
        for (int x = 0; x < image->w; ++x)
            ((unsigned char *)image->pixels)[y * image->pitch + x] = (unsigned char)(x + 3 * y);
    if (SDL_BlitSurface(image, NULL, screen, NULL) != 0 || SDL_Flip(screen) != 0) {
        puts(SDL_GetError()); return 1;
    }
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 256; ++x) {
            unsigned char index = (unsigned char)(host_vram[y * 128 + x / 2] >> (8 * (x % 2)));
            if (index != (unsigned char)((x * 640 / 256) + 3 * (y * 480 / 192))) {
                printf("FAIL: blit/present index at %d,%d\n", x, y); return 1;
            }
        }
    SDL_Event event;
    while (SDL_PollEvent(&event)) { }
    static const SDLKey expected_keys[12] = { SDLK_F2, SDLK_F1, SDLK_F7, SDLK_F8,
        SDLK_RIGHT, SDLK_LEFT, SDLK_UP, SDLK_DOWN, SDLK_F6, SDLK_F5, SDLK_F4, SDLK_F3 };
    for (int bit = 0; bit < 12; ++bit) {
        host_key_state = 1u << bit;
        int presses = 0, releases = 0;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == expected_keys[bit]
                && event.key.keysym.mod == KMOD_NONE) ++presses;
        }
        if (presses != 1 || !SDL_GetKeyState(NULL)[expected_keys[bit]] || SDL_GetModState() != KMOD_NONE) {
            printf("FAIL: button press/state/modifiers bit %d count=%d state=%u mod=%d\n", bit, presses, SDL_GetKeyState(NULL)[expected_keys[bit]], SDL_GetModState()); return 1;
        }
        host_key_state = 0;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYUP && event.key.keysym.sym == expected_keys[bit]) ++releases;
        }
        if (releases != 1 || SDL_GetKeyState(NULL)[expected_keys[bit]] || SDL_GetModState() != KMOD_NONE) {
            printf("FAIL: button release/state bit %d\n", bit); return 1;
        }
    }
    host_touch.px = 128; host_touch.py = 96;
    host_key_state = KEY_TOUCH;
    int touch_down = 0, touch_up = 0;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
            && event.button.x == 320 && event.button.y == 240) ++touch_down;
    }
    host_key_state = 0;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT
            && event.button.x == 320 && event.button.y == 240) ++touch_up;
    }
    if (touch_down != 1 || touch_up != 1) { puts("FAIL: aligned touch down/up events"); return 1; }
    SDL_Event custom;
    memset(&custom, 0, sizeof(custom));
    custom.type = SDL_USEREVENT;
    custom.user.code = 42;
    if (SDL_PushEvent(&custom) != 0 || !SDL_PollEvent(&event)
        || event.type != SDL_USEREVENT || event.user.code != 42) {
        puts("FAIL: queued loading-progress/user event"); return 1;
    }

    SDL_ShowCursor(SDL_DISABLE);
    SDL_BlitSurface(image, NULL, screen, NULL);
    DsiSetTopView(DSI_TOP_MENU);
    DsiSetReadingFocus(320, 240);
    SDL_Flip(screen);
    if ((host_visible_backgrounds & (1u << 4)) || !(host_visible_backgrounds & (1u << 7))) {
        puts("FAIL: top bitmap/console visibility"); return 1;
    }
    for (int i = 0; i < 256; ++i)
        if (host_sub_palette[i] != host_palette[i]) { puts("FAIL: top game palette"); return 1; }
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 256; ++x) {
            unsigned char index = (unsigned char)(host_sub_vram[y * 128 + x / 2] >> (8 * (x % 2)));
            if (index != (unsigned char)(192 + x + 3 * (144 + y))) {
                printf("FAIL: native reading pixel %d,%d\n", x, y); return 1;
            }
        }
    /* Reading gesture never clicks an item or sends shoulder/navigation keys.
     * Consumed held keys remain blocked until physically released. */
    host_touch.px = 255; host_touch.py = 191;
    host_key_state = KEY_L | KEY_R | KEY_RIGHT | KEY_TOUCH;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) {
            puts("FAIL: reading gesture leaked an activation"); return 1;
        }
    }
    DsiSetReadingFocus(100, 100); /* normal render focus must not undo manual pan */
    SDL_Flip(screen);
    if ((unsigned char)host_sub_vram[0] != (unsigned char)(384 + 3 * 288)) {
        puts("FAIL: reading edge clamp/manual focus"); return 1;
    }
    host_key_state = KEY_RIGHT | KEY_TOUCH;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) {
            puts("FAIL: consumed reading keys reactivated before release"); return 1;
        }
    }
    host_key_state = 0;
    while (SDL_PollEvent(&event)) { }
    host_key_state = KEY_RIGHT;
    int fresh_right = 0;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT) ++fresh_right;
    if (fresh_right != 1) { puts("FAIL: navigation after reading gesture"); return 1; }
    host_key_state = 0;
    while (SDL_PollEvent(&event)) { }

    for (int i = 0; i < 256 * 192; ++i) DsiMapBuffer()[i] = (unsigned char)(i * 7);
    DsiSetTopView(DSI_TOP_MAP);
    SDL_Flip(screen);
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 256; ++x) {
            unsigned char top = (unsigned char)(host_sub_vram[y * 128 + x / 2] >> (8 * (x % 2)));
            unsigned char bottom = (unsigned char)(host_vram[y * 128 + x / 2] >> (8 * (x % 2)));
            if (top != (unsigned char)((y * 256 + x) * 7)
                || bottom != (unsigned char)(x * 640 / 256 + 3 * (y * 480 / 192))) {
                printf("FAIL: independent buffers at %d,%d top=%u expected=%u bottom=%u expected=%u\n", x, y,
                    top, (unsigned char)((y * 256 + x) * 7), bottom,
                    (unsigned char)(x * 640 / 256 + 3 * (y * 480 / 192))); return 1;
            }
        }
    /* The shoulder chord is reserved only in menus; gameplay keeps both keys. */
    host_key_state = KEY_L | KEY_R;
    int shoulders = 0;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_F5 || event.key.keysym.sym == SDLK_F6)) ++shoulders;
    if (shoulders != 2) { puts("FAIL: gameplay shoulders intercepted"); return 1; }
    host_key_state = 0;
    while (SDL_PollEvent(&event)) { }
    DsiSetTopView(DSI_TOP_DIAGNOSTICS);
    if (!(host_visible_backgrounds & (1u << 4)) || (host_visible_backgrounds & (1u << 7))) {
        puts("FAIL: diagnostics visibility restoration"); return 1;
    }
    for (int i = 0; i < 256; ++i)
        if (host_sub_palette[i] != (uint16_t)(0x7000 + i)) { puts("FAIL: console palette restoration"); return 1; }
    /* One-pixel padded source and invalid inputs must be safe. */
    uint8_t tiny[] = { 41, 99, 99, 99 };
    if (!dsi_present_reading_index8(tiny, 1, 1, 4, -2147483647, 2147483647, 250, host_sub_vram, 128)) return 1;
    for (int i = 0; i < 192 * 128; ++i)
        if (host_sub_vram[i] != 0x2929) { puts("FAIL: small reading source"); return 1; }
    if (dsi_present_reading_index8(NULL, 1, 1, 1, 0, 0, 250, host_sub_vram, 128)
        || dsi_present_reading_index8(tiny, 2, 1, 1, 0, 0, 250, host_sub_vram, 128)
        || dsi_present_reading_index8(tiny, 1, 1, 4, 0, 0, 250, host_sub_vram, 127)) {
        puts("FAIL: invalid reading arguments accepted"); return 1;
    }
    if (!CheckScaleCache()) { puts("FAIL: scale cache"); return 1; }
    if (!CheckRenderKernels() || !CheckPackedStores()) { puts("FAIL: render kernel/word store regression"); return 1; }
    if (!CheckPerformance(screen)) { puts("FAIL: performance regression"); return 1; }
    if (!CheckMapOff(screen)) { puts("FAIL: scene/map off options"); return 1; }
    if (!CheckR5Settings(screen)) return 1;
    if (!CheckR6(screen)) { puts("FAIL: R6 regression"); return 1; }
    if (!CheckPotionAction()) { puts("FAIL: potion input regression"); return 1; }
    if (!CheckAudio()) { puts("FAIL: audio regression"); return 1; }
    DsiSetTopView(DSI_TOP_MENU);
    DsiSetLoading(1);
    DsiSetLoading(1);
    DsiSetTopView(DSI_TOP_MAP);
    SDL_Color loading_color = { 0, 255, 0, 0 };
    SDL_SetColors(screen, &loading_color, 0, 1);
    SDL_Flip(screen);
    if ((host_visible_backgrounds & ((1u<<4)|(1u<<7))) || host_sub_palette[0]) { puts("FAIL: black loading screen"); return 1; }
    DsiSetLoading(0);
    if (host_visible_backgrounds & (1u<<7)) { puts("FAIL: nested loading ended early"); return 1; }
    DsiSetLoading(0);
    if (!(host_visible_backgrounds & (1u<<7))) return 1;
    DsiSetLoading(1);
    DsiSetMoviePlaying(1);
    DsiRestoreDiagnostics();
    if (!(host_visible_backgrounds & (1u<<4)) || (host_visible_backgrounds & (1u<<7))) return 1;
    puts("PASS: black loading screen, nested restoration and fatal console override");
    SDL_FreeSurface(image);
    SDL_Quit();
    puts("PASS: R3 input/palette/pixels; R4 native menu crop, manual pan, input isolation, independent map, console restoration, tiny/invalid sources");
    return 0;
}
