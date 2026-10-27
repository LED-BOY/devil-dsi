#include "dsi_video.h"

int dsi_source_coordinate(int coordinate, int screen_extent, int source_extent)
{
    if (screen_extent <= 0 || source_extent <= 0)
        return -1;
    if (coordinate < 0)
        coordinate = 0;
    if (coordinate >= screen_extent)
        coordinate = screen_extent - 1;
    return (int)((uint64_t)(unsigned)coordinate * (unsigned)source_extent
        / (unsigned)screen_extent);
}

int dsi_present_index8(const uint8_t *source, int width, int height,
    size_t source_pitch, volatile uint16_t *destination, size_t destination_pitch)
{
    unsigned x_map[DSI_WIDTH];
    int x, y;
    if (source == NULL || destination == NULL || width <= 0 || height <= 0
        || source_pitch < (size_t)width || destination_pitch < DSI_WIDTH / 2)
        return 0;
    /* Reject overflowing offsets before calculating any source/destination row. */
    if ((size_t)(height - 1) > (SIZE_MAX - (size_t)(width - 1)) / source_pitch
        || destination_pitch > (SIZE_MAX / sizeof(uint16_t) - DSI_WIDTH / 2)
                / (DSI_HEIGHT - 1))
        return 0;
    for (x = 0; x < DSI_WIDTH; ++x)
        x_map[x] = (unsigned)dsi_source_coordinate(x, DSI_WIDTH, width);
    for (y = 0; y < DSI_HEIGHT; ++y) {
        const uint8_t *row = source
            + (size_t)dsi_source_coordinate(y, DSI_HEIGHT, height) * source_pitch;
        volatile uint16_t *output = destination + (size_t)y * destination_pitch;
        for (x = 0; x < DSI_WIDTH; x += 2)
            output[x / 2] = (uint16_t)(row[x_map[x]] | (uint16_t)row[x_map[x + 1]] << 8);
    }
    return 1;
}

uint16_t dsi_palette_color(uint8_t red, uint8_t green, uint8_t blue)
{
    return (uint16_t)((red >> 3) | (green >> 3) << 5 | (blue >> 3) << 10);
}
