#include "dsi_render.h"
#include "dsi_hot.h"
#include "dsi_video.h"

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
enum { SceneWidth = 640, SceneHeight = 480 };
int DsiSceneSampling;
static uint8_t *scene_base;
static unsigned scene_pitch, scene_height, scene_rows;
static uintptr_t scene_bytes;
static int scene_packed, scene_ready;
static uint16_t scene_next_x[SceneWidth + 1], scene_x[DSI_WIDTH + 1], scene_y[DSI_HEIGHT + 1];
/* Zero means the logical row is skipped. Other entries are native row + 1. */
static uint8_t scene_row[SceneHeight], scene_previous_y[SceneHeight];
static uint8_t scene_pixels[DSI_WIDTH * DSI_HEIGHT] __attribute__((aligned(4)));

static inline uint8_t *PackedRow(unsigned y)
{
    return scene_pixels + (scene_row[y] - 1) * DSI_WIDTH;
}

/* The 640-to-256 grid advances by two and three source pixels
 * alternately. After the first lookup, every pair advances by five. */
static DSI_INLINE void CopyPackedPixels(uint8_t *dst, const uint8_t *src,
    unsigned count, unsigned first, const uint8_t *map)
{
    const unsigned step = 2 + (first & 1);
    unsigned i = 0, k = 0;
    if (map) {
        for (; i + 1 < count; i += 2, k += 5) {
            dst[i] = map[src[k]];
            dst[i + 1] = map[src[k + step]];
        }
        if (i < count) dst[i] = map[src[k]];
    } else {
        for (; i + 1 < count; i += 2, k += 5) {
            dst[i] = src[k];
            dst[i + 1] = src[k + step];
        }
        if (i < count) dst[i] = src[k];
    }
}

