#ifndef DEVILUTIONX_DSI_VIDEO_H
#define DEVILUTIONX_DSI_VIDEO_H

#include <stddef.h>
#include <stdint.h>

enum { DSI_WIDTH = 256, DSI_HEIGHT = 192 };

#ifdef __cplusplus
extern "C" {
#endif

enum DsiTopView { DSI_TOP_DIAGNOSTICS, DSI_TOP_MENU, DSI_TOP_MAP, DSI_TOP_MOVIE };
enum DsiLAction { DSI_L_CONTROLLER, DSI_L_RIGHT_MOUSE, DSI_L_DISABLED, DSI_L_HEALTH_POTION };
/* Separate SDL 1.2 user slot from the engine's loading/progress event (24). */
enum { DSI_EVENT_HEALTH_POTION = 25 };
void DsiConfigureOptions(int reading_percent, int map_scale, int l_action, int r_action);
int DsiTopMapScale(void);
void DsiSetTopView(enum DsiTopView view);
void DsiSetMoviePlaying(int playing);
void DsiSetLoading(int loading);
void DsiRestoreDiagnostics(void);
void DsiSetReadingFocus(int x, int y);
uint8_t *DsiMapBuffer(void);
/* 0 = every frame; otherwise 10 or 15 Hz. Rendering and VRAM copies are gated. */
void DsiConfigurePerformance(int map_hz, int show_stats);
int DsiMapNeedsRedraw(void);
void DsiMapReady(void);
enum DsiPerfStage { DSI_PERF_RENDER, DSI_PERF_MAP, DSI_PERF_BOTTOM, DSI_PERF_TOP, DSI_PERF_COUNT };
struct DsiPerfStats { unsigned fps10, microseconds[DSI_PERF_COUNT]; };
int DsiPerfEnabled(void);
uint64_t DsiPerfClock(void);
void DsiPerfBeginFrame(void);
void DsiPerfRecord(enum DsiPerfStage stage, uint64_t start);
void DsiPerfEndFrame(void);
void DsiPerfGetStats(struct DsiPerfStats *stats);

/* Coordinates use the same nearest-neighbour transform as the presenter. */
int dsi_source_coordinate(int screen_coordinate, int screen_extent, int source_extent);

/* Destination pitch is measured in 16-bit words. The source remains in RAM.
 * Paired stores are deliberate: DS background VRAM does not support byte writes.
 * Returns zero for invalid arguments, without touching the destination. */
int dsi_present_index8(const uint8_t *source, int width, int height,
    size_t source_pitch, volatile uint16_t *destination, size_t destination_pitch);

uint16_t dsi_palette_color(uint8_t red, uint8_t green, uint8_t blue);

/* Zoom relative to the complete bottom overview: 150, 200 or 250 percent.
 * The source window is clamped to the source, including very small modes. */
int dsi_present_reading_index8(const uint8_t *source, int width, int height,
    size_t source_pitch, int center_x, int center_y, int zoom_percent,
    volatile uint16_t *destination, size_t destination_pitch);

#ifdef __cplusplus
}
#endif

#endif
