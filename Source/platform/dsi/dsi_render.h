#ifndef DEVILUTIONX_DSI_RENDER_H
#define DEVILUTIONX_DSI_RENDER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Byte kernels accept arbitrary alignment and length, including zero.
 * src/dst must not overlap. Palette/table buffers remain read-only. */
void DsiBlitMapped(uint8_t *dst, const uint8_t *src, unsigned length, const uint8_t *map);
void DsiBlitBlended(uint8_t *dst, const uint8_t *src, unsigned length, const uint8_t table[256][256]);
void DsiBlitBlendedMapped(uint8_t *dst, const uint8_t *src, unsigned length, const uint8_t *map, const uint8_t table[256][256]);
void DsiFillBlended(uint8_t *dst, unsigned length, const uint8_t *row);
/* RAM-only 2x scene composition. Buffers must not overlap. Returns 0 without
 * writes on invalid dimensions/pitches. Supports odd destination extents. */
int DsiExpandScene2x(const uint8_t *source, int width, int height, unsigned source_pitch,
    uint8_t *destination, int destination_width, int destination_height, unsigned destination_pitch);
#ifdef __cplusplus
}
#endif
#endif
