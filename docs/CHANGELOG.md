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
