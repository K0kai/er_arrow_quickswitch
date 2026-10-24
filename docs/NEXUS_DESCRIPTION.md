<h1 align="center"><font color="#6d9eeb">Arrow Wheel</font></h1>
<p align="center"><em>Quick-switch arrows and bolts with radial wheels, no inventory needed</em></p>

<p align="center"><a href="https://github.com/K0kai/er_arrow_quickswitch">Source code on GitHub</a></p>

---

## <font color="#6d9eeb">What it does</font>

Hold a key, point the mouse toward the ammo you want, and release. That's it. Arrow Wheel adds **four wheels**, one for each arrow slot and one for each bolt slot, with optional controller support.

- **Wheels:** arrow slots 1 and 2 on **1** and **2**, bolt slots 1 and 2 on **3** and **4** (rebindable).
- **What's shown:** only ammo you actually own. Arrow wheels list arrows and greatarrows, bolt wheels list bolts and ballista bolts. Empty stacks and ammo already equipped in the paired slot are hidden.
- **Info on each wedge:** name in your game language, quantity, item artwork, and damage/status badges. Multiple effects can appear together.
- **Many arrows?** More than eight types are split into pages. Hold the wheel key and scroll **up** for the next page, **down** for the previous one.
- **Works with modded ammo:** names and effects are read from the running game, so there are no fixed ammo ID lists and no Convergence-specific tables.
- **Controller (optional):** hold a bound button, aim with the right stick, release. Page with **RB**/**LB**. Press **Back+Start** to open or close settings.

---

## <font color="#6d9eeb">Requirements</font>

- Windows Elden Ring **1.17.2** (exe version 2.7.2.0). The mod checks the game version before loading.
- **Mod Engine 3 (ME3)**
- Offline play only. Do not use this DLL online.

---

## <font color="#6aa84f">Compatibility</font>

- **Vanilla Elden Ring 1.17.2**
- **<font color="#f1c232">Elden Ring Reforged 2.3.7.1</font>**
- **<font color="#6d9eeb">The Convergence 3.0.3.0</font>:** awaiting a version for Elden Ring 1.17.2.

Physical controllers have not been verified in-game on the supported game build.

Other mods may work if they keep the game's standard equipment and item data. Conflicts are possible with other DLLs that touch the same HUD, input or equipment code.

---

## <font color="#6d9eeb">Installation</font>

1. Set up a working ME3 profile for your vanilla or modded Elden Ring. Close the game.
2. Extract the download anywhere you like and keep this layout. If an `assets` folder is included, keep it next to `dist` for effect badges.

```text
   ArrowQuickSwitch/
     dist/
       ArrowQuickSwitch.dll
     assets/
       wheel-game-icons.rgba
```

3. Add this to **your own copy** of the ME3 profile, replacing the path with where you extracted the mod (forward slashes):

```toml
   [[natives]]
   path = "C:/Mods/ArrowQuickSwitch/dist/ArrowQuickSwitch.dll"
   load_early = false
   initializer.function = "QuickSwitchInitialize"
```

4. Launch through that ME3 profile.
5. Hold **1/2/3/4**, point, and release. A wheel can equip ammo into an empty slot. Pointing at the center cancels.

Nothing needs to be copied into the Elden Ring folder.

---

## <font color="#6d9eeb">Configuration</font>

Press **F10** or **Back+Start** on a controller during gameplay to open the settings panel, powered by **Dear ImGui**. Open a wheel once to populate its preview.

- **Wheel and indicator:** customize colors, transparency, size, position, text, icons, selection lighting, and decorative trim. The ammo indicator can also be turned off.
- **Controls:** change keyboard and controller bindings for all four wheels, controller paging, and the settings panel. Click a binding and press the desired key or mouse button; hold controller buttons together and release to capture a chord. Escape cancels. Manual name entry is also available. Keyboard chords such as `Ctrl+E` and controller chords such as `LT+DPadUp` are supported.
- **Behavior:** toggle sounds and restrict each wheel to its equipped ammo type, if preferred. An empty slot ignores this restriction for its first selection.

Appearance changes preview immediately. Save to keep them and apply controls and behavior without restarting. Controller buttons still reach the game, so choose bindings that avoid gameplay conflicts.

Settings are saved in `ArrowQuickSwitch.ini`. See the included `CUSTOMIZATION.md` for all available options.

Arrow and bolt artwork is loaded from installed game/mod files at runtime. Missing artwork falls back to effect badges.

---

## <font color="#6d9eeb">Limitations</font>

- Unknown or inconsistent equipment mappings are rejected to protect the current loadout.
- Effect icons follow the active weapon and linked status data. Effects that another mod implements through bullets or other systems may be missed or need extra icon support.

---

## <font color="#6d9eeb">Planned</font>

- Manual wheel reorganization (choose which ammo sits in each wedge)

---

## <font color="#6d9eeb">AI use disclosure</font>

AI tools helped investigate the game's data, write and revise code, run checks, and draft documentation. I chose the features, reviewed the code, and tested the wheels, cross-family switching, and empty-slot equipping in-game.

Effect icons are derived from Elden Ring's in-game UI assets. This is an unofficial fan mod, not affiliated with or endorsed by FromSoftware, Bandai Namco, The Convergence team, or other mod authors.
