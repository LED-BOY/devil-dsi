# Experimental Nintendo DSi engine integration

## R3 update

The R2 hardware report said only the D-pad worked. The supplied startup log
never reached `StartGame`; its last heap-used checkpoint was 2,584,688 bytes,
so that log does not establish RAM exhaustion during level loading.

A host test reproduced the more fundamental failure: with SDL_THREADS_DISABLED,
SDL leaves its event queue lock null and still calls the mutex helpers. The DSi
helpers rejected a null lock, dropping ALL queued key, touch, and progress
events while held-key state still updated. R3 treats those locks as no-ops in
the single-engine-thread build, matching SDL's disabled-thread semantics.

R3 also routes the main engine to the bottom LCD with `lcdMainOnBottom`; the
diagnostic console moves to the top. Touch coordinates use that same displayed
viewport. Button keys are non-modifier F1-F8 events, with the correct Nintendo
physical positions (A is right, B bottom, X top, Y left). This avoids keyboard
modifier shortcuts as well as correcting the swapped face-button positions.

The real SDL core/driver host test passes all 12 press/release pairs and held
states, zero modifier leakage, aligned touch down/up, a queued user/progress
event, bottom display selection, all 256 palette colours, and 49,152 pixels.
The original queue helpers reproduced the failure before the fix. Physical
DSi verification of R3 and actual level/gameplay testing are still required.

## R2 update

The user reported that R1 reaches the menu, but has pink/blue colours and freezes
when starting a character. R2 corrects the missing SDL hardware-palette flag
(which caused a shadow surface with remapped indices), uses the SDL RAM surface
as the engine backbuffer, and adds `sd:/devilutionx/dsi-startup.txt` checkpoints
with malloc arena statistics. The trace is overwritten on the next launch.
This reduces duplicate framebuffer storage by about 600 KiB. Heap statistics
report the allocator's arena, not all available RAM. No SD swap system is added.

A desktop regression test compiled the actual SDL core and DSi driver against
simulated hardware registers: R1 failed due to its shadow surface; R2 passed
256 palette colours and all 49,152 blit/presentation pixels. This validates SDL
integration logic, not physical display hardware. The tests and original driver
are included under `test/dsi-host` in the source ZIP; with Python and TinyCC,
run `python test/dsi-host/run.py`. Set the `TCC` environment variable to the
compiler path if needed.

The character-start freeze is still unconfirmed. R2's loading trace is needed
before deciding whether it is memory pressure, a stalled operation, or another
platform issue.

This is the supplied DevilutionX 1.6.0-dev source with a DSi platform target.
It compiles the actual game engine and calls `devilution::DiabloMain`; it does
not use the earlier test-pattern program as its main application.

The original Desktop source was not edited. This working copy adds:

- A devkitARM CMake toolchain and DSi platform configuration.
- SDL 1.2.15 software surfaces, blits, palette, file streams, and event handling,
  with a new DSi video/input/timing driver. SDL source is in `3rdParty/SDL12`.
- Indexed 640x480 rendering downsampled to the 256x192 bottom display. The
  touch surface maps to the same displayed virtual screen coordinates.
- Button events connected to the engine's Nintendo keyboard-controller mapping.
- DSi-mode/SD initialization, fixed `sd:/devilutionx/` data/config/save paths,
  a 128 KiB engine stack with Calico TLS, and startup movies skipped.
- Sound and networking disabled; level loading runs on the engine thread.
  SDL-created background threads are unsupported. Mutexes assume one engine
  thread; enabling networking, background loading, or audio requires real locks
  and thread implementations.
- Random initialization that avoids unsupported `std::random_device` during
  static construction, filesystem fallbacks suitable for newlib/libfat, and a
  fix for the existing no-LUT black-transparency rendering fallback.
- Fatal engine errors printed on the diagnostic console and written to
  `sd:/devilutionx/dsi-error.txt`, then held until START.

## Build

Use native Windows CMake and Ninja, plus an installed devkitPro DS toolchain.
The MSYS CMake shipped in the tested installation generated invalid dependency
paths with the Windows ARM compiler; the native Windows generator avoids that.

In PowerShell:

```powershell
.\Build-DSi.ps1 -CMake 'C:\path\to\cmake.exe' -Ninja 'C:\path\to\ninja.exe'
```

The source archive includes the pinned fetched dependencies in `dsi-vendor`.
The script supplies those to CMake to avoid fetching them again. If that folder
is absent, the original upstream dependency declarations download their pinned
sources and verify their configured hashes.

The expected outputs are `build-dsi/devilutionx`, `build-dsi/devilutionx.map`,
`build-dsi/devilutionx-dsi.nds`, and the generated `assets` and `mods` folders.
The runtime package places the NDS on the SD root and those folders under
`/devilutionx/`. Add your own `DIABDAT.MPQ` in `/devilutionx/`.

## Verification and remaining work

The complete engine cross-compiled, linked, and packaged with devkitARM GCC
16.1.0, native CMake/Ninja, installed libnds/Calico, and ndstool 2.3.1. The
portable display-coordinate tests passed earlier. The earlier standalone probe
was shown running in DSi mode with SD access and touch on the user's hardware.

**This engine build has not yet been run on a DSi or emulator.** The probe's
hardware result does not validate the engine. Main menu, character creation,
town loading, frame rate, peak heap use, gameplay, save/reload, and repeated
level changes still require testing. Default nearest-neighbour scaling makes
text small, display updates may tear, and UI/control usability needs work.

First test: launch in DSi mode, confirm the engine reaches its main menu, create
a single-player Warrior, and attempt town loading. Photograph both screens if
it stops, and preserve `dsi-error.txt` if one is created. Back up any existing
saves before using an experimental engine build.

See the root `LICENSE.md` and dependency license files. SDL 1.2.15 is LGPL-2.1
or later; its complete source is included. No Diablo MPQ data is distributed.
