#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dsi_render.h"
#define PITCH 43
#define W 37
#define H 29
#define BYTES (PITCH*(H+2))
static uint32_t state=1;
static unsigned rnd(void) { state=state*1664525u+1013904223u; return state; }
static void reference(uint8_t *dst, int x, int y, const uint8_t *src, unsigned size, unsigned w, const uint8_t *map)
{
 unsigned s=0,p=0;
 while(s<size) {
  unsigned c=src[s++], n=c<128?c:c<=190?191-c:256-c, i;
  if(c<128) {p+=n;continue;}
  for(i=0;i<n;++i,++p) {
   uint8_t v=c<=190?src[s]:src[s+i];
   int xx=x+(int)(p%w), yy=y-(int)(p/w);
   if(xx>=0 && xx<W && yy>=0 && yy<H) dst[yy*PITCH+xx]=map?map[v]:v;
  }
  s+=c<=190?1:n;
 }
}
static int native_test(void)
{
 static uint8_t full[672*480], sampled[672*480], src[640], map[256], blend[256][256];
 unsigned i,j,step;
 for(i=0;i<256;++i){map[i]=255-i;for(j=0;j<256;++j)blend[i][j]=(i*3+j*7)&255;}
 for(i=0;i<sizeof(full);++i)full[i]=sampled[i]=(i*13)&255;
 for(i=0;i<640;++i)src[i]=i&255;
 DsiBeginSceneSamples(sampled,672,640,480,352);
 if(!DsiSceneSampling)return 20;
 for(step=0;step<20000;++step){
  unsigned x=rnd()%640,y=rnd()%352,n=1+rnd()%(640-x),k,mode=rnd()%4,color=rnd()%256;
  const uint8_t *data=mode==0?NULL:src;
  const uint8_t *table=mode==2?map:NULL;
  const uint8_t (*mix)[256]=mode==3?blend:NULL;
  for(k=0;k<n;++k){uint8_t v=data?data[k]:color;if(table)v=table[v];if(mix)v=mix[full[y*672+x+k]][v];full[y*672+x+k]=v;}
  if(!DsiSampleSpan(sampled+y*672+x,data,n,color,table,mix,1))return 21;
 }
 DsiEndSceneSamples(1);if(DsiSceneSampling)return 22;
 for(i=0;i<192;++i){unsigned y=i*480/192;if(y>=352)break;
  for(j=0;j<256;++j){unsigned x=j*640/256;if(full[y*672+x]!=sampled[y*672+x])return 23;}}
 for(i=0;i<480;++i)for(j=0;j<672;++j)
  if((i>=352 || j>=640) && full[i*672+j]!=sampled[i*672+j])return 24;
 /* Ordinary gameplay leaves non-display pixels alone. */
 memset(sampled, 0, sizeof(sampled));
 DsiBeginSceneSamples(sampled,672,640,480,352);
 if (!DsiSampleSpan(sampled, NULL, 640, 7, NULL, NULL, 0)) return 26;
 DsiEndSceneSamples(0);
 if (DsiSceneSampling || sampled[0] != 7 || sampled[1] != 0 || sampled[2] != 7 || sampled[672] != 0) return 27;
 /* The cache skips whole unsampled rows even as the sprite moves. */
 const uint8_t sprite[] = {187,11,187,12,187,13,187,14};
 DsiClearSpriteCache();
 for (int bottom = 0; bottom < 12; ++bottom) {
  memset(sampled,0,sizeof(sampled));
  DsiBeginSceneSamples(sampled,672,640,480,352);
  DsiDrawCachedClx(sampled,672,640,352,-1,bottom,sprite,sizeof(sprite),4,4,NULL);
  if (!DsiDrawCachedClx(sampled,672,640,352,-1,bottom,sprite,sizeof(sprite),4,4,NULL)) return 28;
  DsiEndSceneSamples(0);
  for (int y = 0; y < 12; ++y)
   for (int x = 0; x < 4; ++x) {
    const int visible = (y % 5 == 0 || y % 5 == 2) && (x == 0 || x == 2);
    const int row = bottom - y;
    const int expected = visible && row >= 0 && row < 4 ? 11 + row : 0;
    if (sampled[y*672+x] != expected) return 29;
   }
 }
 DsiBeginSceneSamples(sampled,672,320,480,352);if(DsiSceneSampling)return 25;
 puts("PASS: native-grid scene samples match full renderer after 20000 layered spans; 256x192 camera coverage, UI and pitch guards preserved");return 0;
}
int main(void)
{
 if(native_test())return 20;
 uint8_t src[4096], map[256], a[BYTES],b[BYTES]; unsigned trial,hits=0;
 for(trial=0;trial<300;++trial) {
  unsigned w=1+rnd()%64,h=1+rnd()%48,p=0,s=0,total=w*h,i;
  while(p<total) {
   unsigned kind=rnd()%3,n=1+rnd()%((kind==0)?127:63);
   if(n>total-p)n=total-p;
   if(kind!=0 && n>w-p%w)n=w-p%w;
   src[s++]=kind==0?n:kind==1?191-n:256-n;
   if(kind==1)src[s++]=rnd()>>24;
   if(kind==2)for(i=0;i<n;++i)src[s++]=rnd()>>24;
   p+=n;
  }
  for(i=0;i<256;++i)map[i]=(uint8_t)(i*13+trial);
  /* Same pointer is deliberately reused for different dimensions/content. */
  for(i=0;i<40;++i) {
   int x=(int)(rnd()%(w+W+4))-(int)w-2;
   int y=(int)(rnd()%(h+H+4))-2;
   const uint8_t *m=(i%3)?map:NULL;
   memset(a,0x5a,sizeof(a));memcpy(b,a,sizeof(a));
   reference(a+PITCH,x,y,src,s,w,m);
   if(DsiDrawCachedClx(b+PITCH,PITCH,W,H,x,y,src,s,w,h,m))++hits;
   else reference(b+PITCH,x,y,src,s,w,m);
   if(memcmp(a,b,sizeof(a))) {printf("FAIL trial %u pos %d,%d\n",trial,x,y);return 1;}
   if(i==20)map[0]^=255; /* Same table pointer with changed contents. */
  }
  /* Edit literal/fill colors without changing dimensions or pointer. */
  for(i=0;i<s;++i) { if(src[i]>=128) { src[i+1]^=37; break; } }
  memset(a,0x69,sizeof(a));memcpy(b,a,sizeof(a));
  reference(a+PITCH,1,20,src,s,w,map);
  if(!DsiDrawCachedClx(b+PITCH,PITCH,W,H,1,20,src,s,w,h,map)) reference(b+PITCH,1,20,src,s,w,map);
  if(memcmp(a,b,sizeof(a)))return 2;
  if(trial%7==0)DsiClearSpriteCache();
 }
 if(!DsiDrawCachedClx(NULL,0,W,H,W,0,NULL,20,32,32,NULL))return 3;
 if(!DsiSpriteVisible(-31,0,32,32,W,H) || DsiSpriteVisible(-32,0,32,32,W,H))return 4;
 if(!DsiSpriteVisible(0,H+30,32,32,W,H) || DsiSpriteVisible(0,H+31,32,32,W,H))return 5;

 /* Cold admission then a real cache hit, followed by an altered color table. */
 DsiClearSpriteCache();src[0]=187;src[1]=0;
 for(trial=0;trial<256;++trial)map[trial]=(uint8_t)(trial+1);
 if(DsiDrawCachedClx(b+PITCH,PITCH,W,H,0,0,src,2,4,1,map))return 7;
 if(!DsiDrawCachedClx(b+PITCH,PITCH,W,H,0,0,src,2,4,1,map) || b[PITCH]!=1)return 8;
 map[0]=17;
 if(!DsiDrawCachedClx(b+PITCH,PITCH,W,H,0,0,src,2,4,1,map) || b[PITCH]!=17)return 9;
 src[1]=3;
 if(!DsiDrawCachedClx(b+PITCH,PITCH,W,H,0,0,src,2,4,1,NULL) || b[PITCH]!=3)return 10;
 /* Over-budget runs must return fallback without changing output. */
 for(trial=0;trial<600;++trial){src[2*trial]=255;src[2*trial+1]=(uint8_t)trial;}
 for(trial=0;trial<3;++trial){
  memset(b,0x42,sizeof(b));memcpy(a,b,sizeof(a));
  if(DsiDrawCachedClx(b+PITCH,PITCH,W,H,0,0,src,1200,20,30,NULL) || memcmp(a,b,sizeof(a)))return 11;
 }
 /* Entry recovers when content at the same address changes to fewer runs. */
 src[0]=171;src[1]=33;
 DsiDrawCachedClx(b+PITCH,PITCH,W,H,0,0,src,2,20,1,NULL);
 if(!DsiDrawCachedClx(b+PITCH,PITCH,W,H,0,0,src,2,20,1,NULL))return 12;
 /* Malformed source is rejected before touching the destination. */
 src[0]=0;
 for(trial=0;trial<3;++trial){
  memset(b,0x42,sizeof(b));memcpy(a,b,sizeof(a));
  if(DsiDrawCachedClx(b+PITCH,PITCH,W,H,0,0,src,1,20,1,NULL) || memcmp(a,b,sizeof(a)))return 13;
 }
 /* Visibility check agrees with exhaustive rectangle intersection at edges. */
 for(int yy=-3;yy<H+40;++yy)for(int xx=-70;xx<W+3;++xx){
  int expected=0;
  for(int py=0;py<32;++py)for(int px=0;px<64;++px)
   if(xx+px>=0 && xx+px<W && yy-py>=0 && yy-py<H)expected=1;
  if(DsiSpriteVisible(xx,yy,64,32,W,H)!=expected)return 14;
 }
 if(hits<1000)return 6;
 printf("PASS: 12000 clipped sprite draws, guards/pitch, transparent overruns, fill/literals, live TRN changes, reused source pointers, invalidation (%u handled)\n",hits);
 return 0;
}
