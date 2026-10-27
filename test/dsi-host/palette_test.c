#include <nds.h>
#include <SDL.h>
#include <SDL_sysvideo.h>
#include <stdio.h>
#include <string.h>
#include "dsi_video.h"
#undef main

volatile uint16_t host_palette[256], host_vram[32768];
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
    SDL_FreeSurface(image);
    SDL_Quit();
    puts("PASS: event queue, bottom LCD, 12 button press/release pairs, no modifiers, aligned touch, 256 colors, 49152 pixels");
    return 0;
}
