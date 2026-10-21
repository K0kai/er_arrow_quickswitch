# Arrow Wheel

Four configurable arrow and bolt wheels for **Elden Ring 1.17.1** (executable
2.7.1.0), loaded through **Mod Engine 3** for offline play. **Patch 1.17.2 is not
supported.** The DLL checks the executable version before installing hooks.

Hold **1**, **2**, **3**, or **4** for the corresponding ammo slot, point with the
mouse, and release to equip. The center cancels; scroll to change pages. Optional
controller bindings use the right stick for selection. Equip ammo manually once
in each slot you want to control.

Press **F10** for the settings panel powered by **Dear ImGui**. It includes wheel
and indicator appearance, keyboard/controller bindings, sound and ammo filtering.
Appearance changes preview immediately; save to keep settings and apply controls
and behavior. The INI is created on first launch and gains missing defaults after
updates. **Keep your existing INI when updating.**

Names, ammo data and item artwork come from the running game and installed files.
The source repository contains no game binaries or extracted artwork. Missing
artwork falls back to effect symbols. See [customization](docs/THEMING.md).

## Build from source

For reviewers, see the [step-by-step build and verification guide](BUILDING.md).

Requirements:

- Windows x64.
- Visual Studio 2022 or newer C++ Build Tools with MSVC x64 tools and Windows SDK.
- Python 3.11 or newer available as `python`.

From the repository root, run:

```bat
build.cmd
```

The build discovers Visual Studio with `vswhere`, or uses an existing x64 Developer
Command Prompt. An explicit `AQS_VCVARS64` environment variable can point to your
`vcvars64.bat`. All compiled dependencies are included as source under `third_party`.
No game installation, external mod profile or downloaded dependency binary is
required to compile and run the automated tests.

Output: `dist/ArrowQuickSwitch.dll`. Close the game before replacing a loaded
DLL. To build a separate DLL without replacing it:

```bat
build.cmd --test
```

Output: `dist/ArrowQuickSwitch-testing.dll`. Builds run configuration, input,
controller, switching, text, icon-format, hook and D3D12/WARP rendering checks.
The renderer tests require Windows graphics APIs, including the software WARP
adapter; no running game is needed. The build also verifies PE security flags and
Windows version metadata.

## Install

Add the DLL to your own ME3 profile, adjusting the example path:

```toml
[[natives]]
path = "C:/Mods/ArrowQuickSwitch/dist/ArrowQuickSwitch.dll"
load_early = false
initializer.function = "QuickSwitchInitialize"
```

Launch through that profile, offline. Local launchers and generated profiles are
not part of the source repository. [Installation and compatibility details](docs/NEXUS_DESCRIPTION.md).

## Package

```bat
build-release.cmd
```

This rebuilds and tests the mod, then writes `release/ArrowQuickSwitch-0.6.2.zip`.
The ZIP contains the DLL, installation/customization documents and dependency
licenses under `licenses/`. It excludes the user's INI. An optional local legacy
badge sheet is included if present; source builds work without it, using fallback
symbols while item artwork is loaded at runtime.

## Review and maintenance

- [Build security review](docs/BUILD_SECURITY_REVIEW.md): exact reviewed hashes,
  protections, remaining scan detections and review limits.
- [Rebuilding after game updates](docs/REBUILD_AFTER_UPDATES.md).
- [Changelog](docs/CHANGELOG.md).
- [Manual wheel organization plan](docs/MANUAL_WHEEL_ORGANIZATION.md).

Dear ImGui, MinHook and miniz retain their upstream licenses under `third_party`.
The project's original code and documentation use the
[Arrow Wheel Attribution-Noncommercial License](LICENSE): use, modification,
and redistribution are allowed with credit to K0kai; commercial use or
monetization requires prior written permission. This includes Nexus Donation
Points and donations or advertising tied to the mod.
This is an unofficial fan mod and is not affiliated with FromSoftware, Bandai
Namco, or other mod authors.

The DLL filename stays `ArrowQuickSwitch.dll` across updates. Set your launcher profile to this filename once. Check its version by hovering over it in Windows Explorer or opening Properties > Details.
