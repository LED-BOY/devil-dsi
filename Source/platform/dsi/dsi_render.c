#include "dsi_render.h"
#include "dsi_hot.h"
#include <stddef.h>
#include <string.h>

DSI_HOT void DsiBlitMapped(uint8_t *restrict dst, const uint8_t *restrict src,
    unsigned length, const uint8_t *restrict map)
{
    while (length >= 4) {
        dst[0] = map[src[0]]; dst[1] = map[src[1]];
        dst[2] = map[src[2]]; dst[3] = map[src[3]];
        dst += 4; src += 4; length -= 4;
    }
    while (length--) *dst++ = map[*src++];
}

DSI_HOT void DsiBlitBlended(uint8_t *restrict dst, const uint8_t *restrict src,
    unsigned length, const uint8_t table[256][256])
{
    while (length >= 4) {
        dst[0] = table[src[0]][dst[0]]; dst[1] = table[src[1]][dst[1]];
        dst[2] = table[src[2]][dst[2]]; dst[3] = table[src[3]][dst[3]];
        dst += 4; src += 4; length -= 4;
    }
    while (length--) { *dst = table[*src++][*dst]; ++dst; }
}

DSI_HOT void DsiBlitBlendedMapped(uint8_t *restrict dst, const uint8_t *restrict src,
    unsigned length, const uint8_t *restrict map, const uint8_t table[256][256])
{
    /* Preserve the existing renderer's destination-first table orientation. */
    while (length >= 4) {
        dst[0] = table[dst[0]][map[src[0]]]; dst[1] = table[dst[1]][map[src[1]]];
        dst[2] = table[dst[2]][map[src[2]]]; dst[3] = table[dst[3]][map[src[3]]];
        dst += 4; src += 4; length -= 4;
    }
    while (length--) { *dst = table[*dst][map[*src++]]; ++dst; }
}

DSI_HOT void DsiFillBlended(uint8_t *dst, unsigned length, const uint8_t *row)
{
    while (length >= 4) {
        dst[0] = row[dst[0]]; dst[1] = row[dst[1]];
        dst[2] = row[dst[2]]; dst[3] = row[dst[3]];
        dst += 4; length -= 4;
    }
    while (length--) { *dst = row[*dst]; ++dst; }
}

DSI_HOT int DsiExpandScene2x(const uint8_t *source, int width, int height, unsigned source_pitch,
    uint8_t *destination, int destination_width, int destination_height, unsigned destination_pitch)
{
    if (!source || !destination || width <= 0 || height <= 0
        || destination_width <= 0 || destination_height <= 0
        || source_pitch < (unsigned)width || destination_pitch < (unsigned)destination_width
        || ((unsigned)destination_width + 1) / 2 > (unsigned)width
        || ((unsigned)destination_height + 1) / 2 > (unsigned)height)
        return 0;
    if ((size_t)(height - 1) > (SIZE_MAX - (unsigned)width) / source_pitch
        || (size_t)(destination_height - 1) > (SIZE_MAX - (unsigned)destination_width) / destination_pitch)
        return 0;
    for (int y = 0; y < destination_height; y += 2) {
        const uint8_t *src = source + (size_t)(y / 2) * source_pitch;
        uint8_t *dst = destination + (size_t)y * destination_pitch;
        int x = 0;
        for (; x + 1 < destination_width; x += 2)
            dst[x] = dst[x + 1] = src[x / 2];
        if (x < destination_width) dst[x] = src[x / 2];
        if (y + 1 < destination_height) memcpy(dst + destination_pitch, dst, destination_width);
    }
    return 1;
}
