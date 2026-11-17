#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dsi_render.h"
#include "dsi_video.h"

enum { Width = 640, Height = 480, Pitch = 648, VramPitch = 130 };
static uint8_t canvas[Pitch * Height], reference[Pitch * Height];
static uint8_t expected[DSI_WIDTH * DSI_HEIGHT];
static uint8_t source[Width], map[256], blend[256][256];
static uint16_t output[VramPitch * DSI_HEIGHT];
static uint32_t random_state = 17;

static unsigned next_random(void)
{
    random_state = random_state * 1664525u + 1013904223u;
    return random_state >> 8;
}

static int check_mode(int mode, unsigned view_height)
{
    const unsigned repeat_x = mode == 2 ? 2 : 1;
    const unsigned repeat_y = mode == 1 || mode == 2 ? 2 : 1;
    const unsigned sample_width = DSI_WIDTH / repeat_x;
    const unsigned sample_height = DSI_HEIGHT / repeat_y;
    const unsigned native_rows = (view_height * DSI_HEIGHT + Height - 1) / Height;

    DsiDiscardPackedScene();
    memset(canvas, 0xa5, sizeof(canvas));
    for (unsigned y = 0; y < Height; ++y) {
        for (unsigned x = 0; x < Width; ++x) {
            reference[y * Pitch + x] = (x * 13 + y * 7) & 255;
            if (y >= view_height)
                canvas[y * Pitch + x] = reference[y * Pitch + x];
        }
    }
    if (!DsiBeginPackedScene(canvas, Pitch, Width, Height, view_height)) return 1;
    for (unsigned y = 0; y < view_height; ++y)
        if (!DsiSampleSpan(canvas + y * Pitch, reference + y * Pitch, Width, 0, NULL, NULL, 0)) return 2;
    DsiBeginWorldSamples(mode);

    for (unsigned n = 0; n < 4000; ++n) {
        const unsigned x = next_random() % Width;
        const unsigned y = next_random() % view_height;
        const unsigned length = 1 + next_random() % (Width - x);
        const unsigned kind = next_random() % 6;
        const unsigned color = next_random() & 255;
        const uint8_t *pixels = kind == 0 ? NULL : source;
        const uint8_t *translation = kind == 2 || kind == 4 ? map : NULL;
        const uint8_t (*mix)[256] = kind == 3 || kind == 4 ? blend : NULL;
        for (unsigned i = 0; i < length; ++i) {
            uint8_t value = pixels ? pixels[i] : color;
            uint8_t *dst = reference + y * Pitch + x + i;
            if (kind == 5 && value == 0) continue;
            if (translation) value = translation[value];
            *dst = mix ? (kind == 4 ? mix[*dst][value] : mix[value][*dst]) : value;
        }
        uint8_t *dst = canvas + y * Pitch + x;
        if (kind == 5) {
            if (!DsiSampleKeyedSpan(dst, pixels, length)) return 3;
        } else if (!DsiSampleSpan(dst, pixels, length, color, translation, mix, kind == 4)) {
            return 4;
        }
    }

    /* Exercise clipped, cached rows whose visibility changes with the grid. */
    uint8_t sprite[16];
    for (unsigned row = 0; row < 8; ++row) {
        sprite[row * 2] = 175; /* Opaque fill of 16 pixels. */
        sprite[row * 2 + 1] = 20 + row;
    }
    DsiClearSpriteCache();
    for (int bottom = 0; bottom < 20; ++bottom) {
        const int left = bottom - 10;
        DsiDrawCachedClx(canvas, Pitch, Width, view_height, left, bottom,
            sprite, sizeof(sprite), 16, 8, map);
        if (!DsiDrawCachedClx(canvas, Pitch, Width, view_height, left, bottom,
                sprite, sizeof(sprite), 16, 8, map)) return 5;
        for (int row = 0; row < 8; ++row) {
            const int y = bottom - row;
            if (y < 0 || y >= (int)view_height) continue;
            for (int col = 0; col < 16; ++col)
                if (left + col >= 0)
                    reference[y * Pitch + left + col] = map[20 + row];
        }
    }

    DsiEndWorldSamples();
    for (unsigned y = 0; y < native_rows; ++y) {
        const unsigned sy = (y / repeat_y) * Height / sample_height;
        for (unsigned x = 0; x < DSI_WIDTH; ++x) {
            const unsigned sx = (x / repeat_x) * Width / sample_width;
            expected[y * DSI_WIDTH + x] = reference[sy * Pitch + sx];
        }
    }

    /* Overlay samples must use all native rows and columns after expansion.
     * In Fastest mode these odd rows/columns were absent from world drawing. */
    if (view_height > 2) {
        if (!DsiSampleSpan(canvas + 2 * Pitch, source, Width, 0, NULL, NULL, 0)) return 6;
        for (unsigned x = 0; x < DSI_WIDTH; ++x)
            expected[DSI_WIDTH + x] = source[x * Width / DSI_WIDTH];
    }
    if (!DsiSampleSpan(canvas + 637, NULL, 1, 193, NULL, NULL, 0)) return 7;
    expected[255] = 193;
    DsiEndSceneSamples(0);

    unsigned rows;
    const uint8_t *scene = DsiPackedScene(canvas, Pitch, &rows);
    if (!scene || rows != native_rows || memcmp(scene, expected, rows * DSI_WIDTH)) return 8;
    for (unsigned y = 0; y < view_height; ++y)
        for (unsigned x = 0; x < Pitch; ++x)
            if (canvas[y * Pitch + x] != 0xa5) return 9;

    memset(output, 0xb3, sizeof(output));
    if (!dsi_present_game_index8(canvas, Pitch, scene, rows, output, VramPitch)) return 10;
    for (unsigned y = 0; y < DSI_HEIGHT; ++y) {
        for (unsigned x = 0; x < DSI_WIDTH; ++x) {
            const uint8_t want = y < rows ? expected[y * DSI_WIDTH + x]
                : reference[(y * Height / DSI_HEIGHT) * Pitch + x * Width / DSI_WIDTH];
            const uint8_t got = output[y * VramPitch + x / 2] >> (8 * (x & 1));
            if (want != got) return 11;
        }
        if (output[y * VramPitch + 128] != 0xb3b3 || output[y * VramPitch + 129] != 0xb3b3) return 12;
    }
    DsiRestorePackedScene();
    return 0;
}

