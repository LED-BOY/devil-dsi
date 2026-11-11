# DevilutionX DSi R8 ARM3 test build

On DSi, Load Game from the pause menu now exits the active session and opens the selected character's save through the title-screen loading path. It releases the old level, cursor state and cached audio before loading. The reload request itself does not save the current game. If the console resets again, copy `sd:/devilutionx/dsi-startup.txt` and `dsi-error.txt` (if present) before starting the game again.

The boot console and startup log display the compiled version, R8-arm3. The engine warms enemy sounds during level loading. During combat, it spaces cold enemy sound loads by at least 150 ms and skips them if all sound channels are busy. This can make level loads a little longer and can skip some uncached enemy effects in busy fights.

Turn on Performance Stats under Nintendo DSi settings to see the `Peak` line. It holds the slowest frame since stats were enabled, in milliseconds, with `L` for game logic and `D` for drawing on that frame. Toggle Performance Stats off and back on before testing a crowded room to clear the peak. Menu and loading gaps are excluded.

This test build runs the packed scene blitter from ARM9 instruction memory. Opaque and palette-mapped pixels are copied in pairs using the fixed two/three-pixel source spacing. Solid blended spans use the palette row directly. Aligned screen rows use ARM block loads and stores to copy eight words per loop into video RAM, with the original fallback for unaligned rows.

R8 draws the gameplay viewport into a packed buffer at the DSi screen's 256×192 pixel grid. The camera and field of view match R7. The bottom screen copies the native scene and combines it with the existing HUD. The top screen still shows the map during gameplay and the reading view in menus. The launcher banner uses the supplied Diablo artwork.

Native Scene Samples is enabled by default under Settings → Nintendo DSi. The packed path requires the standard 640×480 layout with Graphics Zoom and Per-Pixel Lighting off. Menus, the bottom-screen automap, the Map of the Stars and the death screen use the logical canvas.

Extract the runtime ZIP to the root of your DSi SD card and launch `devilutionx-dsi.nds` in DSi mode from TWiLight Menu++. Keep your existing `DIABDAT.MPQ`, settings and saves. If the matching support files are already installed, you can replace just the .nds.

R8 passed the ARM build and host renderer tests. Hardware FPS is still unmeasured. For a comparison with the original R8 build, use the same character and area, with Native Scene Samples on and the same map and sound settings. Compare FPS, Draw and Copy B on the top screen with Performance Stats enabled.
