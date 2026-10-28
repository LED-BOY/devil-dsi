# Experimental Nintendo DSi engine integration


## R6 test5: renderer kernels and fast instruction memory

`dsi_render.c` provides byte-exact palette-map, blend, mapped-blend and blended
fill kernels, unrolled four pixels at a time. `blit_impl.hpp` routes DSi calls
to them while retaining upstream paths on other platforms. Table argument order
is preserved, including destination-first mapped blends. Kernels accept arbitrary
byte alignment, zero/odd lengths and do not read/write outside the requested run.

These four kernels and both presenters compile as ARM with -O2 and reside in
Calico's standard `.itcm` section. Loader/linker initialization is unchanged.
The selected sprite/tile/scene C++ translation units use -O2; other engine code
retains -Os. Word-aligned destination rows use 32-bit stores, reducing VRAM
store count from 24576 to 12288 per full screen. Other rows use halfwords;
source reads remain byte-safe. No DMA, memory-controller remapping or new cache.

The linked ELF uses 3640/32736 ITCM bytes including library routines; text is
2174208 bytes (+7428 from test4) and BSS 710764 (+8). Host pixel tests and the
linked ARM/section checks pass. The internal resolution, touch transform, view,
reader/map settings, game rules, SFX cache and shoulder controls are unchanged.
Actual FPS gains require DSi measurement; the reported test4 baseline is about
7.4-8 FPS, without a stage timing breakdown yet.

## R6 test4: map pacing, display copies and measurements

The top map renders/copies at 15 Hz by default, with 10 Hz and Every Frame
options in Nintendo DSi settings. Bottom presentation continues independently.
Scale changes, loading/movie/menu restoration and live configuration invalidate
the map. Millisecond rollover is covered by scheduler tests.

Optional Performance Stats uses Calico ticks to average engine drawing, map
drawing and bottom/top presentation per gameplay frame, plus wall-clock FPS.
The native-resolution overlay is on the map only; off by default, with no
per-frame SD logging. Skipped map frames contribute zero cost. Display totals
exclude uninstrumented game logic and intentional frame waiting.

Separate cached overview/reader tables remove repeated coordinate division.
A native 256x192 copy avoids scale gathers, preserving paired VRAM writes and
support for unaligned/padded sources. Pixel comparisons cover mode changes,
alternating reader/overview calls, odd sizes, destination guards and all pixels.

Hardware profiling is still required before promising an FPS increase or
changing internal resolution, placing renderer code in ITCM, or adding an SD
cache. This build preserves the tested 640x480 layout and existing audio cache.

## R6 test3: slow-save timeout and default R healing potion

The saved-message persistence had a second cause in `gamemenu_save_game`:
`start + 1000 - now` was passed as an unsigned duration after writing the save.
A 2.5-second SD write produced 4,294,965,796 ms (about 49.7 days). The saved
notice now starts after SaveGame finishes and lasts a fixed 1500 ms. The test2
redraw invalidation is retained. A harness executes the actual save-menu body
with simulated fast writes, 2.5-second and 2-minute writes, and tick wraparound:
the old body fails and the new body passes.

R now defaults to **Use Health Potion**, drawing from the belt. It uses a
separate SDL user event and bypasses keyboard/controller mappings, producing
one use per physical press without repeats while held. It does not act while
dead, paused, in NPC/game menus, during movies or loading. Inventory panels
can remain open. L still defaults to Right Mouse Button. Both shoulder menus
offer right-click, health potion, controller shoulder and disabled.

The old default R=Controller Shoulder migrates once to the new healing action.
Explicit Right Mouse Button or Disabled choices are preserved. After migration,
choosing Controller Shoulder remains available and retains the engine's usual
R mapping (mana potion unless rebound). INI values are `R Button=3` and
`Shoulder Defaults Version=1`. The reader chord remains menu-only and consumes
pending potion presses when L+R are pressed together.

Run `python test/dsi-host/save_notice_test.py` with TCC configured, alongside
the SDL driver suite. The latter verifies potion events, held/release/remap
behavior and chord isolation. Actual belt use and message disappearance still
need DSi confirmation.

