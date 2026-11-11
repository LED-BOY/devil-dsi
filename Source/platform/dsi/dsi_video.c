#include "dsi_video.h"
#include "dsi_hot.h"

/* Hardware VRAM permits aligned word stores as well as halfwords. may_alias
 * lets the host pixel tests back either access width with the same allocation. */
#if defined(__GNUC__)
typedef uint32_t WordPixels __attribute__((may_alias));
#else
typedef uint32_t WordPixels;
#endif

/* Separate overview/reader caches: alternating screens must not evict each
 * other's table. The engine calls these presenters on its single thread. */
struct ScaleTable {
    int width, height;
    unsigned x[DSI_WIDTH], y[DSI_HEIGHT];
};
static struct ScaleTable overview_table, reading_table;

static void prepare_table(struct ScaleTable *table, int width, int height)
{
    if (table->width == width && table->height == height) return;
    for (int x = 0; x < DSI_WIDTH; ++x)
        table->x[x] = (unsigned)dsi_source_coordinate(x, DSI_WIDTH, width);
    for (int y = 0; y < DSI_HEIGHT; ++y)
        table->y[y] = (unsigned)dsi_source_coordinate(y, DSI_HEIGHT, height);
    table->width = width;
    table->height = height;
}

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

static inline void copy_native_row(const uint8_t *row, volatile uint16_t *output)
{
    if ((((uintptr_t)row | (uintptr_t)output) & 3) == 0) {
        const WordPixels *source = (const WordPixels *)row;
        volatile WordPixels *words = (volatile WordPixels *)output;
#if defined(ARM9) && defined(__arm__) && !defined(__thumb__)
        const WordPixels *end = source + DSI_WIDTH / 4;
        /* Both addresses are word-aligned. Batch eight words without
         * introducing byte stores to background VRAM. */
        __asm__ volatile (
            "1:\n\t"
            "ldmia %[src]!, {r0-r3}\n\t"
            "stmia %[dst]!, {r0-r3}\n\t"
            "ldmia %[src]!, {r0-r3}\n\t"
            "stmia %[dst]!, {r0-r3}\n\t"
            "cmp %[src], %[end]\n\t"
            "bne 1b"
            : [src] "+&r"(source), [dst] "+&r"(words)
            : [end] "r"(end)
            : "r0", "r1", "r2", "r3", "cc", "memory");
#else
        for (int x = 0; x < DSI_WIDTH / 4; ++x)
            words[x] = source[x];
#endif
    } else if (((uintptr_t)output & 3) == 0) {
        volatile WordPixels *words = (volatile WordPixels *)output;
        for (int x = 0; x < DSI_WIDTH; x += 4)
            words[x / 4] = (uint32_t)row[x] | (uint32_t)row[x + 1] << 8
                | (uint32_t)row[x + 2] << 16 | (uint32_t)row[x + 3] << 24;
    } else {
        for (int x = 0; x < DSI_WIDTH; x += 2)
            output[x / 2] = (uint16_t)(row[x] | (uint16_t)row[x + 1] << 8);
    }
}

static inline void copy_scaled_row(const uint8_t *row, const unsigned *sample_x, volatile uint16_t *output)
{
    if (((uintptr_t)output & 3) == 0) {
        volatile WordPixels *words = (volatile WordPixels *)output;
        for (int x = 0; x < DSI_WIDTH; x += 4)
            words[x / 4] = (uint32_t)row[sample_x[x]]
                | (uint32_t)row[sample_x[x + 1]] << 8
                | (uint32_t)row[sample_x[x + 2]] << 16
                | (uint32_t)row[sample_x[x + 3]] << 24;
    } else {
        for (int x = 0; x < DSI_WIDTH; x += 2)
            output[x / 2] = (uint16_t)(row[sample_x[x]] | (uint16_t)row[sample_x[x + 1]] << 8);
    }
}

DSI_HOT int dsi_present_index8(const uint8_t *source, int width, int height,
    size_t source_pitch, volatile uint16_t *destination, size_t destination_pitch)
{
    if (source == NULL || destination == NULL || width <= 0 || height <= 0
        || source_pitch < (size_t)width || destination_pitch < DSI_WIDTH / 2)
        return 0;
    /* Reject overflowing offsets before calculating any source/destination row. */
    if ((size_t)(height - 1) > (SIZE_MAX - (size_t)(width - 1)) / source_pitch
        || destination_pitch > (SIZE_MAX / sizeof(uint16_t) - DSI_WIDTH / 2)
                / (DSI_HEIGHT - 1))
        return 0;
    if (width == DSI_WIDTH && height == DSI_HEIGHT) {
        for (int y = 0; y < DSI_HEIGHT; ++y)
            copy_native_row(source + (size_t)y * source_pitch, destination + (size_t)y * destination_pitch);
        return 1;
    }
    prepare_table(&overview_table, width, height);
    for (int y = 0; y < DSI_HEIGHT; ++y)
        copy_scaled_row(source + (size_t)overview_table.y[y] * source_pitch,
            overview_table.x, destination + (size_t)y * destination_pitch);
    return 1;
}

DSI_HOT int dsi_present_game_index8(const uint8_t *canvas, unsigned pitch,
    const uint8_t *scene, unsigned rows, volatile uint16_t *destination, size_t destination_pitch)
{
    if (!canvas || !scene || !destination || rows > DSI_HEIGHT || pitch < 640
        || (size_t)pitch > (SIZE_MAX - 639) / 479
        || destination_pitch < DSI_WIDTH / 2
        || destination_pitch > (SIZE_MAX / sizeof(uint16_t) - DSI_WIDTH / 2) / (DSI_HEIGHT - 1))
        return 0;
    for (unsigned y = 0; y < rows; ++y)
        copy_native_row(scene + y * DSI_WIDTH, destination + y * destination_pitch);
    prepare_table(&overview_table, 640, 480);
    for (unsigned y = rows; y < DSI_HEIGHT; ++y)
        copy_scaled_row(canvas + (size_t)overview_table.y[y] * pitch,
            overview_table.x, destination + y * destination_pitch);
    return 1;
}

uint16_t dsi_palette_color(uint8_t red, uint8_t green, uint8_t blue)
{
    return (uint16_t)((red >> 3) | (green >> 3) << 5 | (blue >> 3) << 10);
}

DSI_HOT int dsi_present_reading_index8(const uint8_t *source, int width, int height,
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
    prepare_table(&reading_table, crop_width, crop_height);
    for (int y = 0; y < DSI_HEIGHT; ++y) {
        int sy = top + (int)reading_table.y[y];
        const uint8_t *row = source + (size_t)sy * source_pitch + left;
        volatile uint16_t *output = destination + (size_t)y * destination_pitch;
        if (((uintptr_t)output & 3) == 0) {
            volatile WordPixels *words = (volatile WordPixels *)output;
            for (int x = 0; x < DSI_WIDTH; x += 4)
                words[x / 4] = (uint32_t)row[reading_table.x[x]]
                    | (uint32_t)row[reading_table.x[x + 1]] << 8
                    | (uint32_t)row[reading_table.x[x + 2]] << 16
                    | (uint32_t)row[reading_table.x[x + 3]] << 24;
            continue;
        }
        for (int x = 0; x < DSI_WIDTH; x += 2) {
            unsigned sx0 = reading_table.x[x], sx1 = reading_table.x[x + 1];
            destination[(size_t)y * destination_pitch + x / 2]
                = (uint16_t)(row[sx0] | (uint16_t)row[sx1] << 8);
        }
    }
    return 1;
}
