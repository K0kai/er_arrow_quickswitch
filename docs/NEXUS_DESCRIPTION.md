<h1 align="center">Arrow Wheel</h1>
<p align="center"><em>Quick-switch arrows and bolts with radial wheels, no inventory needed</em></p>
<p align="center"><strong>Version 0.6.2</strong></p>

---

## What it does

Hold a key, point the mouse toward the ammo you want, and release. That's it. Arrow Wheel adds **four wheels**, one for each arrow slot and one for each bolt slot, with optional controller support.

- **Wheels:** arrow slots 1 and 2 on **1** and **2**, bolt slots 1 and 2 on **3** and **4** (rebindable).
- **What's shown:** only ammo you actually own. Arrow wheels list arrows and greatarrows, bolt wheels list bolts and ballista bolts. Empty stacks and ammo already equipped in the paired slot are hidden.
- **Info on each wedge:** name in your game language, quantity, item artwork, and damage/status badges. Multiple effects can appear together.
- **Many arrows?** More than eight types are split into pages. Hold the wheel key and scroll **up** for the next page, **down** for the previous one.
- **Works with modded ammo:** names and effects are read from the running game, so there are no fixed ammo ID lists and no Convergence-specific tables.
- **Controller (optional):** hold a bound button, aim with the right stick, release. Page with **RB**/**LB**.

---

## Requirements

- Windows Elden Ring **1.17.1** (exe version 2.7.1.0). **Patch 1.17.2 is not supported yet.** The mod checks the game version before loading.
- **Mod Engine 3 (ME3)**
- Offline play only. Do not use this DLL online.

**Version 0.6.2 has been tested in-game.**

**Tested setups:** The Convergence 3.0.3.0 (four wheels, cross-family arrow selection) and Reforged 2.3.5.3 (secondary-slot switching, paged wheels).

**Not fully tested:** vanilla (checked offline only) and physical controllers (simulated tests only).

Other mods may work if they keep the game's standard equipment and item data. Conflicts are possible with other DLLs that touch the same HUD, input or equipment code.

---

## Installation

1. Set up a working ME3 profile for your vanilla or modded Elden Ring. Close the game.
2. Extract the download anywhere you like and keep this layout. If an `assets` folder is included, keep it next to `dist` for effect badges.

```text
   ArrowQuickSwitch/
     dist/
       ArrowQuickSwitch-0.6.2.dll
     assets/
       wheel-game-icons.rgba
```

3. Add this to **your own copy** of the ME3 profile, replacing the path with where you extracted the mod (forward slashes):

```toml
   [[natives]]
   path = "C:/Mods/ArrowQuickSwitch/dist/ArrowQuickSwitch-0.6.2.dll"
   load_early = false
   initializer.function = "QuickSwitchInitialize"
```

4. Launch through that ME3 profile.
5. **Important:** open the equipment screen and equip some ammo in each slot you want to control, at least once. Then hold **1/2/3/4**, point, and release. Pointing at the center of the wheel cancels.

Nothing needs to be copied into the Elden Ring folder.

---

## Configuration

Press **F10** during gameplay to open the settings panel, powered by **Dear ImGui**. Open a wheel once to populate its preview.

- **Wheel and indicator:** customize colors, transparency, size, position, text, icons, selection lighting, and decorative trim. The ammo indicator can also be turned off.
- **Controls:** change keyboard and controller bindings for all four wheels and controller paging. Keyboard chords such as `Ctrl+E` and controller chords such as `LT+DPadUp` are supported.
- **Behavior:** toggle sounds and restrict each wheel to its equipped ammo type, if preferred.

Appearance changes preview immediately. Save to keep them and apply controls and behavior without restarting. Controller buttons still reach the game, so choose bindings that avoid gameplay conflicts.

The settings shortcut can also be changed or removed. Removing it disables the panel until you set a shortcut again under `[Controls] SettingsKey` in the INI.

The mod creates `dist/ArrowQuickSwitch.ini` on first launch. **Keep your existing INI when updating.** New settings are added automatically. The included `CUSTOMIZATION.md` explains the available appearance options and alternate icon source folders. For troubleshooting, `DiagnosticKey` defaults to **F8**.

Arrow and bolt artwork is loaded from installed game/mod files at runtime. Missing artwork falls back to effect badges.

---

## Limitations

- An empty or unrecognized slot needs ammo equipped manually once.
- Effect icons follow the active weapon and linked status data. Effects that another mod implements through bullets or other systems may be missed or need extra icon support.

---

## Planned

- Manual wheel reorganization (choose which ammo sits in each wedge)

---

## AI use disclosure

AI tools helped investigate the game's data, write and revise code, run checks, and draft documentation. I chose the features, reviewed the code, and tested the wheels and cross-family switching in-game.

Effect icons are derived from Elden Ring's in-game UI assets. This is an unofficial fan mod, not affiliated with or endorsed by FromSoftware, Bandai Namco, The Convergence team, or other mod authors.
