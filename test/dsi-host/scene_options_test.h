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
