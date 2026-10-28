static int CheckSceneComposition(void)
{
    static uint8_t src[320*240], canvas[640*480];
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x) src[y*320+x] = (uint8_t)(x*3+y*7);
    /* Full view and each side-panel layout; HUD rows must remain untouched. */
    for (int panel = 0; panel < 3; ++panel) {
        int dx = panel == 1 ? 320 : 0;
        int w = panel ? 320 : 640;
        memset(canvas,99,sizeof(canvas));
        if (!DsiExpandScene2x(src,320,176,320,canvas+dx,w,352,640)) return 0;
        for (int y = 0; y < 480; ++y)
            for (int x = 0; x < 640; ++x) {
                uint8_t expected = y < 352 && x >= dx && x < dx+w
                    ? src[(y/2)*320+(x-dx)/2] : 99;
                if (canvas[y*640+x] != expected) return 0;
            }
    }
    memset(canvas,99,sizeof(canvas));
    if (!DsiExpandScene2x(src,320,240,320,canvas,639,479,640)) return 0;
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 640; ++x) {
            uint8_t expected = y < 479 && x < 639 ? src[(y/2)*320+x/2] : 99;
            if (canvas[y*640+x] != expected) return 0;
        }
    memset(canvas,99,sizeof(canvas));
    if (DsiExpandScene2x(NULL,320,240,320,canvas,640,480,640)
        || DsiExpandScene2x(src,320,240,319,canvas,640,480,640)
        || DsiExpandScene2x(src,320,240,320,canvas,641,480,641)
        || DsiExpandScene2x(src,320,240,320,canvas,640,481,640)
        || DsiExpandScene2x(src,320,240,320,canvas,640,480,639)) return 0;
    for (int i = 0; i < sizeof(canvas); ++i) if (canvas[i] != 99) return 0;
    puts("PASS: 320x240 scene composition: every pixel, full/left/right-panel viewports, HUD preservation, odd extents and invalid bounds");
    return 1;
}

static int CheckMapOff(SDL_Surface *screen)
{
    DsiConfigurePerformance(15,0);
    DsiSetTopView(DSI_TOP_MAP);
    DsiConfigureMap(0);
    host_sub_vram[0] = 0x1234;
    DsiMapBuffer()[0] = 37;
    if (DsiTopMapEnabled() || DsiMapNeedsRedraw()) return 0;
    SDL_Flip(screen);
    SDL_Color color = {255,255,255,0}; SDL_SetColors(screen,&color,0,1);
    if (host_visible_backgrounds & ((1u<<4)|(1u<<7))
        || host_sub_palette[0] || host_sub_vram[0] != 0x1234) return 0;
    /* Menu reader still works, then returns to a black gameplay top screen. */
    DsiSetTopView(DSI_TOP_MENU); SDL_Flip(screen);
    if (!(host_visible_backgrounds & (1u<<7))) return 0;
    DsiSetTopView(DSI_TOP_MAP);
    if (host_visible_backgrounds & ((1u<<4)|(1u<<7))) return 0;
    DsiSetLoading(1); DsiSetLoading(0);
    DsiSetMoviePlaying(1); DsiSetMoviePlaying(0);
    if (host_visible_backgrounds & ((1u<<4)|(1u<<7)) || DsiMapNeedsRedraw()) return 0;
    /* Stats are an independent opt-in layer: black buffer, no automap, 2 Hz. */
    DsiConfigurePerformance(15,1);
    SDL_SetColors(screen,&color,0,1);
    if (host_sub_palette[0]) return 0;
    if (!DsiMapNeedsRedraw()) return 0;
    memset(DsiMapBuffer(),0,256*192); DsiMapReady(); SDL_Flip(screen);
    if (!(host_visible_backgrounds & (1u<<7)) || host_sub_vram[0]) return 0;
    host_ticks += 499000;
    if (DsiMapNeedsRedraw()) return 0;
    host_ticks += 1000;
    if (!DsiMapNeedsRedraw()) return 0;
    DsiConfigurePerformance(15,0);
    if (host_visible_backgrounds & ((1u<<4)|(1u<<7))) return 0;
    DsiConfigureMap(1);
    if (!DsiMapNeedsRedraw() || !(host_visible_backgrounds & (1u<<7))) return 0;
    DsiSetTopView(DSI_TOP_DIAGNOSTICS);
    if (!(host_visible_backgrounds & (1u<<4)) || host_sub_palette[0] != 0x7000) return 0;
    puts("PASS: map off: no redraw/copy, black palette-safe gameplay, menu reader, loading/movie restoration, independent stats and re-enable");
    return 1;
}
