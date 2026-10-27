#ifndef DEVILUTIONX_DSI_VIDEO_H
#define DEVILUTIONX_DSI_VIDEO_H

#include <stddef.h>
#include <stdint.h>

enum { DSI_WIDTH = 256, DSI_HEIGHT = 192 };

/* Coordinates use the same nearest-neighbour transform as the presenter. */
int dsi_source_coordinate(int screen_coordinate, int screen_extent, int source_extent);

/* Destination pitch is measured in 16-bit words. The source remains in RAM.
 * Paired stores are deliberate: DS background VRAM does not support byte writes.
 * Returns zero for invalid arguments, without touching the destination. */
int dsi_present_index8(const uint8_t *source, int width, int height,
    size_t source_pitch, volatile uint16_t *destination, size_t destination_pitch);

uint16_t dsi_palette_color(uint8_t red, uint8_t green, uint8_t blue);

#endif
