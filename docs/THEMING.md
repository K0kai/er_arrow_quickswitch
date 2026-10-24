# Customization

Open a wheel once, release it, then press **F10** or **Back+Start** on a controller during gameplay. The settings panel previews your last wheel. Wheel and Indicator changes preview immediately; **Save changes** keeps them, **Revert** restores saved values, and closing discards unsaved edits. The panel does not pause the game.

The Behavior tab controls ammo filtering, sound, and the keyboard settings shortcut. The Controls tab edits keyboard and controller wheel bindings, controller paging, and the controller settings shortcut. Clearing both settings shortcuts and saving closes and disables the panel. Restore `[Controls] SettingsKey=F10` or `[Controller] SettingsButton=Back+Start` in the INI to enable it again. `None` disables a controller shortcut; `None`, `0`, or blank disables the keyboard shortcut. Shortcuts reload live.

The mod creates its INI on first launch and adds missing options after updates. Keep your existing INI when updating. Appearance, audio, ammo filtering, and the settings shortcut also reload after external edits within about half a second while no wheel or equip request is active.

## Rebinding controls

In the Controls tab, click the current binding and press the desired key or mouse
button. Hold Ctrl, Shift, Alt or Win first to record a keyboard chord. For a
controller binding, hold the desired buttons together, then release them. Press
Escape to cancel capture. Release inputs already held when capture starts.

Choose **Clear** to disable a controller binding or the keyboard settings shortcut. Clearing
both settings shortcuts takes effect after saving and disables the panel until one
shortcut is restored in the INI. Required keyboard wheel bindings cannot be cleared.
**Edit binding names manually** retains text entry for advanced bindings, including
Escape. Save validates the complete set of bindings before applying changes.

## Wheel appearance

These keys belong in `[Theme]`. Colors use `#RRGGBB`. Positions and font sizes use reference pixels at 1080p. Opacity affects surfaces independently of text and artwork; sizes fit uniformly to the screen.

| Keys | Defaults | Purpose |
| --- | --- | --- |
| `WheelScalePercent`, `WheelOpacityPercent` | `100`, `72` | Size (50–150%) and surface opacity (0–100%). |
| `OffsetX`, `OffsetY` | `0`, `0` | Position; negative moves left/up. |
| `BackgroundColor`, `SelectedFillColor` | `#141815`, `#49402D` | Wedge and selected fill. |
| `AccentColor`, `SelectionColor` | `#A18C61`, `#E1C484` | Borders and selection marker/outlines. |
| `TextColor`, `SelectedTextColor` | `#F4EBD6`, `#FFE6A6` | Item names and selected text. |
| `MutedTextColor`, `CountColor` | `#BDB49F`, `#D9CFB9` | Details and quantities. |
| `FontFamily`, `DetailFontFamily` | `Palatino Linotype`, `Segoe UI` | Installed Windows fonts. |
| `NameFontSize`, `CountFontSize` | `18`, `16` | Name and quantity size. |
| `EffectFontSize`, `CenterFontSize` | `11`, `22` | Detail and center size. |
| `TextShadow`, `ShowCounts`, `ShowEffectLabels` | `1`, `1`, `0` | Readability and label visibility. |
| `ShowIcons`, `ShowItemIcons`, `ShowEffectIcons` | `1`, `1`, `1` | All icons, actual ammo artwork, and effect badges. |
| `ItemIconScalePercent`, `IconScalePercent` | `100`, `90` | Artwork (50–150%) and badge (50–125%) size. |
| `ShowSelectionMarker`, `MarkerScalePercent` | `1`, `100` | Marker visibility and size (50–200%). |
| `SelectionImage` | blank | Optional 64×64 transparent PNG, relative to the DLL or absolute. Source points up; the marker rotates toward the wedge. |
| `BorderScalePercent` | `100` | Border thickness (50–200%). |
| `BrightnessPercent`, `HdrPaperWhiteNits` | `100`, `200` | Overall brightness (50–200%) and HDR white level (80–400 nits). |
| `CancelRadius` | `78` | Cancel mouse distance (30–140); the drawn center follows it. |
| `IconSourceDirectory` | blank | Optional mod root containing `menu/low` or `menu/hi`. |

