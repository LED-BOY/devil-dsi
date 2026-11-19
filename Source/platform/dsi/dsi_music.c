#include <nds.h>
#include <calico/system/tick.h>
#include <SDL.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include "dsi_audio.h"

/* Decode on the engine thread: MPQ handles can share the underlying SD file.
 * ARM7 plays the ring independently. Never let an SD read write into a page
 * the sound channel has already reached. */
enum { Page = 4096, Pages = 4, Channel = 15, MaxBlock = 8192 };
static int16_t ring[Pages][Page] __attribute__((aligned(32)));
static int16_t staging[Page];
static struct {
    SDL_RWops *file;
    unsigned format, channels, rate, bits, align, samples_per_block;
    unsigned data_start, data_size, remaining, volume, block_frame;
    int predictor[2], step_index[2];
    Uint8 block[MaxBlock];
    uint64_t start, page_ticks, next;
    int playing;
} music;

static unsigned le16(const Uint8 *p) { return p[0] | (unsigned)p[1] << 8; }
static unsigned le32(const Uint8 *p) { return le16(p) | le16(p+2) << 16; }
static int signed16(unsigned n) { return n < 32768 ? (int)n : (int)n - 65536; }

static int header(void)
{
    Uint8 b[20];
    if (SDL_RWread(music.file,b,1,12) != 12 || memcmp(b,"RIFF",4) || memcmp(b+8,"WAVE",4)) return 0;
    const unsigned size = le32(b+4);
    if (size < 4 || size > INT_MAX-8) return 0;
    unsigned end = size+8;
    for (unsigned pos = 12; end-pos >= 8;) {
        if (SDL_RWread(music.file,b,1,8) != 8) return 0;
        unsigned len = le32(b+4);
        pos += 8;
        if (len > end-pos) return 0;
        if (!memcmp(b,"fmt ",4)) {
            unsigned n = len < sizeof(b) ? len : sizeof(b);
            if (n < 16 || SDL_RWread(music.file,b,1,n) != (int)n) return 0;
            music.format=le16(b); music.channels=le16(b+2); music.rate=le32(b+4);
            music.align=le16(b+12); music.bits=le16(b+14);
            if (music.channels < 1 || music.channels > 2 || music.rate < 8000 || music.rate > 48000) return 0;
            if (music.format == 1) {
                if ((music.bits != 8 && music.bits != 16) || music.align != music.channels * music.bits/8) return 0;
            } else if (music.format == 17) {
                unsigned group = 4*music.channels;
                if (n < 20 || le16(b+16) < 2 || music.bits != 4 || music.align < group || music.align > MaxBlock || music.align % group) return 0;
                music.samples_per_block=le16(b+18);
                if (music.samples_per_block != 1 + (music.align-group)*2/music.channels) return 0;
            } else return 0;
        } else if (!memcmp(b,"data",4)) {
            if (!music.align || len < music.align) return 0;
            music.data_start=pos; music.data_size=len-len%music.align;
            music.remaining=music.data_size;
            music.block_frame=music.samples_per_block;
            return 1;
        }
        if ((len & 1) && len == end-pos) return 0;
        pos += len+(len&1);
        if (SDL_RWseek(music.file,(int)pos,RW_SEEK_SET) != (int)pos) return 0;
    }
    return 0;
}

static int rewind_if_needed(void)
{
    if (music.remaining) return 1;
    if (SDL_RWseek(music.file,(int)music.data_start,RW_SEEK_SET) != (int)music.data_start) return 0;
    music.remaining=music.data_size;
    return 1;
}

static int ima_sample(unsigned channel, unsigned code)
{
    static const int steps[89] = {
        7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,
        73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,
        449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,
        2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,
        7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,
        24623,27086,29794,32767 };
    static const int adjust[8] = {-1,-1,-1,-1,2,4,6,8};
    int idx=music.step_index[channel], step=steps[idx];
    int delta=(step>>3) + ((code&4)?step:0) + ((code&2)?step>>1:0) + ((code&1)?step>>2:0);
    int value=music.predictor[channel]+((code&8)?-delta:delta);
    idx+=adjust[code&7];
    music.step_index[channel]=idx<0?0:idx>88?88:idx;
    return music.predictor[channel]=value < -32768 ? -32768 : value > 32767 ? 32767 : value;
}

