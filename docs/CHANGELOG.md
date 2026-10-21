# Unreleased

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
