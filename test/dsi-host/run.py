from pathlib import Path
import subprocess
import sys
import os
root = Path(__file__).resolve().parent.parent.parent
engine = root if (root / 'Source').is_dir() else root / 'work/devilutionx-dsi'
sdl = engine / '3rdParty/SDL12'
test = Path(__file__).resolve().parent
driver = Path(sys.argv[1]) if len(sys.argv) > 1 else engine / 'Source/platform/dsi/sdl_dsi.c'
patterns = ['src/*.c', 'src/audio/*.c', 'src/audio/dummy/*.c', 'src/cdrom/*.c',
            'src/cdrom/dummy/*.c', 'src/events/*.c', 'src/file/*.c', 'src/cpuinfo/*.c',
            'src/joystick/*.c', 'src/joystick/dummy/*.c', 'src/loadso/dummy/*.c',
            'src/stdlib/*.c', 'src/thread/*.c', 'src/thread/generic/*.c',
            'src/timer/*.c', 'src/video/*.c']
sources = [p for pat in patterns for p in sorted(sdl.glob(pat)) if p.name != 'SDL_sysmutex.c']
tcc = os.environ.get('TCC', str(root / 'work/tooling/tcc/tcc.exe') if (root / 'work/tooling/tcc/tcc.exe').is_file() else 'tcc')
args = [tcc, '-std=c99',
        '-U__WIN32__', '-D_SDL_platform_h', '-D__NDS__', '-include', str(test / 'include/host_compat.h')]
for include in [test / 'include', sdl / 'include', sdl / 'src', sdl / 'src/video',
                sdl / 'src/events', engine / 'Source/platform/dsi']:
    args += ['-I' + str(include)]
exe = test / 'palette_test.exe'
args += [str(p) for p in sources] + [str(driver), str(engine / 'Source/platform/dsi/dsi_video.c'), str(engine / 'Source/platform/dsi/dsi_render.c'),
                                   str(engine / 'Source/platform/dsi/dsi_audio.c'),
                                   str(test / 'palette_test.c'), '-o', str(exe)]
result = subprocess.run(args)
if result.returncode:
    raise SystemExit(result.returncode)
raise SystemExit(subprocess.run([str(exe)]).returncode)