static int fill(int16_t *dst)
{
    unsigned count=0;
    while (count<Page) {
        if (music.format == 1) {
            if (!rewind_if_needed()) return 0;
            unsigned frames=music.remaining/music.align;
            if (frames>Page-count) frames=Page-count;
            if (frames>MaxBlock/music.align) frames=MaxBlock/music.align;
            unsigned bytes=frames*music.align;
            if (SDL_RWread(music.file,music.block,1,bytes)!=(int)bytes) return 0;
            music.remaining-=bytes;
            const Uint8 *p=music.block;
            while (frames--) {
                int value=0;
                for (unsigned c=0;c<music.channels;++c) {
                    value += music.bits==8 ? ((int)*p-128)*256 : signed16(le16(p));
                    p+=music.bits/8;
                }
                dst[count++]=music.channels==2 ? value/2 : value;
            }
        } else {
            if (music.block_frame == music.samples_per_block) {
                if (!rewind_if_needed() || SDL_RWread(music.file,music.block,1,music.align)!=(int)music.align) return 0;
                music.remaining-=music.align;
                for (unsigned c=0;c<music.channels;++c) {
                    music.predictor[c]=signed16(le16(music.block+4*c));
                    music.step_index[c]=music.block[4*c+2];
                    if (music.step_index[c]>88) return 0;
                }
                music.block_frame=0;
            }
            int value=0;
            for (unsigned c=0;c<music.channels;++c) {
                if (!music.block_frame) value+=music.predictor[c];
                else {
                    unsigned i=music.block_frame-1;
                    unsigned byte=music.block[4*music.channels + (i/8)*4*music.channels + 4*c + (i%8)/2];
                    value+=ima_sample(c,(byte >> ((i&1)*4)) & 15);
                }
            }
            ++music.block_frame;
            dst[count++]=music.channels==2 ? value/2 : value;
        }
    }
    return 1;
}

void DsiMusicClose(void)
{
    if (music.playing) DsiReleaseMusicChannel();
    if (music.file) SDL_RWclose(music.file);
    music.file=NULL;
    music.playing=0;
}

static int prime(void)
{
    if (music.playing) {
        DsiReleaseMusicChannel();
        music.playing=0;
    }
    for (unsigned i=0;i<Pages;++i) if (!fill(ring[i])) return 0;
    DsiClaimMusicChannel();
    DC_FlushRange(ring,sizeof(ring));
    unsigned timer=soundTimerFromHz(music.rate);
    /* Calico ticks run at SYSTEM_CLOCK/64, sound at SYSTEM_CLOCK/2. */
    music.page_ticks=(uint64_t)Page*timer/32;
    soundPreparePcm(Channel|SOUND_START,music.volume,64,timer,SoundMode_Repeat,SoundFmt_Pcm16,ring,0,sizeof(ring)/4);
    soundSynchronize();
    music.start=tickGetCount(); music.next=1; music.playing=1;
    return 1;
}

int DsiMusicOpen(const char *path, unsigned volume)
{
    DsiMusicClose();
    music.file=DsiOpenAudioAsset(path); music.align=0; music.volume=volume;
    if (!music.file || !header() || !prime()) { DsiMusicClose(); return 0; }
    return 1;
}

void DsiMusicVolume(unsigned volume)
{
    music.volume=volume;
    if (music.playing) soundChSetVolume(Channel,volume);
}

void DsiMusicPump(void)
{
    if (!music.playing) return;
    uint64_t elapsed=tickGetCount()-music.start;
    if (elapsed < music.next*music.page_ticks) return;
    if (elapsed >= (music.next+Pages-2)*music.page_ticks) {
        if (!prime()) DsiMusicClose();
        return;
    }
    if (!fill(staging)) { DsiMusicClose(); return; }
    if (tickGetCount()-music.start >= (music.next+Pages-2)*music.page_ticks) {
        if (!prime()) DsiMusicClose();
        return;
    }
    int16_t *page=ring[(music.next-1)%Pages];
    memcpy(page,staging,sizeof(staging));
    DC_FlushRange(page,sizeof(staging));
    ++music.next;
}
