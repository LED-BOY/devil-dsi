/* Actual SDL/driver integration checks for configurable R5 zoom and L input. */
static int CheckR5Settings(SDL_Surface *screen)
{
    SDL_Event event;
    const int zooms[] = { 150, 200, 250 };
    const int scales[] = { 25, 35, 50 };
    for (int choice = 0; choice < 3; ++choice) {
        DsiConfigureOptions(zooms[choice], scales[choice], DSI_L_CONTROLLER);
        if (DsiTopMapScale() != scales[choice]) { puts("FAIL: configurable map scale"); return 0; }
        DsiSetTopView(DSI_TOP_MENU);
        DsiSetReadingFocus(320 + choice, 240); /* distinct focus to reset manual pan */
        SDL_Flip(screen);
        int cw = 64000 / zooms[choice], ch = 48000 / zooms[choice];
        for (int y = 0; y < 192; ++y)
            for (int x = 0; x < 256; ++x) {
                unsigned char actual = (unsigned char)(host_sub_vram[y * 128 + x / 2] >> (8 * (x % 2)));
                unsigned char expected = (unsigned char)(320 + choice - cw / 2 + x * cw / 256
                    + 3 * (240 - ch / 2 + y * ch / 192));
                if (actual != expected) { printf("FAIL: zoom %d at %d,%d\n", zooms[choice], x, y); return 0; }
            }
        if (!dsi_present_reading_index8(screen->pixels, 640, 480, screen->pitch,
                2147483647, 2147483647, zooms[choice], host_sub_vram, 128)) return 0;
        for (int y = 0; y < 192; ++y)
            for (int x = 0; x < 256; ++x) {
                unsigned char actual = (unsigned char)(host_sub_vram[y * 128 + x / 2] >> (8 * (x % 2)));
                if (actual != (unsigned char)(640 - cw + x * cw / 256 + 3 * (480 - ch + y * ch / 192))) {
                    puts("FAIL: zoom edge clamp"); return 0;
                }
            }
    }
    DsiConfigureOptions(-1, 0, 99); /* Invalid configuration safely restores defaults. */
    if (DsiTopMapScale() != 35) return 0;
    DsiSetReadingFocus(320, 240);
    SDL_Flip(screen);
    if ((unsigned char)host_sub_vram[0] != (unsigned char)(160 + 3 * 120)) {
        puts("FAIL: default 2x zoom"); return 0;
    }
    if (dsi_present_reading_index8(screen->pixels, 640, 480, screen->pitch,
            320, 240, 0, host_sub_vram, 128)) { puts("FAIL: invalid zoom accepted"); return 0; }

    DsiSetTopView(DSI_TOP_MAP);
    SDL_WarpMouse(211, 123);
    while (SDL_PollEvent(&event)) { }
    host_key_state = KEY_L;
    int right_down = 0, right_up = 0;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN) { puts("FAIL: right-click L leaked controller input"); return 0; }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT
            && event.button.x == 211 && event.button.y == 123) ++right_down;
    }
    if (right_down != 1 || !(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        puts("FAIL: default L right mouse down/state"); return 0;
    }
    /* A setting change while L is held must release the action that was pressed. */
    DsiConfigureOptions(200, 35, DSI_L_CONTROLLER);
    host_key_state = 0;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYUP) { puts("FAIL: L released wrong mapping"); return 0; }
        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) ++right_up;
    }
    if (right_up != 1 || (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        puts("FAIL: L right mouse release/state"); return 0;
    }

    DsiConfigureOptions(200, 35, DSI_L_RIGHT_MOUSE);
    DsiSetTopView(DSI_TOP_MENU);
    host_key_state = KEY_L;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN) {
            puts("FAIL: menu L did not wait for reading chord"); return 0;
        }
    host_ticks += 50000;
    host_key_state = KEY_L | KEY_R | KEY_TOUCH;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN) {
            puts("FAIL: reading chord generated click/key"); return 0;
        }
    host_ticks += 150000;
    host_key_state = 0;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
            puts("FAIL: cancelled chord clicked on release"); return 0;
        }
    /* Quick L tap still produces one complete menu right-click. */
    host_key_state = KEY_L;
    while (SDL_PollEvent(&event)) { }
    host_key_state = 0;
    right_down = right_up = 0;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) ++right_down;
        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) ++right_up;
    }
    if (right_down != 1 || right_up != 1) { puts("FAIL: quick L tap"); return 0; }
    /* Held L starts one click after the menu chord interval. */
    host_key_state = KEY_L;
    while (SDL_PollEvent(&event)) { }
    host_ticks += 121000;
    right_down = right_up = 0;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) ++right_down;
    host_key_state = 0;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) ++right_up;
    if (right_down != 1 || right_up != 1) { puts("FAIL: held menu L"); return 0; }

    DsiSetTopView(DSI_TOP_MAP);
    host_touch.px = 200; host_touch.py = 100;
    host_key_state = KEY_L | KEY_TOUCH;
    right_down = 0;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            puts("FAIL: L+touch generated left click too"); return 0;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT
            && event.button.x == 500 && event.button.y == 250) ++right_down;
    }
    if (right_down != 1) { puts("FAIL: L+touch right-click coordinates"); return 0; }
    host_key_state = 0;
    while (SDL_PollEvent(&event)) { }
    DsiConfigureOptions(200, 35, DSI_L_DISABLED);
    host_key_state = KEY_L;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) {
            puts("FAIL: disabled L generated input"); return 0;
        }
    host_key_state = 0;
    while (SDL_PollEvent(&event)) { }
    DsiConfigureOptions(200, 35, DSI_L_CONTROLLER);
    host_key_state = KEY_L;
    int shoulder_down = 0;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F5) ++shoulder_down;
    host_key_state = 0;
    while (SDL_PollEvent(&event)) { }
    if (shoulder_down != 1 || SDL_GetKeyState(NULL)[SDLK_F5]) {
        puts("FAIL: restored controller shoulder"); return 0;
    }
    puts("PASS: R5 3 zoom levels/all pixels/edge clamps, 3 map scales, defaults, L right-click/quick tap/hold/remap/disabled, chord isolation, L+touch");
    return 1;
}
