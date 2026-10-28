"""Run the actual save-menu body with a fake SD writer and clock."""
from pathlib import Path
import os, subprocess, sys, tempfile
engine=Path(__file__).resolve().parent.parent.parent
source=Path(sys.argv[1]) if len(sys.argv)>1 else engine/'Source/gamemenu.cpp'
text=source.read_text(encoding='utf-8');start=text.index('void gamemenu_save_game(')
end=text.index('{',start)+1;depth=1
while depth:
    depth+=(text[end]=='{')-(text[end]=='}');end+=1
body=text[start:end].replace('/*bActivate*/','activate').replace('demo::IsRunning()','DemoIsRunning()')
prefix=r"""
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
enum { CURSOR_HAND=1, CURSOR_NONE=0, PM_DEATH=9, EMSG_SAVING=1, EMSG_GAME_SAVED=2 };
static int pcurs=CURSOR_HAND, MyPlayerIsDead;
static struct { int _pmode; } player;
#define MyPlayer (&player)
static struct { int activated; } CornerStone;
typedef void (*EventHandler)(void);
static uint32_t clock_ms, save_delay, saved_at, saved_duration;
static int saved_count, redraws;
static void DisableInputEventHandler(void) {}
static EventHandler SetEventHandler(EventHandler e) { return e; }
static void NewCursor(int c) { pcurs=c; }
static void gamemenu_off(void) {}
static void SaveGame(void) { clock_ms+=save_delay; }
static uint32_t SDL_GetTicks(void) { return clock_ms; }
static void ClrDiabloMsg(void) {}
static void Notice(int kind,uint32_t duration) {
    if(kind==EMSG_GAME_SAVED) { ++saved_count;saved_duration=duration;saved_at=clock_ms; }
}
#define NOTICE1(kind) Notice(kind,3500)
#define NOTICE2(kind,duration) Notice(kind,duration)
#define PICK(_1,_2,NAME,...) NAME
#define InitDiabloMsg(...) PICK(__VA_ARGS__,NOTICE2,NOTICE1)(__VA_ARGS__)
static void RedrawEverything(void) { ++redraws; }
static void DrawAndBlit(void) {}
static void CornerstoneSave(void) {}
static bool DemoIsRunning(void) { return false; }
static void SaveOptions(void) {}
static void interface_msg_pump(void) {}
"""
suffix=r"""
int main(void) {
    const uint32_t starts[]={100,0xfffffff0u};
    const uint32_t delays[]={0,100,999,2500,1000,1001,10000,120000};
    for(unsigned i=0;i<2;++i) for(unsigned j=0;j<8;++j) {
        clock_ms=starts[i];save_delay=delays[j];saved_count=redraws=0;
        gamemenu_save_game(true);
        if(saved_count!=1 || saved_duration==0 || saved_duration>3500
           || saved_at!=(uint32_t)(starts[i]+delays[j]) || redraws<2 || pcurs!=CURSOR_HAND) {
            printf("FAIL: save delay %u ms at tick %u produced duration %u ms\n",delays[j],starts[i],saved_duration);
            return 1;
        }
    }
    puts("PASS: actual gamemenu_save_game with fast/slow SD saves, >1 second, 2 minutes and clock wrap: bounded post-save notice");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix='dsi-save-notice-') as d:
    code=Path(d)/'test.c';exe=Path(d)/'test.exe'
    code.write_text(prefix+body+suffix,encoding='utf-8')
    subprocess.run([os.environ.get('TCC','tcc'),'-std=c99',str(code),'-o',str(exe)],check=True)
    raise SystemExit(subprocess.run([str(exe)]).returncode)