int main(void)
{
    for (unsigned i = 0; i < Width; ++i) source[i] = i % 7 == 0 ? 0 : (i * 11) & 255;
    for (unsigned i = 0; i < 256; ++i) {
        map[i] = 255 - i;
        for (unsigned j = 0; j < 256; ++j) blend[i][j] = (i * 3 + j * 7) & 255;
    }
    const unsigned heights[] = { 1, 3, 351, 352, 479, 480 };
    const int modes[] = { 1, 2, 0, 2, 1, 99, -1 };
    for (unsigned i = 0; i < sizeof(heights) / sizeof(heights[0]); ++i) {
        for (unsigned j = 0; j < sizeof(modes) / sizeof(modes[0]); ++j) {
            const int result = check_mode(modes[j], heights[i]);
            if (result) {
                fprintf(stderr, "FAIL: fast mode %d, viewport %u, check %d\n", modes[j], heights[i], result);
                return result;
            }
        }
    }
    /* Menu reader uses the full logical grid even if fast drawing is requested. */
    memset(canvas, 0, sizeof(canvas));
    DsiBeginSceneSamples(canvas, Pitch, Width, Height, 352);
    DsiBeginWorldSamples(2);
    if (!DsiSampleSpan(canvas + 2 * Pitch, NULL, Width, 77, NULL, NULL, 0)) return 13;
    DsiEndSceneSamples(1);
    if (canvas[2 * Pitch] != 77 || canvas[2 * Pitch + 637] != 77) return 14;
    puts("PASS: R9 fast drawing modes match sampled reference over 168000 spans, cached sprites, clipped viewports, native overlays, HUD and menu fallback");
    return 0;
}
