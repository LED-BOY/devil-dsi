# DevilutionX DSi R10

Extract the SD-card ZIP to the root of your DSi SD card. Keep your own DIABDAT.MPQ in sd:/devilutionx/ and launch devilutionx-dsi.nds with TWiLight Menu++ in DSi mode. For an existing R9 installation you can replace just the NDS.

* Balanced Fast Drawing samples directly into alternate screen rows, then fills the gaps. It keeps the R9 field of view and sampling pattern.
* In single player the top map reuses its last image while the player stands still. Turning, exploring, changing zoom, and picking up or dropping visible items update the map. The frame statistics can refresh independently.
* Projectiles reuse tile lists between frames and preserve their drawing order.
* Monster sounds share the existing 1 MiB PCM cache. Attack and hit sounds take initial priority during level loading. The port counts requests for missing clips during play and favors those sounds when preloading them again during the session.

The maintainer tested R10 on a Nintendo DSi and reported that it worked fine.

To compare performance, use the same save and DSi settings in R9 and R10 with Balanced Fast Drawing and the top map enabled. Compare FPS and Draw time while standing and walking, then compare Peak frame time in a fight with several enemies or projectiles. Hardware FPS and frame-time improvements have yet to be measured.
