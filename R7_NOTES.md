# R7 native-sampling test build

Settings -> Nintendo DSi -> Native Scene Samples (On by default) draws the world using the same 256x192 sampling grid as the bottom display, keeping the existing camera and visible world area. It activates for the standard 640x480 canvas with Graphics Zoom off. Disable it for comparison with the previous R7 renderer.

This is selective world rasterization, not a complete replacement of the logical canvas: cached sprites and common terrain/sprite spans skip samples absent from the final display. A 48 KiB sample buffer reconstructs the logical world before drawing UI, preserving engine coordinates, input and the top reader. Per-pixel lighting and some special primitives still use their original paths. Menus and HUD retain their existing layout. The top reader may show coarser world graphics; its UI text is drawn afterwards normally.

Host tests compare 20000 layered spans (copy, fill, translation and blending) with full rendering at every sampled location and verify camera coverage, UI boundaries and pitch guards. This does not replace hardware verification of all engine effects. Check moving edges, transparency, outlines, lighting, both panels and top reading view. Compare FPS/Draw time with this option On and Off in the same scene. Extra processing for reconstruction means improved FPS is not guaranteed.

Keep the previous R7 binary as fallback. Install the complete matching runtime ZIP as usual, preserving DIABDAT.MPQ, settings and saves.

---

# R7 — visibility culling and sprite cache

R7 keeps the 640x480 layout and all R6 controls, sound-effects and top-map options. No DSTWO support or 320x240 scene mode is included.

## Rendering changes

- Reject off-screen terrain cells before light-table/bleed-lightmap setup; stop processing microtiles above the viewport and skip rows below it. Entity drawing and wall ordering remain independent because sprites can extend beyond their tile.
- Reject completely off-screen outlines before generating outline pixels.
- Cache decoded opaque sprite runs for direct and translated CLX draws, with clipped run copies and cached translated colors. The 16-entry, two-way cache uses approximately 200 KiB of static RAM, with no heap allocations. Sprites are admitted on repeat use.
- Limit entries to 4096 encoded bytes, 512 opaque runs and 256x256 dimensions. Larger sprites and blended/per-pixel-lit draws keep the original rendering paths.
- Compare source and translation-table contents to handle reused pointers, recoloring and lighting changes; clear the cache when the engine clears CLX drawing caches. This validation costs CPU time, so hardware FPS gains must be measured.

## Install and compare

Extract devilutionx-dsi-engine-r7.zip to the SD root, replacing the executable and matching support files. Keep DIABDAT.MPQ, diablo.ini and saves. Launch devilutionx-dsi.nds in DSi mode via TWiLight Menu++.

Compare R6 and R7 in the same scene, with identical map refresh, map visibility, sound and performance-stat settings. Check town/dungeon, moving near all screen edges, inventory/character panels, highlighted entities, lighting changes, save/load and level transitions. Final hardware verification is pending; no FPS improvement is promised.

## Validation

The host suite compares 12000 clipped cached sprite draws against an independent CLX decoder, including transparent overruns, fills/literals, pitch and guard bytes, changed source/table contents, cache clearing, run-budget fallback and invalid streams. Existing display, input, SFX and save-message tests are retained. Build/ROM validation results are recorded in DSI_BUILD_INFO.json and the source archive.
