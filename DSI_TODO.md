# Next DSi version — R6 verification

Source implementations are present and the SDL host regressions pass. The items
remain unchecked until a cross-build and DSi testing confirm them. R6 test2 has been cross-built locally; `DSI_BUILD_INFO.json` records its binary.
Audible playback and all hardware checklist items remain unverified.

- [ ] **Play videos on one screen only.** Check intro, normal completion,
  skipping, a missing movie, and in-game playback. Only the bottom screen should
  show the movie; the top should stay black through palette changes. Afterwards,
  verify the appropriate menu reader, gameplay map, or startup console returns.
- [ ] **Fix L right-click consuming a potion.** Right-click mode must emit no
  controller shoulder action. Record belt potion counts before and after press,
  hold, release, touch combinations (including shoulder-up before touch-up),
  cursor warps away from belt/inventory, and the L+R reader gesture. Ordinary
  right-click away from items must not consume a potion. Intentional right-click
  directly on a usable item retains the engine's normal use behavior. The source
  now refreshes the item hit test at click time to avoid stale controller focus;
  the reported R5 hardware issue still needs reproduction and confirmation.
- [ ] **R Button Action in Settings -> Nintendo DSi.** Check Right Mouse Button,
  Controller Shoulder (the default), and Disabled. Verify changes apply live and
  persist after exiting settings and restarting. Change mappings while held and
  confirm the original action releases without firing the new action. Test both
  shoulders mapped to right-click, overlapping holds, R+touch, quick/held menu
  clicks, and L+R reader gestures in either press/release order and all mappings.

Before release, cross-build with `Build-DSi.ps1`, test on a DSi, and record the
actual build details. Keep these boxes unchecked until hardware verification.

Additional R6 test2 hardware checks:

- [ ] Save a game, wait at least four seconds without moving, and confirm the notification and its border disappear. Repeat while paused and after dismissing it.
- [ ] Load a save and change levels; confirm the top stays black until loading ends and then restores map/menu as appropriate.
- [ ] Check menu clicks, footsteps, combat and short voice effects; test panning, the in-game SFX volume and Sound Effects toggle. Capture the startup log if silent or unstable. Music/movie audio and long clips remain unsupported.

R6 test3: the slow-save duration underflow is reproduced and fixed in a host harness executing the actual save function. Verify the 1.5-second post-save notice on DSi. Verify default R uses one belt healing potion per press and that right-click/disabled/custom choices persist.


R6 test4 performance checks (hardware pending):

- [ ] Compare 10 Hz, 15 Hz and Every Frame in the same town/dungeon scene with
  movement and combat; confirm only the top map cadence changes.
- [ ] Enable Performance Stats and record FPS/Draw/Map/Copy B/T after two seconds.
  Repeat with sound effects off to isolate audio cost. Disable the overlay for
  normal play; it adds some measurement/drawing overhead.
- [ ] Check zoom changes, menu return, movies, loading, save/reload and map scale
  changes refresh the map immediately with no stale content.
- [ ] Use measured stage costs to choose the next optimization: renderer hot
  code/ITCM, a separately tested lower-resolution layout, or SD asset caching.
  Do not claim speed gains from host tests or allocate Wi-Fi RAM as game RAM.


R6 test5 hardware checks (pending):

- [ ] Compare with test4 in the same slow scene/settings; record FPS and
  Draw/Map/Copy B/T. The reported baseline is about 7.4-8 FPS.
- [ ] Inspect dungeon lighting, translucent walls, sprites, spell effects,
  menus/reader zoom, map and movies for identical colors and no new artifacts.
- [ ] Confirm entry/level changes/save/reload and long play remain stable with
  the ITCM kernels. Keep the test4 executable as the fallback.
- [ ] Use stage timings before selecting further resolution, renderer or SD
  caching work; no hardware FPS gain is inferred from host/linker tests.


R6 test6 hardware checks (pending):

- [ ] Toggle Top Map Off: ordinary gameplay top must be black, menu/panel reader
  must remain available, and loading/movie returns must respect the switch.
  Turn Performance Stats on to see timing text on black without the automap.
- [ ] Choose Scene Resolution 320x240 before entering/loading a game; confirm the
  closer camera, town/dungeon/combat rendering and unchanged complete HUD.
- [ ] Verify cursor/entity selection, ground item pickup, touch/right-click,
  inventory, character panel, stash and labels at 320 mode. Save/reload and
  compare 640 mode with the same map/audio/statistics settings.
- [ ] Restart to check Scene Resolution and Top Map persist. Use
  Scene Resolution=640 / Top Map=1 in [Nintendo DSi] for the default fallback.
