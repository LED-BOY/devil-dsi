#ifndef DEVILUTIONX_DSI_VIDEO_H
#define DEVILUTIONX_DSI_VIDEO_H

#include <stddef.h>
#include <stdint.h>

enum { DSI_WIDTH = 256, DSI_HEIGHT = 192 };

#ifdef __cplusplus
extern "C" {
#endif

enum DsiTopView { DSI_TOP_DIAGNOSTICS, DSI_TOP_MENU, DSI_TOP_MAP };
enum DsiLAction { DSI_L_CONTROLLER, DSI_L_RIGHT_MOUSE, DSI_L_DISABLED };
void DsiConfigureOptions(int reading_percent, int map_scale, int l_action);
int DsiTopMapScale(void);
void DsiSetTopView(enum DsiTopView view);
void DsiSetReadingFocus(int x, int y);
uint8_t *DsiMapBuffer(void);

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
