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

#include <string.h>
#include <stddef.h>

#define SPRITE_CACHE_SLOTS 16
int DsiSceneSampling;
static uint8_t *scene_base;
static unsigned scene_pitch, scene_height;
static uint16_t scene_next_x[641], scene_x[257], scene_y[193];
static uint8_t scene_row[480];

#define SPRITE_CACHE_BYTES 4096
#define SPRITE_CACHE_RUNS 512
/* Raw source comparisons handle allocations reused at the same address, and
 * in-place TRN edits. Palette contents (not their address) validate colors. */
typedef struct { uint16_t x, y, offset; uint8_t length, fill; } SpriteRun;
typedef struct {
    const uint8_t *key;
    unsigned size, width, height, age, count;
    int ready, mapped;
    uint8_t source[SPRITE_CACHE_BYTES], colors[SPRITE_CACHE_BYTES], map[256];
    SpriteRun runs[SPRITE_CACHE_RUNS];
    uint16_t rowEnd[256];
} SpriteCache;
static SpriteCache sprite_cache[SPRITE_CACHE_SLOTS];
static unsigned sprite_age;

int DsiSpriteVisible(int x, int y, unsigned width, unsigned height, int outWidth, int outHeight)
{
    return width && height && outWidth > 0 && outHeight > 0
        && x < outWidth && y >= 0
        && (int64_t)x + width > 0 && (int64_t)y - height + 1 < outHeight;
}

void DsiClearSpriteCache(void)
{
    unsigned i;
    for (i = 0; i < SPRITE_CACHE_SLOTS; ++i) sprite_cache[i].key = NULL;
    sprite_age = 0;
}

static int PrepareSpriteRuns(SpriteCache *entry, const uint8_t *src)
{
    unsigned offset = 0, pixel = 0, count = 0;
    const unsigned total = entry->width * entry->height;
    while (offset < entry->size) {
        unsigned control = src[offset++], length, fill, data;
        if (control < 128) {
            if (!control || control > total - pixel) return 0;
            pixel += control;
            continue;
        }
        fill = control <= 190;
        length = fill ? 191 - control : 256 - control;
        data = offset;
        if (length > total - pixel || offset + (fill ? 1 : length) > entry->size) return 0;
        offset += fill ? 1 : length;
        while (length) {
            unsigned x = pixel % entry->width;
            unsigned n = entry->width - x;
            SpriteRun *run;
            if (n > length) n = length;
            if (count == SPRITE_CACHE_RUNS) return 0;
            run = &entry->runs[count++];
            run->x = x; run->y = pixel / entry->width;
            run->offset = data; run->length = n; run->fill = fill;
            pixel += n; length -= n;
            if (!fill) data += n;
        }
    }
    if (pixel != total) return 0;
    for (unsigned i = 0; i < count; ++i)
        entry->rowEnd[entry->runs[i].y] = i + 1;
    entry->count = count;
    memcpy(entry->source, src, entry->size);
    entry->mapped = 0;
    return 1;
}

int DsiDrawCachedClx(uint8_t *dst, unsigned pitch, int outWidth, int outHeight,
    int x, int y, const uint8_t *src, unsigned size, unsigned width, unsigned height, const uint8_t *map)
{
    SpriteCache *entry, *a, *b;
    unsigned bucket, i;
    const uint8_t *colors;
    if (!DsiSpriteVisible(x, y, width, height, outWidth, outHeight)) return 1;
    if (!src || !dst || !size || size > SPRITE_CACHE_BYTES || width > 256 || height > 256
        || pitch < (unsigned)outWidth) return 0;
    bucket = (((uintptr_t)src >> 4) ^ ((uintptr_t)src >> 12)) % (SPRITE_CACHE_SLOTS / 2) * 2;
    a = &sprite_cache[bucket]; b = a + 1;
    entry = a->key == src ? a : b->key == src ? b : NULL;
    if (!entry || entry->size != size || entry->width != width || entry->height != height) {
        if (!entry) entry = !a->key || a->age <= b->age ? a : b;
        entry->key = src; entry->size = size; entry->width = width; entry->height = height;
        entry->age = ++sprite_age; entry->ready = 0;
        return 0; /* Admit only on repeat use; one-shot sprites use the original path. */
    }
    entry->age = ++sprite_age;
    if (entry->ready && memcmp(entry->source, src, size)) entry->ready = 0;
    if (entry->ready < 0) return 0;
    if (!entry->ready) {
        if (!PrepareSpriteRuns(entry, src)) {
            memcpy(entry->source, src, size);
            entry->ready = -1; /* Remember oversized run lists until source changes. */
            return 0;
        }
        entry->ready = 1;
    }
    colors = entry->source;
    if (map) {
        if (!entry->mapped || memcmp(entry->map, map, 256)) {
            memcpy(entry->map, map, 256);
            for (i = 0; i < size; ++i) entry->colors[i] = map[entry->source[i]];
            entry->mapped = 1;
        }
        colors = entry->colors;
    }
    const int sampleScene = DsiSceneSampling && dst == scene_base
        && pitch == scene_pitch && outWidth == 640 && outHeight <= (int)scene_height;
    for (i = 0; i < entry->count; ++i) {
        const SpriteRun *run = &entry->runs[i];
        int yy = y - run->y, left, right;
        unsigned skip;
        uint8_t *out;
        if (yy < 0) break;
        if (yy >= outHeight || (sampleScene && !scene_row[yy])) {
            i = entry->rowEnd[run->y] - 1;
            continue;
        }
        left = x + run->x; right = left + run->length;
        if (right <= 0 || left >= outWidth) continue;
        skip = left < 0 ? (unsigned)-left : 0;
        if (left < 0) left = 0;
        if (right > outWidth) right = outWidth;
        out = dst + (size_t)yy * pitch + left;
        if (sampleScene) {
            /* The sprite cache already knows the row and column. */
            for (unsigned at = scene_next_x[left]; at < (unsigned)right; at = scene_next_x[at + 1]) {
                const unsigned sourceOffset = run->offset + (run->fill ? 0 : skip + at - left);
                out[at - left] = colors[sourceOffset];
            }
            continue;
        }
        if (DsiSceneSampling && DsiSampleSpan(out, run->fill ? NULL : colors + run->offset + skip,
                right - left, colors[run->offset], NULL, NULL, 0)) continue;
        if (run->fill) memset(out, colors[run->offset], right - left);
        else memcpy(out, colors + run->offset + skip, right - left);
    }
    return 1;
}

