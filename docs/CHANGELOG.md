# 0.7.0

- Add radial, horizontal strip, side list, and quick strip selection designs.
- Add optional tap-to-cycle and hold-to-open controls, with per-style release behavior.
- Keep the camera still during mouse selection in the radial wheel by default; allow disabling this in settings.
- Improve text clarity, item-artwork filtering, wheel contrast, and selection-marker edges.
- Cache wheel metadata for faster repeated opening and log slow opening phases.
- Improve icon-source fallback and report which source supplied artwork.
- Add confirmation feedback after a verified equipment change. Native HUD and camera-input experiments remain optional and disabled by default.
- Preserve existing INI values and add missing settings automatically. Automated build and graphics checks pass; broader in-game validation remains open.
- User-confirmed compatibility with Convergence 3.0.4 on Elden Ring 1.17.2.

# 0.6.3

- Support Elden Ring 1.17.2 (executable 2.7.2.0), tested in-game on vanilla
  and Reforged 2.3.7.1.
- Correct the live HUD vtable guard for 1.17.2 and recognize empty ammo slots
  whose handle is zero. Empty slots can be filled from a wheel, regardless of
  CycleSameFamily; the setting applies again after ammo is equipped.
- Convergence 3.0.3.0 does not yet have a release for Elden Ring 1.17.2.
  Physical controller input remains untested in-game on 1.17.2.
- Restore wheel opening and hover sound cues on 1.17.2 using the game's menu
  event call; confirmed audible in Reforged 2.3.7.1.

- Add a configurable controller chord to toggle the settings panel, defaulting to Back+Start.

- Keep the DLL filename stable as ArrowQuickSwitch.dll, with its version in Windows file details.

- Click a binding to capture a keyboard key, mouse button or controller chord.
- Capture modifiers, ignore activation inputs, and cancel with Escape or focus loss.
- Keep manual binding entry available and validate conflicts before saving.
- Capture controller bindings even when no controller wheels were enabled at startup.

# 0.6.2

- Track keyboard and mouse-button states from game-window events instead of
  `GetAsyncKeyState` polling; clear held inputs on focus loss.
- Keep shortcuts active before settings-panel creation and across graphics resets.
- Build hook trampolines in writable memory, then seal them read/execute.
- Batch HUD/input hook activation and retain trampolines safely after failed
  activation rollback.
- Validate compressed icon headers, texture names and archive offsets more
  strictly; clear partial data after failed reads or decompression.
- Runtime arrow/bolt icon extraction remains enabled in normal builds.

Automated checks and the author's in-game test passed for version 0.6.2.
Existing INI values are preserved.

# 0.6.1

- Settings use a raw-input virtual cursor, so the game's system-cursor recentering cannot trap it. Clicks and scrolling use the same input path.
- Long wedge and center names shrink to fit complete lines rather than drawing a cut-off final line.
- Brighter text/artwork, soft icon backdrops, selection glow, optional gentle animation, and an ornamental outer border. Each has an in-game appearance control.
- Settings panel uses an installed Windows font and a warmer color palette.

- Keyboard and controller bindings, including controller paging, can be edited
  in-game and applied after saving without restarting.
- Control Flow Guard, DLL version metadata and narrower decoder DLL loading.
- Source-only public repository with included build dependencies and portable
  build scripts.

Existing INI values are preserved; missing appearance options are added automatically.