static DSI_INLINE void WritePackedSpan(unsigned x, unsigned y, unsigned length, const uint8_t *src,
    unsigned color, const uint8_t *map, const uint8_t blend[256][256], int destinationFirst, int keyed)
{
    const unsigned first = scene_next_x[x], end = scene_next_x[x + length];
    const unsigned count = end - first;
    if (!count) return;
    uint8_t *dst = PackedRow(y) + first;

    if (!blend) {
        if (!src) {
            const uint8_t value = map ? map[color] : color;
            if (count == 1)
                *dst = value;
            else
                memset(dst, value, count);
        } else if (!keyed) {
            CopyPackedPixels(dst, src + (scene_x[first] - x), count, first, map);
        } else {
            for (unsigned i = 0; i < count; ++i) {
                const uint8_t value = src[scene_x[first + i] - x];
                if (value != 0) dst[i] = value;
            }
        }
        return;
    }

    if (!src && !destinationFirst) {
        DsiFillBlended(dst, count, blend[map ? map[color] : color]);
        return;
    }
    for (unsigned i = 0; i < count; ++i) {
        uint8_t value = src ? src[scene_x[first + i] - x] : color;
        if (map) value = map[value];
        dst[i] = destinationFirst ? blend[dst[i]][value] : blend[value][dst[i]];
    }
}

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
        && pitch == scene_pitch && outWidth == SceneWidth && outHeight <= (int)scene_height;
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
            const uint8_t *data = run->fill ? NULL : colors + run->offset + skip;
            if (scene_packed) {
                WritePackedSpan(left, yy, right - left, data, colors[run->offset], NULL, NULL, 0, 0);
            } else {
                for (unsigned i = scene_next_x[left]; i < scene_next_x[right]; ++i) {
                    const unsigned k = scene_x[i] - left;
                    out[k] = data ? data[k] : colors[run->offset];
                }
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

/* Keep the camera and all sprite positions in logical coordinates. */
static int BeginSceneSamples(uint8_t *base, unsigned pitch, unsigned width,
    unsigned height, unsigned viewHeight, int packed)
{
    static int tables_ready;
    DsiSceneSampling = 0;
    if (!base || width != SceneWidth || height != SceneHeight || viewHeight == 0
        || viewHeight > SceneHeight || pitch < SceneWidth)
        return 0;

    if (!tables_ready) {
        unsigned j = 0;
        for (unsigned i = 0; i <= DSI_WIDTH; ++i) scene_x[i] = i * SceneWidth / DSI_WIDTH;
        for (unsigned i = 0; i <= DSI_HEIGHT; ++i) scene_y[i] = i * SceneHeight / DSI_HEIGHT;
        for (unsigned i = 0; i < DSI_HEIGHT; ++i) {
            scene_row[scene_y[i]] = i + 1;
            for (unsigned y = scene_y[i]; y < scene_y[i + 1]; ++y)
                scene_previous_y[y] = i;
        }
        for (unsigned i = 0; i <= SceneWidth; ++i) {
            while (scene_x[j] < i) ++j;
            scene_next_x[i] = j;
        }
        tables_ready = 1;
    }
    const unsigned rows = (viewHeight * DSI_HEIGHT + SceneHeight - 1) / SceneHeight;
    if (packed && (!scene_ready || base != scene_base || pitch != scene_pitch || rows != scene_rows))
        memset(scene_pixels, 0, rows * DSI_WIDTH);
    scene_ready = 0;
    scene_base = base;
    scene_pitch = pitch;
    scene_height = viewHeight;
    scene_bytes = (uintptr_t)viewHeight * pitch;
    scene_rows = rows;
    scene_packed = packed;
    DsiSceneSampling = 1;
    return 1;
}

void DsiBeginSceneSamples(uint8_t *base, unsigned pitch, unsigned width, unsigned canvasHeight, unsigned viewHeight)
{
    BeginSceneSamples(base, pitch, width, canvasHeight, viewHeight, 0);
}

int DsiBeginPackedScene(uint8_t *base, unsigned pitch, unsigned width, unsigned height, unsigned viewHeight)
{
    return BeginSceneSamples(base, pitch, width, height, viewHeight, 1);
}

static inline int LogicalSpan(const uint8_t *pixel, unsigned length, unsigned *x, unsigned *y)
{
    if (!DsiSceneSampling || (uintptr_t)pixel < (uintptr_t)scene_base) return 0;
    const uintptr_t offset = (uintptr_t)pixel - (uintptr_t)scene_base;
    if (offset >= scene_bytes) return 0;
    /* The standard canvas uses constant division on ARM9. */
    *y = scene_pitch == SceneWidth ? offset / SceneWidth : offset / scene_pitch;
    *x = offset - (uintptr_t)*y * scene_pitch;
    return *x < SceneWidth && length <= SceneWidth - *x;
}

static DSI_INLINE int SampleSpan(uint8_t *dst, const uint8_t *src, unsigned length, unsigned color,
    const uint8_t *map, const uint8_t blend[256][256], int destinationFirst, int keyed)
{
    unsigned x, y;
    if (!LogicalSpan(dst, length, &x, &y)) return 0;
    if (!scene_row[y]) return 1;
    if (scene_packed) {
        WritePackedSpan(x, y, length, src, color, map, blend, destinationFirst, keyed);
    } else {
        for (unsigned i = scene_next_x[x]; i < scene_next_x[x + length]; ++i) {
            const unsigned k = scene_x[i] - x;
            uint8_t v = src ? src[k] : color;
            if (keyed && v == 0) continue;
            if (map) v = map[v];
            if (blend) v = destinationFirst ? blend[dst[k]][v] : blend[v][dst[k]];
            dst[k] = v;
        }
    }
    return 1;
}

DSI_HOT int DsiSampleSpan(uint8_t *dst, const uint8_t *src, unsigned length, unsigned color,
    const uint8_t *map, const uint8_t blend[256][256], int destinationFirst)
{
    return SampleSpan(dst, src, length, color, map, blend, destinationFirst, 0);
}

int DsiReadPackedSpan(uint8_t *dst, const uint8_t *logical, unsigned length)
{
    unsigned x, y;
    if (!scene_packed || !LogicalSpan(logical, length, &x, &y)) return 0;
    const uint8_t *row = scene_pixels + scene_previous_y[y] * DSI_WIDTH;
    for (unsigned i = 0; i < length; ++i) {
        const unsigned next = scene_next_x[x + i];
        dst[i] = row[next - (scene_x[next] > x + i)];
    }
    return 1;
}

int DsiSampleKeyedSpan(uint8_t *dst, const uint8_t *src, unsigned length)
{
    return SampleSpan(dst, src, length, 0, NULL, NULL, 0, 1);
}

static void ExpandScene(int packed)
{
    for (unsigned y = 0; y < scene_rows; ++y) {
        uint8_t *row = scene_base + scene_y[y] * scene_pitch;
        const uint8_t *native = scene_pixels + y * DSI_WIDTH;
        for (unsigned x = 0; x < DSI_WIDTH; ++x) {
            const uint8_t sample = packed ? native[x] : row[scene_x[x]];
            for (unsigned xx = scene_x[x] + !packed; xx < scene_x[x + 1]; ++xx) row[xx] = sample;
        }
        for (unsigned yy = scene_y[y] + 1; yy < scene_y[y + 1] && yy < scene_height; ++yy)
            memcpy(scene_base + yy * scene_pitch, row, SceneWidth);
    }
}

void DsiEndSceneSamples(int expandForReader)
{
    if (!DsiSceneSampling) return;
    DsiSceneSampling = 0;
    if (scene_packed)
        scene_ready = 1;
    else if (expandForReader)
        ExpandScene(0);
}

const uint8_t *DsiPackedScene(const void *base, unsigned pitch, unsigned *rows)
{
    if (!scene_ready || base != scene_base || pitch != scene_pitch) return NULL;
    *rows = scene_rows;
    return scene_pixels;
}

void DsiRestorePackedScene(void)
{
    if (!scene_ready) return;
    ExpandScene(1);
    scene_ready = 0;
}

void DsiDiscardPackedScene(void)
{
    scene_ready = 0;
}
