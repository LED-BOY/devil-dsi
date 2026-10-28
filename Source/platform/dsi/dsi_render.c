#include "dsi_render.h"
#include "dsi_hot.h"

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
