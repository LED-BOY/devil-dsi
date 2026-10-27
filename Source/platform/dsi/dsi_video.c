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

int dsi_present_reading_index8(const uint8_t *source, int width, int height,
    size_t source_pitch, int center_x, int center_y, int zoom_percent,
    volatile uint16_t *destination, size_t destination_pitch)
{
    if (source == NULL || destination == NULL || width <= 0 || height <= 0
        || source_pitch < (size_t)width || destination_pitch < DSI_WIDTH / 2)
        return 0;
    if ((size_t)(height - 1) > (SIZE_MAX - (size_t)(width - 1)) / source_pitch
        || destination_pitch > (SIZE_MAX / sizeof(uint16_t) - DSI_WIDTH / 2)
                / (DSI_HEIGHT - 1))
        return 0;
    if (zoom_percent != 150 && zoom_percent != 200 && zoom_percent != 250)
        return 0;
    int crop_width = (int)((uint64_t)(unsigned)width * 100 / zoom_percent);
    int crop_height = (int)((uint64_t)(unsigned)height * 100 / zoom_percent);
    if (crop_width < 1) crop_width = 1;
    if (crop_height < 1) crop_height = 1;
    /* Clamp centers before subtracting to avoid overflow for arbitrary inputs. */
    if (center_x < 0) center_x = 0;
    if (center_x > width) center_x = width;
    if (center_y < 0) center_y = 0;
    if (center_y > height) center_y = height;
    int left = center_x - crop_width / 2;
    int top = center_y - crop_height / 2;
    if (left > width - crop_width) left = width - crop_width;
    if (top > height - crop_height) top = height - crop_height;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    unsigned x_map[DSI_WIDTH];
    for (int x = 0; x < DSI_WIDTH; ++x)
        x_map[x] = (unsigned)(left + dsi_source_coordinate(x, DSI_WIDTH, crop_width));
    for (int y = 0; y < DSI_HEIGHT; ++y) {
        int sy = top + dsi_source_coordinate(y, DSI_HEIGHT, crop_height);
        const uint8_t *row = source + (size_t)sy * source_pitch;
        for (int x = 0; x < DSI_WIDTH; x += 2) {
            unsigned sx0 = x_map[x], sx1 = x_map[x + 1];
            destination[(size_t)y * destination_pitch + x / 2]
                = (uint16_t)(row[sx0] | (uint16_t)row[sx1] << 8);
        }
    }
    return 1;
}