Wi-Fi module memory is not normal ARM9 working RAM; the Atheros is reached via
SDIO. The DSi's separate 768 KiB NWRAM may be useful for selected workloads,
but no FPS gain is established and this build does not remap memory banks.
Performance work should profile rendering/blitting/map cost first.
References: https://dsibrew.org/wiki/SD/MMC/SDIO_Registers and
https://blocksds.skylyrac.net/docs/internal/memory_map/ .

## R6 test2: messages, loading screen and experimental effects audio

- Message expiry now runs before the frame redraw decision. Expiry, dismissal,
  and clearing invalidate the background, so the last saved-message frame is
  repainted instead of remaining baked into the direct framebuffer.
- Game/level loading and UI progress dialogs hold the top display black through
  palette changes. Nested loading restores the intended menu/map view only on
  final completion. Fatal error output overrides loading/movie blackout.
- Experimental sound effects use the Calico ARM7 sound service: up to 16 mono
  PCM hardware voices with positional panning, original sample rates, and the
  existing SFX/user volume settings. No audio thread or SDL_audiolib is used.
- SDL's WAV decoder accepts PCM and ADPCM; stereo input is downmixed to mono.
  Samples are decoded lazily. Input WAVs are limited to 128 KiB, each decoded
  sample to 256 KiB, and the shared LRU cache to 1 MiB. Active channels pin their
  PCM until ARM7 completes/stops playback. Oversized/unsupported effects are
  skipped; long voices, MP3 replacements, music and movie audio are not supported.
- Settings -> Nintendo DSi -> **Sound Effects** toggles the backend immediately.
  To disable it before startup, set `Sound Effects=0` under `[Nintendo DSi]` in
  `sd:/devilutionx/diablo.ini`. The default is on. The first few successful and
  skipped samples are recorded in the startup log for hardware diagnosis.

Actual SDL host tests pass PCM8/stereo16/IMA decoding and sample values,
volume/pan, cache reuse/eviction/pinning and the 1 MiB limit, channel saturation,
mute/missing/oversized inputs, enable/disable/shutdown, black loading screens,
nested restoration, fatal console override, and previous R3-R6 regressions.
The complete devkitARM build passes. Audible playback, game-save notification
removal, loading transitions, and the potion issue still need DSi testing.

## R6 source changes (hardware verification pending)

- Movies use only the bottom display. The top bitmap and console are hidden
  against a black backdrop during playback; normal completion, skipping, or
  failure to open a movie restores the previous top view. Palette changes do
  not recolor the black top backdrop.
- Settings -> Nintendo DSi now includes **R Button Action**, with Right Mouse
  Button, Controller Shoulder, and Disabled. R defaults to Controller Shoulder
  to preserve R5 behavior. It uses `R Button=0` in the existing settings section;
  values 0/1/2 have the same meaning as L. The normal settings flow saves it.
- Both shoulders release their original action on the next input pump after a
  mapping change, then wait for physical release before accepting a new action.
  Right-click mappings emit no controller shoulder key. Two right-click
  shoulders share one mouse hold. Either shoulder + touch moves the pointer;
  lifting the shoulder first does not create an extra left click.
- DSi right-click rechecks inventory/belt hit targets at the current pointer.
  This addresses a possible potion-use path through stale controller focus
  after a touch or cursor warp. Intentional right-click on a belt potion or
  inventory item retains normal item-use behavior. The reported hardware
  symptom is not yet confirmed fixed.
- L+R reading remains available with all mappings. Both right-click mappings
  use the menu chord delay; gameplay clicks remain immediate.

Run `python test/dsi-host/run.py` with `TCC` pointing to a Windows Tiny C
Compiler executable. The actual SDL core/driver tests pass the R3-R5 regressions
and new R6 checks for both shoulders, holds, live remapping, touch, quick menu
clicks, reader chords in either order, overlapping mouse holds, and movie
frame/palette/view isolation. These tests do not run the engine's potion logic
or settings UI/persistence. No R6 NDS was cross-built in this environment
(devkitPro is unavailable). Use the [hardware checklist](DSI_TODO.md) before
publishing an R6 build; existing build metadata still describes R5.

## R5: configurable reading, map and L button

Open **Options -> Nintendo DSi** (main-menu Settings) to configure:

| Setting | Choices | Default |
| --- | --- | --- |
| Menu Reading Zoom | 1.5x, 2x, 2.5x | 2x |
| Top Map Zoom | Far (25%), Wide (35%), Close (50%) | Wide (35%) |
| L Button Action | Right Mouse Button, Controller Shoulder, Disabled | Right Mouse Button |

