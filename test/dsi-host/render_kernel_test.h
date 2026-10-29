#include "dsi_render.h"

static int CheckRenderKernels(void)
{
    static uint8_t table[256][256], map[256], src[1040], dst[1040], expected[1040];
    const unsigned lengths[] = {0,1,2,3,4,5,7,8,15,16,31,32,63,64,127,256,1023,1024,1025};
    for (int a = 0; a < 256; ++a) {
        map[a] = (uint8_t)(a * 47 + 11);
        for (int b = 0; b < 256; ++b) table[a][b] = (uint8_t)((a * 7) ^ (b * 3 + 17));
    }
    for (int kind = 0; kind < 4; ++kind)
        for (int offset = 0; offset < 4; ++offset)
            for (unsigned c = 0; c < sizeof(lengths)/sizeof(lengths[0]); ++c) {
                unsigned n = lengths[c];
                for (int i = 0; i < 1040; ++i) {
                    src[i] = (uint8_t)(i * 29 + c);
                    dst[i] = expected[i] = (uint8_t)(i * 31 + 3);
                }
                for (unsigned i = offset; i < offset + n; ++i) {
                    if (kind == 0) expected[i] = map[src[i]];
                    if (kind == 1) expected[i] = table[src[i]][expected[i]];
                    if (kind == 2) expected[i] = table[expected[i]][map[src[i]]];
                    if (kind == 3) expected[i] = table[37][expected[i]];
                }
                if (kind == 0) DsiBlitMapped(dst+offset,src+offset,n,map);
                if (kind == 1) DsiBlitBlended(dst+offset,src+offset,n,table);
                if (kind == 2) DsiBlitBlendedMapped(dst+offset,src+offset,n,map,table);
                if (kind == 3) DsiFillBlended(dst+offset,n,table[37]);
                if (memcmp(dst,expected,sizeof(dst))) return 0;
                for (int i = 0; i < 1040; ++i)
                    if (src[i] != (uint8_t)(i * 29 + c)) return 0;
            }
    /* Every source/destination color, using an asymmetric table so reversing
     * blend arguments would fail (a symmetric production table can hide it). */
    for (int a = 0; a < 256; ++a) {
        memset(src,a,256);
        for (int b = 0; b < 256; ++b) dst[b] = (uint8_t)b;
        DsiBlitBlended(dst,src,256,table);
        for (int b = 0; b < 256; ++b) if (dst[b] != table[a][b]) return 0;
        for (int b = 0; b < 256; ++b) dst[b] = (uint8_t)b;
        DsiBlitBlendedMapped(dst,src,256,map,table);
        for (int b = 0; b < 256; ++b) if (dst[b] != table[b][map[a]]) return 0;
    }
    DsiBlitMapped(NULL,NULL,0,NULL); DsiBlitBlended(NULL,NULL,0,NULL);
    DsiBlitBlendedMapped(NULL,NULL,0,NULL,NULL); DsiFillBlended(NULL,0,NULL);
    puts("PASS: sprite pixel kernels: all blend color pairs/orientation, lighting map, zero/odd/tail lengths, unaligned buffers, source preservation and destination guards");
    return 1;
}

static int CheckPackedStores(void)
{
    static uint8_t source[644*480+1];
    static uint16_t target[132*192+8];
    const int modes[][2] = {{256,192},{640,480},{641,479}};
    for (int mode = 0; mode < 3; ++mode)
        for (int offset = 0; offset < 2; ++offset)
            for (int pitch = 128; pitch <= 131; pitch += 3) {
                int w = modes[mode][0], h = modes[mode][1], sp = w+2;
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x) source[1+y*sp+x] = (uint8_t)(x*3+y*7);
                for (int zoom = 0; zoom <= 250; zoom += zoom ? 50 : 150) {
                    for (int i = 0; i < 132*192+8; ++i) target[i] = 0x5a5a;
                    if (zoom) {
                        if (!dsi_present_reading_index8(source+1,w,h,sp,0,h,zoom,target+offset,pitch)) return 0;
                    } else if (!dsi_present_index8(source+1,w,h,sp,target+offset,pitch)) return 0;
                    int cw = zoom ? w*100/zoom : w, ch = zoom ? h*100/zoom : h;
                    int left = 0, top = zoom ? h-ch : 0;
                    for (int y = 0; y < 192; ++y) {
                        for (int x = 0; x < 256; ++x) {
                            uint8_t actual = (uint8_t)(target[offset+y*pitch+x/2] >> (8*(x%2)));
                            uint8_t expected = (uint8_t)((left+x*cw/256)*3+(top+y*ch/192)*7);
                            if (actual != expected) return 0;
                        }
                        for (int x = 128; x < pitch; ++x)
                            if (target[offset+y*pitch+x] != 0x5a5a) return 0;
                    }
                    if (offset && target[0] != 0x5a5a) return 0;
                    for (int i = offset+192*pitch; i < 132*192+8; ++i)
                        if (target[i] != 0x5a5a) return 0;
                }
            }
    puts("PASS: word/halfword VRAM paths: native/scaled/reader pixels, alignment fallback, odd pitch and guard regions");
    return 1;
}
