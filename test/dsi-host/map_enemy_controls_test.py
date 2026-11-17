"""Run the actual map actions and enemy-render dispatch with host data adapters."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]


def function(text, name):
    start = text.index('void ' + name + '(')
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


control = (repo / 'Source/control/control_panel.cpp').read_text(encoding='utf-8')
actions = function(control, 'DoAutoMap') + '\n' + function(control, 'CycleAutomapType')
actions = actions.replace('auto &options = GetOptions().Dsi;', '')
actions = actions.replace('*options.topMapShortcut', 'options.topMapShortcut')
actions = actions.replace('*GetOptions().Dsi.topMapShortcut', 'options.topMapShortcut')
actions = actions.replace('options.topMap.SetValue(!*options.topMap);', 'SetTopMap(!options.topMap);')
actions = re.sub(r'const AutomapType newType.*?;', 'const int newType = (GetAutomapType() + 1) % 3;', actions, flags=re.S)
actions = actions.replace('AutomapType::FIRST', '0')

renderer = (repo / 'Source/engine/render/scrollrt.cpp').read_text(encoding='utf-8')
draw = function(renderer, 'DrawMonsterHelper')
draw = draw.replace('const Surface &out', 'Surface out').replace('std::abs(mi)', 'abs(mi)')
draw = draw.replace('auto &towner = Towners[mi];', 'const Towner towner = Towners[mi];')
draw = draw.replace('targetBufferPosition + towner.getRenderingOffset()', 'AddPoint(targetBufferPosition, towner.offset)')
draw = draw.replace('towner.currentSprite()', 'towner.sprite')
draw = draw.replace('static_cast<size_t>(mi)', '(size_t)mi')
draw = draw.replace('const auto &monster = Monsters[mi];', 'const Monster monster = Monsters[mi];')
draw = draw.replace('monster.animInfo.currentSprite()', 'monster.sprite')
draw = draw.replace('monster.getRenderingOffset(sprite)', 'monster.offset')
draw = draw.replace('targetBufferPosition + offset', 'AddPoint(targetBufferPosition, offset)')
draw = draw.replace('*GetOptions().Dsi.highlightEnemies', 'options.highlightEnemies')
draw = draw.replace('monster.hasNoLife()', '(monster.life <= 0)')

enum_start = renderer.index('enum OutlineColors')
enum_end = renderer.index('};', enum_start) + 2
colors = renderer[enum_start:enum_end].replace(' : uint8_t', '')
palette = (repo / 'Source/engine/palette.h').read_text()
defines = '\n'.join(line for line in palette.splitlines() if line.startswith('#define PAL'))

prefix = r"""
#define __DSI__ 1
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
enum { DTYPE_TOWN, DTYPE_CATHEDRAL, MFLAG_HIDDEN = 1, MFLAG_GOLEM = 2, TEXT_NONE = 0, MaxMonsters = 4 };
typedef struct { int x, y; } Point;
typedef Point Displacement;
typedef int Surface;
typedef int ClxSprite;
typedef struct { unsigned flags; int life, talkMsg, talker, sprite; Point offset; } Monster;
typedef struct { int sprite; Point offset; } Towner;
static Monster Monsters[4];
static Towner Towners[4];
static int dMonster[2][2], pcursmonst, leveltype;
static struct { int _pInfraFlag; } player;
#define MyPlayer (&player)
static struct { bool topMapShortcut, topMap, highlightEnemies; } options;
static bool AutomapActive, tile_lit, tile_floor;
static int map_starts, map_type, map_sets, outline_count, outline_color, monster_draws, town_draws;
#define Log(...) ((void)0)
static bool IsTileLit(Point p) { (void)p; return tile_lit; }
static bool IsFloor(Point p) { (void)p; return tile_floor; }
static bool M_Talker(Monster m) { return m.talker; }
static Point AddPoint(Point a, Point b) { Point r = { a.x + b.x, a.y + b.y }; return r; }
static void ClxDrawOutlineSkipColorZero(Surface out, uint8_t color, Point at, ClxSprite sprite) {
    (void)out; (void)at; (void)sprite; ++outline_count; outline_color = color;
}
static void ClxDraw(Surface out, Point at, ClxSprite sprite) {
    (void)out; (void)at; (void)sprite; ++town_draws;
}
static void DrawMonster(Surface out, Point tile, Point at, Monster m, int light) {
    (void)out; (void)tile; (void)at; (void)m; (void)light; ++monster_draws;
}
static void StartAutomap(void) { AutomapActive = true; ++map_starts; }
static int GetAutomapType(void) { return map_type; }
static void SetAutomapType(int type) { map_type = type; }
static void SetTopMap(bool on) { options.topMap = on; ++map_sets; }
static void check(bool ok) { if (!ok) exit(1); }
"""

suffix = r"""
static void render(void)
{
    Point zero = { 0, 0 };
    outline_count = monster_draws = town_draws = 0;
    DrawMonsterHelper(0, zero, zero, 0);
}
int main(void)
{
    options.topMapShortcut = options.topMap = AutomapActive = true;
    DoAutoMap();
    check(!options.topMap && !AutomapActive && map_sets == 1 && map_starts == 0);
    DoAutoMap();
    check(options.topMap && map_sets == 2 && !AutomapActive);
    CycleAutomapType();
    check(!options.topMap && map_sets == 3 && map_type == 0);
    options.topMapShortcut = false;
    DoAutoMap(); check(AutomapActive && map_starts == 1 && map_sets == 3);
    CycleAutomapType(); check(AutomapActive && map_type == 1);
    CycleAutomapType(); check(AutomapActive && map_type == 2);
    CycleAutomapType(); check(!AutomapActive && map_type == 0);

    leveltype = DTYPE_CATHEDRAL; tile_lit = tile_floor = true;
    Monsters[0].life = 10; dMonster[0][0] = 1; pcursmonst = -1;
    render(); check(monster_draws == 1 && !outline_count);
    pcursmonst = 0;
    render(); check(outline_count == 1 && outline_color == 233);
    options.highlightEnemies = true; pcursmonst = -1;
    render(); check(outline_count == 1 && outline_color == 183);
    pcursmonst = 0;
    render(); check(outline_count == 1 && outline_color == 233);
    pcursmonst = 1;
    render(); check(outline_count == 1 && outline_color == 183);
    tile_lit = false;
    render(); check(!monster_draws && !outline_count);
    player._pInfraFlag = true;
    render(); check(monster_draws == 1 && outline_color == 183);
    Monsters[0].flags = MFLAG_HIDDEN;
    render(); check(!monster_draws && !outline_count);
    Monsters[0].flags = MFLAG_GOLEM;
    render(); check(monster_draws == 1 && !outline_count);
    Monsters[0].flags = 0; Monsters[0].life = 0;
    render(); check(monster_draws == 1 && !outline_count);
    Monsters[0].life = 10; Monsters[0].talker = Monsters[0].talkMsg = 1;
    render(); check(monster_draws == 1 && !outline_count);
    Monsters[0].talkMsg = TEXT_NONE;
    render(); check(monster_draws == 1 && outline_count == 1 && outline_color == 183);
    leveltype = DTYPE_TOWN; pcursmonst = -1;
    render(); check(town_draws == 1 && !outline_count);
    pcursmonst = 0;
    render(); check(outline_count == 1 && outline_color == OutlineColorsTowner);
    puts("PASS: map shortcut/cycle route to top map or legacy automap; blue enemies/red target respect visibility, allies and town NPCs");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix='dsi-map-enemies-') as temp:
    code = Path(temp) / 'test.c'
    exe = Path(temp) / 'test.exe'
    code.write_text(prefix + '\n' + defines + '\n' + colors + '\n' + actions + '\n' + draw + suffix)
    subprocess.run([os.environ.get('TCC', 'tcc'), '-std=c99', str(code), '-o', str(exe)], check=True)
    raise SystemExit(subprocess.run([str(exe)]).returncode)