Reading zoom is relative to the complete bottom overview: at 640x480, the
source windows are 426x320, 320x240, and 256x192 respectively. Each fills the
top LCD with nearest-neighbour sampling. R4 used the equivalent of 2.5x.
The new map default (35%) is wider than R4's 50%; 25% shows still more area.
Only the top map is affected. The bottom map's scale/type/offset are preserved.

Changes apply immediately and use the engine's normal configuration save
when leaving Settings or exiting normally, in `sd:/devilutionx/diablo.ini`:

```ini
[Nintendo DSi]
Menu Zoom=200
Map Scale=35
L Button=1
```

Menu Zoom accepts 150/200/250; Map Scale accepts 25/35/50; L Button uses
1=right mouse, 0=controller shoulder, 2=disabled. Missing/invalid values use
the defaults shown above. Do not replace your existing settings file to upgrade.

L presses/releases the right mouse button at the current pointer. In this
mode, holding L while touching moves the pointer without generating a left
click too. Controller Shoulder restores the previous L gamepad behavior.
The L action is latched until release, so changing its setting while held
cannot leave a mouse button or keyboard key pressed.

Press **L+R together** in a menu to pan the reader with D-pad or touch; this
works in all L modes. Menu right-click waits up to 120 ms for that chord,
while a quick L tap still clicks on release. Gameplay right-click is immediate.
Reading gestures stay consumed until the physical keys are released.

The actual SDL host test covers all pixels at all three zoom settings, edge
clamps, map-scale choices/defaults, L right-click coordinates and held state,
quick taps, delayed menu presses, chord cancellation, remapping while held,
disabled/restored shoulder modes, and L+touch input. R5 builds successfully,
but its settings menu, saved preferences and live behavior still need DSi
hardware verification. R3 is the last user-reported working revision.

## R4: menu reader and gameplay map (historical)

- The complete 640x480 game stays on the 256x192 bottom display.
- Menus use a native 256x192 crop on top: one source pixel per LCD pixel,
  approximately 2.5 times the text size of the bottom overview. Main-menu
  selectors and the in-game pause/options selection guide the crop automatically.
- Inventory, character, spell, quest, stash, shop, help, dialogue, and pause
  panels also activate the reader. Long text may require panning.
- In a menu, hold **L+R** and use the **D-pad** to pan, or touch/drag the
  bottom overview to center the reader. Reading gestures do not send clicks
  or navigation keys to the engine. Release the gesture keys before navigating.
- Ordinary gameplay shows a separate native 256x192 automap on top, centered
  on the player at 50% map scale. It uses the engine's map shapes, exploration,
  player arrows, and level information. It does not require the bottom overlay
  to be enabled, change its settings, or reveal unexplored dungeon areas.
- Menus replace the top map temporarily; closing them returns to the map.
  Startup diagnostics remain logged. Handled fatal errors restore the console.
- The top bitmap uses the upper 64 KiB of VRAM C; console tiles/map remain
  in its lower half. Palette switching restores the console palette on errors.
  The separate map buffer adds 48 KiB of static ARM9 RAM.

The user reports R3 works on hardware. The supplied R3 trace reached town,
finished level loading, and entered the game loop, with heap_used=7,297,056
bytes at game-loop entry (allocator arena statistics, not total RAM use).
R4 is cross-built and host-tested but has not yet been tested on a DSi.
Its host test covers the actual SDL core, both output buffers, native reading
pixels and edge bounds, consumed reading controls, normal gameplay shoulders,
palette switching, console restoration, and the previous R3 regressions.
The actual map contents, scene transitions, frame rate, and top LCD VRAM setup
still require hardware testing.

## R3 update (historical)

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

R3 has now reached the game loop on the user's hardware and is reported to
work. R4's map and menu reader need hardware verification: launch, check menu
selection/panning, enter town, walk to confirm the top map follows, open and
close inventory/pause menus, and enter a dungeon to check exploration. Preserve
`dsi-startup.txt` if a problem occurs. Save/reload and longer sessions still
need testing.

See the root `LICENSE.md` and dependency license files. SDL 1.2.15 is LGPL-2.1
or later; its complete source is included. No Diablo MPQ data is distributed.
