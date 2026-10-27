# devil-dsi

An experimental, unofficial Nintendo DSi port of [DevilutionX](https://github.com/diasurgical/DevilutionX), based on the supplied **1.6.0-dev** source snapshot.

**This is a modified DevilutionX source tree.** Upstream engine code, assets, authorship, and license notices are retained. This project is not an official DevilutionX release.

## Current status: R3

- Builds an actual DS/DSi `.nds` executable that calls the game engine.
- Game graphics on the **bottom display**, diagnostics on top.
- 640x480 indexed rendering downsampled to 256x192, with corrected SDL hardware-palette handling.
- Nintendo button mappings and absolute touch-to-left-click input.
- Fixed single-thread SDL event queue handling: previously the D-pad's held state worked while queued button, touch, and progress events were dropped.
- Single-player target; sound and networking are disabled.
- Persistent startup/loading trace at `sd:/devilutionx/dsi-startup.txt` and handled error output at `sd:/devilutionx/dsi-error.txt`.

The standalone hardware probe ran on a DSi with SD and touch access. Earlier engine builds reached the menu. **R3 still needs hardware verification of the input fixes, town loading, gameplay, and save/reload.** Desktop tests exercise the real SDL core/driver with simulated hardware, including all 12 button press/release pairs, held states, touch, queued progress events, palette conversion, and every displayed pixel. They do not replace hardware testing.

## Build on Windows

Install devkitPro's DS development tools, plus native Windows CMake and Ninja. The tested compiler is devkitARM GCC 16.1.0. Use native Windows CMake rather than the MSYS version for this toolchain.

```powershell
.\Build-DSi.ps1 -DevkitPro 'C:/devkitPro' -CMake 'C:/path/to/cmake.exe' -Ninja 'C:/path/to/ninja.exe'
```

The first configuration fetches the pinned engine dependencies. SDL 1.2.15 source is included under `3rdParty/SDL12`. The expected output is `build-dsi/devilutionx-dsi.nds`, with generated `assets` and `mods` folders.

See [DSI_PORT.md](DSI_PORT.md) for the implementation, diagnostics, revision history, and test instructions. [DSI_BUILD_INFO.json](DSI_BUILD_INFO.json) describes the tested local R3 cross-build; it is not a promise that a later build will have the same hash or memory footprint.

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

