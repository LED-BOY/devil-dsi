static int CheckPotionAction(void)
{
    SDL_Event event;
    DsiConfigureOptions(200,35,DSI_L_RIGHT_MOUSE,DSI_L_HEALTH_POTION);
    DsiSetTopView(DSI_TOP_MAP);
    host_key_state=KEY_R;
    int uses=0;
    while (SDL_PollEvent(&event)) {
        if (event.type==DSI_EVENT_HEALTH_POTION) ++uses;
        if (event.type==SDL_KEYDOWN || event.type==SDL_MOUSEBUTTONDOWN) return 0;
    }
    if (uses!=1) return 0;
    host_ticks+=1000000;
    while (SDL_PollEvent(&event)) if (event.type==DSI_EVENT_HEALTH_POTION) return 0;
    host_key_state=0;
    while (SDL_PollEvent(&event)) if (event.type==DSI_EVENT_HEALTH_POTION) return 0;
    for (int order=0;order<2;++order) {
        DsiSetTopView(DSI_TOP_MENU);
        host_key_state=order?KEY_R:KEY_L;
        while (SDL_PollEvent(&event)) if (event.type==DSI_EVENT_HEALTH_POTION) return 0;
        host_key_state=KEY_R|KEY_L|KEY_TOUCH;
        host_ticks+=50000;
        while (SDL_PollEvent(&event)) if (event.type==DSI_EVENT_HEALTH_POTION) return 0;
        host_key_state=0;
        while (SDL_PollEvent(&event)) if (event.type==DSI_EVENT_HEALTH_POTION) return 0;
    }
    host_key_state=KEY_R;
    while (SDL_PollEvent(&event)) { }
    DsiConfigureOptions(200,35,DSI_L_RIGHT_MOUSE,DSI_L_DISABLED);
    while (SDL_PollEvent(&event)) if (event.type==DSI_EVENT_HEALTH_POTION) return 0;
    host_key_state=0;
    while (SDL_PollEvent(&event)) { }
    DsiConfigureOptions(200,35,DSI_L_RIGHT_MOUSE,999);
    DsiSetTopView(DSI_TOP_MAP);
    host_key_state=KEY_R;
    uses=0;
    while (SDL_PollEvent(&event)) if (event.type==DSI_EVENT_HEALTH_POTION) ++uses;
    host_key_state=0;
    while (SDL_PollEvent(&event)) { }
    if (uses!=1 || DSI_EVENT_HEALTH_POTION==SDL_USEREVENT) return 0;
    puts("PASS: R healing-potion event: once per press, no mouse/shoulder leakage, held/remap/chord isolation, separate from progress events");
    return 1;
}
