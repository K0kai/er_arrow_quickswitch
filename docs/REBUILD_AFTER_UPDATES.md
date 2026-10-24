# Rebuilding after updates

## Build requirements

Follow the source build instructions in the repository README. Dependency source
and licenses are included under `third_party`. A game installation is not needed
to compile or run the automated checks. Local profiles and launchers are outside
the public source tree.

## If a mod updates

The wheel reads active item names and weapon parameters rather than using a
per-mod ammo table. Test all four slots, normal/great arrows, bolts/ballista bolts,
paging, cancellation, item names and artwork against the updated mod. Unusual
changes to equipment, message or parameter structures may need an adapter.

## If Elden Ring updates

Version 0.6.3 supports Elden Ring **1.17.2**, executable **2.7.2.0**. The DLL refuses
other executable versions. Recompiling or changing the version check alone does
not establish compatibility.

Review `supported_version()`, signature resolution, the HUD callback, inventory
and equipment fields, the equip call, parameter access, sound calls and message
lookup against the new executable. Keep game and mod installations read-only
during inspection. Only then adjust the version guard and affected code.

Run `build.cmd --test` for a separate test DLL. After the automated checks pass,
test in-game on each setup you intend to support, including physical controller
input. Update the README and both Nexus descriptions together after compatibility
has been confirmed. `build-release.cmd` rebuilds and packages a ZIP when ready;
ordinary builds do not create a release.
