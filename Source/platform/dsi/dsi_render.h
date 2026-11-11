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
/* Bottom-anchored bounds. No sprite bytes are read for invisible rectangles. */
int DsiSpriteVisible(int x, int y, unsigned width, unsigned height, int outWidth, int outHeight);
/* Returns zero to use the original CLX renderer. Fixed ~200 KiB cache, no heap. */
int DsiDrawCachedClx(uint8_t *dst, unsigned pitch, int outWidth, int outHeight,
    int x, int y, const uint8_t *src, unsigned size, unsigned width, unsigned height, const uint8_t *map);
void DsiClearSpriteCache(void);
/* Sample the existing camera at the physical display grid during world drawing.
 * UI rendering is outside this scope. Retains a logical canvas for engine APIs. */
extern int DsiSceneSampling;
void DsiBeginSceneSamples(uint8_t *base, unsigned pitch, unsigned width, unsigned canvasHeight, unsigned viewHeight);
int DsiBeginPackedScene(uint8_t *base, unsigned pitch, unsigned width, unsigned height, unsigned viewHeight);
void DsiEndSceneSamples(int expandForReader);
const uint8_t *DsiPackedScene(const void *base, unsigned pitch, unsigned *rows);
void DsiRestorePackedScene(void);
void DsiDiscardPackedScene(void);
/* Copy the scene under the software cursor into its logical-size undo buffer. */
int DsiReadPackedSpan(uint8_t *dst, const uint8_t *logical, unsigned length);
int DsiSampleKeyedSpan(uint8_t *dst, const uint8_t *src, unsigned length);
int DsiSampleSpan(uint8_t *dst, const uint8_t *src, unsigned length, unsigned color,
    const uint8_t *map, const uint8_t blend[256][256], int destinationFirst);
#ifdef __cplusplus
}
#endif
#endif
