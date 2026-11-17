# DevilutionX DSi R9 test2

**Map Shortcut Uses Top Map** defaults to On under Nintendo DSi settings. The existing Toggle Automap shortcut and controller map action toggle the Top Map setting. On the default DSi bindings, hold START and press Down for the controller map action. Switch this option Off to use the bottom-screen automap controls.

**Highlight Enemies** defaults to Off. Enable it to outline visible enemies in blue and the enemy selected by the cursor or controller targeting in red. Friendly golems, noncombat dialogue monsters and unselected town NPCs use normal drawing. Outlines respect the normal lighting and hidden-monster checks. Highlighting adds drawing work; an eight-entry outline cache reuses recent sprite frames.


R9 adds Fast Drawing under Settings → Nintendo DSi, immediately below Native Scene Samples.

- **Balanced** is the default. It samples the world on a 256×96 grid and duplicates rows to fill the DSi display.
- **Fastest** samples a 128×96 world grid and expands each sample to a 2×2 block.
- **Off** uses R8's 256×192 sampling.

The camera uses the same logical coordinates and field of view. Approximation is scoped to terrain, actors and effects in the packed gameplay renderer. Text, item labels, cursor and HUD are drawn after expansion using the full display grid. Menus use the existing reading view and the top map uses its own buffer.

Enable Native Scene Samples and turn Graphics Zoom and Per-Pixel Lighting off to use the packed renderer. When a mode requires the logical canvas, such as menu reading, the bottom-screen automap or the death screen, the regular rendering path is used.

Balanced makes the world coarser vertically. Fastest also makes it coarser horizontally, and very small details may disappear. Sampling less of the scene reduces pixel and lighting work, while traversal and game logic still take time. The DSi FPS gain needs hardware measurement.

Extract the runtime ZIP to the root of your SD card. Keep your DIABDAT.MPQ, diablo.ini and saves, then launch devilutionx-dsi.nds in DSi mode from TWiLight Menu++. If you already have matching support files, replace just the .nds.

Compare Off, Balanced and Fastest in the same scene with Performance Stats enabled. Compare FPS and Draw time. The R8 ARM9 loops, Diablo launcher icon, sound preloading and session reload changes are included.

The devkitARM build, ARM instruction-memory checks and host regressions passed. The fast-drawing test compares 168000 layered spans with an independent sampled reference, including cached sprites, blending, odd viewport heights, UI samples, HUD composition, mode changes and menu fallback. R9 has not yet been tested on a DSi.
