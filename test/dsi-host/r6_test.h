/* Exercise the actual SDL queue/state, including both owners of right-click. */
static int CheckR6(SDL_Surface *screen)
{
    SDL_Event event;
    const u32 bits[] = { KEY_L, KEY_R };
    const SDLKey keys[] = { SDLK_F5, SDLK_F6 };
    DsiSetTopView(DSI_TOP_MAP);
    for (int side = 0; side < 2; ++side) {
        for (int action = DSI_L_CONTROLLER; action <= DSI_L_DISABLED; ++action) {
            host_key_state = 0;
            while (SDL_PollEvent(&event)) { }
            DsiConfigureOptions(200, 35, side == 0 ? action : DSI_L_DISABLED,
                side == 1 ? action : DSI_L_DISABLED);
            host_key_state = bits[side];
            int key_down = 0, mouse_down = 0, key_up = 0, mouse_up = 0;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym != keys[side]) return 0;
                    ++key_down;
                }
                if (event.type == SDL_MOUSEBUTTONDOWN) ++mouse_down;
            }
            if (key_down != (action == DSI_L_CONTROLLER)
                || mouse_down != (action == DSI_L_RIGHT_MOUSE)) return 0;
            /* Holding must never repeat either action. */
            host_ticks += 500000;
            while (SDL_PollEvent(&event))
                if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) return 0;
            /* Remapping while still physically held releases only the old action. */
            DsiConfigureOptions(200, 35, DSI_L_DISABLED, DSI_L_DISABLED);
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) return 0;
                if (event.type == SDL_KEYUP) ++key_up;
                if (event.type == SDL_MOUSEBUTTONUP) ++mouse_up;
            }
            if (key_up != key_down || mouse_up != mouse_down
                || SDL_GetKeyState(NULL)[keys[side]]
                || (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT))) return 0;
            DsiConfigureOptions(200, 35, DSI_L_CONTROLLER, DSI_L_CONTROLLER);
            while (SDL_PollEvent(&event))
                if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) return 0;
            host_key_state = 0;
            while (SDL_PollEvent(&event))
                if (event.type == SDL_KEYUP || event.type == SDL_MOUSEBUTTONUP) return 0;
        }
        /* Either shoulder + touch moves the pointer without a left click,
         * including releasing the shoulder before lifting the stylus. */
        DsiConfigureOptions(200, 35, DSI_L_RIGHT_MOUSE, DSI_L_RIGHT_MOUSE);
        host_touch.px = 123; host_touch.py = 45;
        host_key_state = bits[side] | KEY_TOUCH;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_KEYDOWN || (event.type == SDL_MOUSEBUTTONDOWN
                    && (event.button.button != SDL_BUTTON_RIGHT
                        || event.button.x != 307 || event.button.y != 112))) return 0;
        host_key_state = KEY_TOUCH;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_MOUSEBUTTONDOWN) return 0;
        host_key_state = 0;
        while (SDL_PollEvent(&event)) { }
        /* Form the reader chord in either order, then release separately. */
        DsiSetTopView(DSI_TOP_MENU);
        host_key_state = bits[side];
        while (SDL_PollEvent(&event))
            if (event.type == SDL_MOUSEBUTTONDOWN) return 0;
        host_key_state = KEY_L | KEY_R | KEY_TOUCH | KEY_LEFT;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) return 0;
        host_key_state = bits[side] | KEY_TOUCH;
        host_ticks += 200000;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) return 0;
        host_key_state = 0;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) return 0;
        /* A quick menu tap and a held menu press each produce one click. */
        for (int hold = 0; hold < 2; ++hold) {
            int downs = 0, ups = 0;
            host_key_state = bits[side];
            while (SDL_PollEvent(&event))
                if (event.type == SDL_MOUSEBUTTONDOWN) return 0;
            if (hold) {
                host_ticks += 121000;
                while (SDL_PollEvent(&event))
                    if (event.type == SDL_MOUSEBUTTONDOWN) ++downs;
            }
            host_key_state = 0;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_MOUSEBUTTONDOWN) ++downs;
                if (event.type == SDL_MOUSEBUTTONUP) ++ups;
            }
            if (downs != 1 || ups != 1) return 0;
        }
        DsiSetTopView(DSI_TOP_MAP);
    }
    int presses = 0, releases = 0;
    const u32 overlap[] = { KEY_L, KEY_L | KEY_R, KEY_R, 0 };
    for (int step = 0; step < 4; ++step) {
        host_key_state = overlap[step];
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) return 0;
            if (event.type == SDL_MOUSEBUTTONDOWN) ++presses;
            if (event.type == SDL_MOUSEBUTTONUP) ++releases;
        }
        if (!!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)) != (step < 3)) return 0;
    }
    if (presses != 1 || releases != 1) return 0;
    /* A movie must leave the top bitmap untouched, black out both layers,
     * accept bottom frames/palettes, and restore each possible view. */
    for (int view = DSI_TOP_DIAGNOSTICS; view <= DSI_TOP_MAP; ++view) {
        DsiSetTopView((enum DsiTopView)view);
        host_sub_vram[0] = 0x1234;
        DsiSetMoviePlaying(1);
        DsiSetMoviePlaying(1); /* idempotent */
        SDL_Color color = { 255, 0, 0, 0 };
        SDL_SetColors(screen, &color, 0, 1);
        ((unsigned char *)screen->pixels)[0] = 37;
        SDL_Flip(screen);
        if (host_visible_backgrounds & ((1u << 4) | (1u << 7))
            || host_sub_palette[0] != 0 || host_sub_vram[0] != 0x1234
            || (unsigned char)host_vram[0] != 37) return 0;
        DsiSetMoviePlaying(0);
        DsiSetMoviePlaying(0);
        if (view == DSI_TOP_DIAGNOSTICS) {
            if (!(host_visible_backgrounds & (1u << 4)) || host_sub_palette[0] != 0x7000) return 0;
        } else if (!(host_visible_backgrounds & (1u << 7))
            || host_sub_palette[0] != dsi_palette_color(255, 0, 0)) return 0;
    }
    DsiSetMoviePlaying(1);
    DsiSetTopView(DSI_TOP_MENU);
    if (host_visible_backgrounds & (1u << 7)) return 0;
    DsiSetMoviePlaying(0);
    if (!(host_visible_backgrounds & (1u << 7))) return 0;
    puts("PASS: R6 both shoulder mappings/hold/live remap/touch/chord/overlap, movie isolation/palette/view restoration");
    return 1;
}
