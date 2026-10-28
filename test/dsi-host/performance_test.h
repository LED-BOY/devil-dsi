/* Scheduling/copy isolation uses the real driver with deterministic hardware ticks. */
static int CheckPerformance(SDL_Surface *screen)
{
    DsiConfigurePerformance(15, 0);
    DsiSetTopView(DSI_TOP_MENU);
    DsiSetTopView(DSI_TOP_MAP);
    if (!DsiMapNeedsRedraw()) return 0;
    DsiMapBuffer()[0] = 71;
    DsiMapReady();
    SDL_Flip(screen);
    if ((unsigned char)host_sub_vram[0] != 71) return 0;
    DsiMapBuffer()[0] = 99;
    ((unsigned char *)screen->pixels)[0] = 33;
    SDL_Flip(screen);
    if ((unsigned char)host_sub_vram[0] != 71 || (unsigned char)host_vram[0] != 33) return 0;
    host_ticks += 66000;
    if (DsiMapNeedsRedraw()) return 0;
    host_ticks += 1000;
    if (!DsiMapNeedsRedraw()) return 0;
    DsiMapReady(); SDL_Flip(screen);
    if ((unsigned char)host_sub_vram[0] != 99 || DsiMapNeedsRedraw()) return 0;
    DsiConfigurePerformance(10, 0);
    if (!DsiMapNeedsRedraw()) return 0;
    DsiMapReady(); host_ticks += 99000;
    if (DsiMapNeedsRedraw()) return 0;
    host_ticks += 1000;
    if (!DsiMapNeedsRedraw()) return 0;
    /* Unsigned millisecond rollover must preserve the deadline. */
    host_ticks = (uint64_t)0xfffffff0u * 1000;
    DsiMapReady(); host_ticks += 100000;
    if (!DsiMapNeedsRedraw()) return 0;
    DsiConfigurePerformance(0, 0); DsiMapReady();
    if (!DsiMapNeedsRedraw()) return 0;
    DsiConfigurePerformance(99, 0); DsiMapReady();
    host_ticks += 66000;
    if (DsiMapNeedsRedraw()) return 0;
    DsiConfigureOptions(200, 25, DSI_L_RIGHT_MOUSE, DSI_L_HEALTH_POTION);
    if (!DsiMapNeedsRedraw()) return 0;
    DsiMapReady();
    DsiSetLoading(1);
    if (DsiMapNeedsRedraw()) return 0;
    DsiSetLoading(0);
    if (!DsiMapNeedsRedraw()) return 0;
    DsiMapReady(); DsiSetMoviePlaying(1);
    if (DsiMapNeedsRedraw()) return 0;
    DsiSetMoviePlaying(0);
    if (!DsiMapNeedsRedraw()) return 0;
    DsiSetTopView(DSI_TOP_MENU);
    if (DsiMapNeedsRedraw()) return 0;
    DsiSetTopView(DSI_TOP_MAP);
    /* Real tick units are microseconds in this host shim. Skipped maps average
     * as zero, rather than reporting a misleading full-map cost per frame. */
    DsiConfigurePerformance(15, 1);
    for (int frame = 0; frame < 10; ++frame) {
        DsiPerfBeginFrame();
        uint64_t start = DsiPerfClock(); host_ticks += 20000;
        DsiPerfRecord(DSI_PERF_RENDER, start);
        if (frame % 2 == 0) {
            start = DsiPerfClock(); host_ticks += 2000;
            DsiPerfRecord(DSI_PERF_MAP, start);
        }
        start = DsiPerfClock(); host_ticks += 3000;
        DsiPerfRecord(DSI_PERF_BOTTOM, start);
        host_ticks += frame % 2 == 0 ? 25000 : 27000;
        DsiPerfEndFrame();
    }
    struct DsiPerfStats stats;
    DsiPerfGetStats(&stats);
    if (!DsiPerfEnabled() || stats.fps10 != 200
        || stats.microseconds[DSI_PERF_RENDER] != 20000
        || stats.microseconds[DSI_PERF_MAP] != 1000
        || stats.microseconds[DSI_PERF_BOTTOM] != 3000) return 0;
    DsiConfigurePerformance(15, 0);
    DsiPerfGetStats(&stats);
    if (DsiPerfEnabled() || DsiPerfClock() || stats.fps10) return 0;
    puts("PASS: map 10/15 Hz/every-frame deadlines, VRAM copy gating, bottom independence, settings/view/loading invalidation, tick wrap and profiling averages");
    return 1;
}

static int CheckScaleCache(void)
{
    static unsigned char pixels[648 * 480 + 1];
    static uint16_t output[132 * 192];
    const int sizes[][2] = { {640,480}, {256,192}, {320,240}, {1,1}, {641,479}, {640,480} };
    for (int choice = 0; choice < 6; ++choice) {
        int w = sizes[choice][0], h = sizes[choice][1], pitch = w + 3;
        unsigned char *source = pixels + 1; /* Deliberately unaligned/padded. */
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) source[y * pitch + x] = (unsigned char)(x * 3 + y * 7);
        for (int pass = 0; pass < 2; ++pass) {
            memset(output, 0x5a, sizeof(output));
            if (!dsi_present_index8(source,w,h,pitch,output,132)) return 0;
            for (int y = 0; y < 192; ++y) {
                for (int x = 0; x < 256; ++x)
                    if ((unsigned char)(output[y * 132 + x / 2] >> (8 * (x % 2)))
                        != (unsigned char)((x * w / 256) * 3 + (y * h / 192) * 7)) return 0;
                for (int x = 128; x < 132; ++x) if (output[y * 132 + x] != 0x5a5a) return 0;
            }
            /* Alternating reading zoom calls must not corrupt the overview cache. */
            if (!dsi_present_reading_index8(source,w,h,pitch,w/2,h/2,200,output,132)) return 0;
        }
    }
    puts("PASS: native map copy and scaling cache: all pixels, odd/padded/unaligned inputs, destination guards, mode changes and alternating reader");
    return 1;
}
