# devil-dsi

An experimental, unofficial Nintendo DSi port of [DevilutionX](https://github.com/diasurgical/DevilutionX), based on the supplied **1.6.0-dev** source snapshot.

**This is a modified DevilutionX source tree.** Upstream engine code, assets, authorship, and license notices are retained. This project is not an official DevilutionX release.

## Current status: R6 test5 (experimental)

- Builds an actual DS/DSi `.nds` executable that calls the game engine.
- Full game on the **bottom display**; a native-resolution map on top during gameplay.
- Menus and in-game panels switch the top screen to a configurable reading zoom (1.5x / 2x / 2.5x, default 2x). Hold **L+R** and use the D-pad to pan, or touch the bottom overview to center the reading view without clicking. Release gesture keys before navigating normally.
- 640x480 indexed rendering downsampled to 256x192, with corrected SDL hardware-palette handling.
- Nintendo button mappings and absolute touch-to-left-click input.
- Fixed single-thread SDL event queue handling: previously the D-pad's held state worked while queued button, touch, and progress events were dropped.
- Experimental short WAV/ADPCM sound effects through the DSi hardware mixer; music and networking remain disabled. Settings -> Nintendo DSi -> Sound Effects can disable audio.
- Persistent startup/loading trace at `sd:/devilutionx/dsi-startup.txt` and handled error output at `sd:/devilutionx/dsi-error.txt`.

The user reports R3 working on a DSi; the supplied trace reached town and entered the game loop. **The R6 changes and effects audio still need hardware verification.** The actual SDL host tests pass input, palette, all three reading zooms/pixel bounds and configurable L-button input, reading-input isolation, separate display buffers, and console restoration checks. Actual map contents, VRAM configuration, transitions, and runtime performance remain hardware test items.

## Next version

R6 test5 adds ARM/ITCM sprite pixel kernels, speed optimization for selected renderer files and aligned word VRAM copies, retaining the same game view and controls. R6 test4 reduces top-map rendering/copying to 15 Hz by default, caches scaling tables and adds an optional native-size performance overlay. Test3 fixes the unsigned saved-message timeout after slow SD writes and defaults R to a dedicated belt healing-potion action. It retains test2 loading blackout, redraw cleanup and experimental hardware effects audio. Full cross-build and SDL host regressions pass; DSi testing remains pending. See [DSI_PORT.md](DSI_PORT.md) and the [hardware checklist](DSI_TODO.md).

## Nintendo DSi options

Open **Settings -> Nintendo DSi** from the main menu. The choices apply immediately and save through the normal settings flow to `sd:/devilutionx/diablo.ini`.

| Setting | Choices | Default |
| --- | --- | --- |
| Menu Reading Zoom | 1.5x, 2x, 2.5x | 2x |
| Top Map Zoom | Far (25%), Wide (35%), Close (50%) | Wide (35%) |
| Top Map Refresh | 10 Hz, 15 Hz, Every Frame | 15 Hz |
| Performance Stats | On, Off | Off |
| L Button Action | Right Mouse Button, Use Health Potion, Controller Shoulder, Disabled | Right Mouse Button |
| R Button Action | Use Health Potion, Right Mouse Button, Controller Shoulder, Disabled | Use Health Potion |
| Sound Effects | On, Off | On (experimental) |

Map percentages refer to map scale: lower values show more area. The new default is wider than R4. Reading zoom is relative to the complete bottom overview. L right-click uses the current pointer; L+touch moves the pointer without also left-clicking. Press L+R together in menus for the reader gesture. A brief 120 ms menu-only delay lets the chord cancel a pending right-click; gameplay right-click is immediate.

Experimental SFX use a 1 MiB bounded cache; long clips and MP3 replacements may be skipped. Music and movie audio are unavailable. For a silent fallback before boot, put `Sound Effects=0` under `[Nintendo DSi]` in `diablo.ini`.

## Renderer optimization (R6 test5)

This build routes palette-mapped sprite pixels and transparency blending through
four unrolled ARM kernels in ITCM. The overview/menu presenters also run from
ITCM, using aligned 32-bit VRAM stores with a halfword fallback for other row
alignments. No byte writes or DMA are used for VRAM. Palette results and the
640x480 layout remain the same. Sprite/tile/scene renderer files use `-O2`;
other engine code retains size optimization.

The final ELF places all six kernels in the standard Calico-loaded ITCM section,
using 3640 of 32736 available bytes including library code. Compared with test4,
text grows 7428 bytes and BSS 8 bytes; there is no new asset cache allocation.
Host tests compare every blend color pair using an asymmetric palette table,
lighting maps, odd/zero/tail lengths, alignments, guard regions and all displayed
pixels. A linked-ARM check verifies section placement and both VRAM store widths.
These checks establish correctness/placement, not a measured hardware speedup.
The user reports test4 helped, with roughly 7.4-8 FPS in a slow scene; compare
this build in the same scene with identical settings before estimating gains.

## Performance measurements (R6 test4)

Settings -> Nintendo DSi -> Performance Stats shows gameplay FPS on the top map.
Draw and Map are average CPU milliseconds per gameplay frame. Copy B and T are
bottom/top display presentation costs, also averaged per gameplay frame; skipped
map work counts as zero. Numbers update about twice a second. The overlay uses
44 rows of the top map and is hidden during menus, movies and loading. It does
not write performance samples to the SD card. FPS includes ordinary frame-rate
limiting and time between gameplay frames; the stage totals do not include all
game logic or waiting. Statistics are disabled by default.

Top Map Refresh gates both drawing the automap and copying it into top VRAM.
It does not cap bottom-screen FPS. 15 Hz is the default; 10 Hz reduces map work
further; Every Frame restores the earlier cadence for comparison. Changes to
map scale, refresh, statistics, or the top view force a fresh map. If gameplay
runs below the chosen map rate, the map updates at the available frame rate.
A short load/menu transition always invalidates the old map.

The presenters cache horizontal/vertical scaling tables separately for overview
and menu reader. Native 256x192 maps take a direct paired-byte copy path. Source
alignment and pitch may vary; VRAM still receives halfword writes. No DMA/cache
remapping, Wi-Fi RAM, lower internal resolution or new SD cache is enabled.
Those changes should follow measured bottlenecks. No hardware FPS gain is yet
claimed; the host suite checks output pixels, scheduling and timer arithmetic,
not DSi speed. Compare 10/15 Hz against Every Frame in the same busy scene.

## Build on Windows

Install devkitPro's DS development tools, plus native Windows CMake and Ninja. The tested compiler is devkitARM GCC 16.1.0. Use native Windows CMake rather than the MSYS version for this toolchain.

```powershell
.\Build-DSi.ps1 -DevkitPro 'C:/devkitPro' -CMake 'C:/path/to/cmake.exe' -Ninja 'C:/path/to/ninja.exe'
```

The first configuration fetches the pinned engine dependencies. SDL 1.2.15 source is included under `3rdParty/SDL12`. The expected output is `build-dsi/devilutionx-dsi.nds`, with generated `assets` and `mods` folders.

See [DSI_PORT.md](DSI_PORT.md) for the implementation, diagnostics, revision history, and test instructions. [DSI_BUILD_INFO.json](DSI_BUILD_INFO.json) describes the tested local R6 test3 cross-build; it is not a promise that a later build will have the same hash or memory footprint.

## Install on DSi

Copy the generated NDS to the SD card and put the generated support folders and your own Diablo data in this layout:

```text
SD:/devilutionx-dsi.nds
SD:/devilutionx/DIABDAT.MPQ          # supply your own
SD:/devilutionx/assets/...
SD:/devilutionx/mods/...
```

Launch through TWiLight Menu++ in **DSi mode**. Keep the support files and NDS from the same build. The repository does not include Diablo's commercial MPQ data or user save files. Builds and download archives are not tracked as source files.

A confirms, B goes back, and START can confirm or open the game menu. D-pad movement and the remaining buttons use the engine's controller system. Touch taps act as left mouse clicks on the bottom image; a dedicated touch UI is future work.

If the program stops, copy `dsi-startup.txt` before relaunching; each launch resets that trace. A handled engine error may also create `dsi-error.txt`. Preserve existing saves before testing.

## Attribution and license

The engine is from [diasurgical/DevilutionX](https://github.com/diasurgical/DevilutionX). The original project README is preserved as [UPSTREAM_README.md](UPSTREAM_README.md). The supplied snapshot did not contain Git history, so this initial import does not reconstruct upstream commit history.

See [LICENSE.md](LICENSE.md) for the upstream Sustainable Use License and the individual third-party notices. SDL 1.2.15 is LGPL-2.1-or-later; its complete source and license are retained. No claim is made that the entire source tree has one uniform license.

Upstream CI workflow files are preserved in `.github/upstream-workflows/` as references. They are not enabled automatically in this DSi repository; no hosted DSi build workflow has been validated yet.
