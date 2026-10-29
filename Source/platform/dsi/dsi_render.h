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
#ifdef __cplusplus
}
#endif
#endif
