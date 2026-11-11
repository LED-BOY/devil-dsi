"""Exercise the DSi menu request and session restart dispatch with a stub game loop."""
from pathlib import Path
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
diablo = (repo / 'Source/diablo.cpp').read_text(encoding='utf-8')
menu = (repo / 'Source/gamemenu.cpp').read_text(encoding='utf-8')


def body(source, start):
    at = source.index('{', start)
    depth = 1
    end = at + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


request = body(diablo, diablo.index('void RequestDsiSavedGameReload()'))
menu_handler = body(menu, menu.index('void gamemenu_load_game(')).replace('/*bActivate*/', 'activate')
startup = diablo.index('bool StartGame(bool bNewGame, bool bSinglePlayer)')
dispatch = body(diablo, diablo.index('for (;;) {', diablo.index('interface_mode uMsg', startup)))

test = r'''
#define __DSI__ 1
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
enum { WM_DIABNEWGAME, WM_DIABLOADGAME };
static bool gbRunGame, gbIsMultiplayer, gbValidSaveFile, DsiSavedGameReloadRequested;
static int menu_closed, opened[4], count;
static void gamemenu_off(void) { ++menu_closed; }
static void DsiTrace(const char *text) { (void)text; }
static void check(bool result) { if (!result) exit(1); }
''' + request + '\n' + menu_handler + r'''
static void RunGameLoop(int mode)
{
    check(!DsiSavedGameReloadRequested && count < 4);
    opened[count++] = mode;
    gbRunGame = true;
    if (count <= 2) {
        gamemenu_load_game(true);
        check(DsiSavedGameReloadRequested && !gbRunGame);
        RequestDsiSavedGameReload(); /* A duplicate request cannot restart recursively. */
    } else {
        gbRunGame = false;
    }
}
int main(void)
{
    int uMsg = WM_DIABNEWGAME;
    RequestDsiSavedGameReload();
    check(!DsiSavedGameReloadRequested);
    gbRunGame = true;
    RequestDsiSavedGameReload();
    check(gbRunGame && !DsiSavedGameReloadRequested);
    gbValidSaveFile = gbIsMultiplayer = true;
    RequestDsiSavedGameReload();
    check(gbRunGame && !DsiSavedGameReloadRequested);
    gbRunGame = gbIsMultiplayer = false;
''' + dispatch + r'''
    check(count == 3 && menu_closed == 2 && !DsiSavedGameReloadRequested);
    check(opened[0] == WM_DIABNEWGAME && opened[1] == WM_DIABLOADGAME && opened[2] == WM_DIABLOADGAME);
    puts("PASS: menu reload returns to the session loop and opens the saved session once per request");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='dsi-reload-session-') as temp:
    code = Path(temp) / 'reload.c'
    exe = Path(temp) / 'reload.exe'
    code.write_text(test, encoding='utf-8')
    subprocess.run([os.environ.get('TCC', 'tcc'), '-std=c99', str(code), '-o', str(exe)], check=True)
    raise SystemExit(subprocess.run([str(exe)]).returncode)
