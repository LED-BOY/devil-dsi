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