/* Keep camera coordinates intact: select exactly the source coordinates used
 * by the 256x192 presenter instead of changing tile geometry or viewport FOV. */
void DsiBeginSceneSamples(uint8_t *base, unsigned pitch, unsigned width, unsigned canvasHeight, unsigned viewHeight)
{
    unsigned i, j;
    DsiSceneSampling = 0;
    if (width != 640 || canvasHeight != 480 || viewHeight > 480 || pitch < 640) return;
    scene_base = base; scene_pitch = pitch; scene_height = viewHeight;
    memset(scene_row, 0, sizeof(scene_row));
    for (i = 0; i <= 256; ++i) scene_x[i] = i * 640 / 256;
    for (i = 0; i <= 192; ++i) scene_y[i] = i * 480 / 192;
    for (i = 0; i < 192; ++i) scene_row[scene_y[i]] = 1;
    j = 0;
    for (i = 0; i <= 640; ++i) {
        while (scene_x[j] < i) ++j;
        scene_next_x[i] = scene_x[j];
    }
    DsiSceneSampling = 1;
}
int DsiSampleSpan(uint8_t *dst, const uint8_t *src, unsigned length, unsigned color,
    const uint8_t *map, const uint8_t blend[256][256], int destinationFirst)
{
    uintptr_t offset;
    unsigned x, y, end, at;
    if (!DsiSceneSampling) return 0;
    if ((uintptr_t)dst < (uintptr_t)scene_base) return 0;
    offset = (uintptr_t)dst - (uintptr_t)scene_base;
    /* The standard canvas can use constant division on ARM9. */
    y = scene_pitch == 640 ? offset / 640 : offset / scene_pitch;
    x = offset - y * scene_pitch;
    if (y >= scene_height || x >= 640 || length > 640 - x) return 0;
    if (!scene_row[y]) return 1;
    end = x + length;
    for (at = scene_next_x[x]; at < end; at = scene_next_x[at + 1]) {
        unsigned k = at - x;
        uint8_t v = src ? src[k] : color;
        if (map) v = map[v];
        if (blend) v = destinationFirst ? blend[dst[k]][v] : blend[v][dst[k]];
        dst[k] = v;
    }
    return 1;
}
void DsiEndSceneSamples(int expandForReader)
{
    unsigned x, y, yy, xx;
    if (!DsiSceneSampling) return;
    DsiSceneSampling = 0;
    if (!expandForReader) return;
    /* Each interval starts at its own sample. Expanding in place leaves the
     * next interval's source untouched. */
    for (y = 0; y < 192 && scene_y[y] < scene_height; ++y) {
        uint8_t *row = scene_base + scene_y[y] * scene_pitch;
        for (x = 0; x < 256; ++x) {
            const uint8_t sample = row[scene_x[x]];
            for (xx = scene_x[x] + 1; xx < scene_x[x + 1]; ++xx) row[xx] = sample;
        }
        for (yy = scene_y[y] + 1; yy < scene_y[y + 1] && yy < scene_height; ++yy)
            memcpy(scene_base + yy * scene_pitch, row, 640);
    }
}