Long names automatically reduce their font size to fit complete lines. The settings cursor uses raw mouse input so gameplay cursor recentering does not trap it.

Additional `[Theme]` controls: `ContentBrightnessPercent=115` brightens text/artwork independently (75–150), `SelectionGlowPercent=70` controls highlight lighting (0–100), and `IconBackdropOpacityPercent=45` gives artwork a soft backdrop (0–80). `DecorativeRings=1` enables the outer trim; `AnimateSelection=1` adds a gentle selected-wedge glow. All are available in the Wheel tab.

Item names retain the chosen font size in every selector. Wheel and strip labels
wrap and use an ellipsis when needed; list labels use a wider single line. The
strip also shows the selected name below its cards. Scroll the mouse wheel to
step through list or strip items; crossing the last or first entry on a page
changes pages automatically. The controller next/previous bindings perform the
same item steps. The radial wheel retains directional selection and page controls.
The strip displays four cards per page; the list and wheel display up to eight.

## Runtime item artwork

The mod reads each ammo item's icon ID from the live weapon parameters. A background worker reads the installed menu layout and texture, decompresses the container, decodes the BC texture, and caches a small image in memory. It reads only the named `menu/low/01_common` or `menu/hi/01_common` files and their relevant archive ranges. It does not unpack the game or write extracted pictures during play. New artwork may appear shortly after opening a wheel for the first time.

The strip and list also request the game's `MENU_SlotBase.png` and `MENU_FL_Cursor_Wide.png` sprites from those installed menu atlases. The first is used around each ammo image; the second marks the selected entry. These sprites load through the same cache and respect the configured icon source directory. They are not packaged with the mod.

Quantities in every selector use `MENU_SL_Arrow.png` or `MENU_SL_Bolt.png` before the number. These installed-game glyphs are tinted to the quantity text color. A small drawn bundle is used if the asset is unavailable.

Convergence and Reforged roots are detected from loaded native modules when possible. Loader/loose files and packed vanilla assets are also checked. If your replacement textures live elsewhere, choose the root containing the `menu` folder with `IconSourceDirectory` or the panel. Missing/unsupported artwork falls back to effect badges. No arrow/bolt artwork is shipped. An optional local `assets/wheel-game-icons.rgba` file supplies legacy damage/status badges; without it, the renderer uses fallback symbols. This game-derived sheet is not included in the source repository.

## Ammo indicator

The indicator shows only the selected ammo name on a transparent background. Quantity appears beside an arrow or bolt bundle glyph in every selector design. Its scale and placement are independent of the selector and remain consistent after closing it. `[Indicator] Enabled=0` retains its legacy meaning of disabling the overlay; use `ShowAmmoText=0` to hide only the indicator.

| `[Indicator]` keys | Defaults | Purpose |
| --- | --- | --- |
| `ShowAmmoText`, `AlwaysShow` | `1`, `0` | Visibility and persistent display. |
| `DurationMs`, `ScalePercent` | `3500`, `100` | Duration (500–30000 ms) and scale (50–200%). |
| `FontFamily`, `NameFontSize` | `Palatino Linotype`, `22` | Indicator name font and size. |
| `TextColor` | `#F4EBD6` | Indicator name color. |
| `Gap`, `OffsetX`, `OffsetY` | `24`, `0`, `0` | Gap below the wheel canvas and relative offsets. |
| `FadeInMs`, `FadeOutMs` | `180`, `600` | Smooth fade timing; 0 disables that fade. |

Legacy `Left` and `Bottom` values are preserved but no longer position this layout.

## Verification

Preview and game drawing share the same triangle geometry, colors, gradients, and text composition. Software GPU checks cover rendering/readback, the ImGui panel/font upload, resize, and SDR/HDR shader variants. The icon preview uses the same archive reader and texture decoder as the runtime worker. These checks do not replace an in-game test of input capture, mod asset selection, and appearance.
