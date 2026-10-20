# Manual wheel organization — implementation plan

Status: design only. No ordering behavior has been changed.

## Goal

Let players assign an ammo type to a specific wedge and page on each of the four wheels. Positions should stay predictable when inventory quantities change or other ammo becomes available.

## Current behavior

`game::snapshot` sorts owned ammo by item ID. `wheel::eligible` keeps that order while filtering for the active ammo family and excluding the type equipped in the paired slot. Each page has up to eight items. `wheel::selection` divides the circle by the number of items on the visible page, so positions shift on a short page.

## Proposed behavior

- Treat each page as eight fixed positions, numbered 0–7 clockwise from 12 o’clock. Empty positions remain visible and cannot be selected for equipping.
- Save assignments by **weapon item ID**, not inventory index, name, or handle. Names are localized and handles or inventory indices can change.
- Keep separate layouts for Arrow 1, Arrow 2, Bolt 1, and Bolt 2. An item may be assigned differently on each eligible wheel.
- On opening a wheel, place eligible assigned items in their saved positions. Fill unassigned positions with other eligible ammo in stable item-ID order, then add pages as needed.
- If an assigned item is unavailable, has zero quantity, or is excluded because it is equipped in the paired slot, reserve its position. The other positions do not move. An unassigned item must not take that reserved position.
- Keep the current center cancel area. Releasing over an empty wedge also cancels. Page controls retain their current behavior.
- If there are more items than fit after reservations, create additional pages. Show which page is active.

## Configuration and editing

Start with a separate, human-readable layout file beside `ArrowQuickSwitch.ini`. Each assignment needs a wheel, page number, position number, and decimal item ID. A separate file keeps the theme and key bindings simple and allows layouts to be shared. The precise file syntax can be chosen during implementation.

Validate the file when loading: reject duplicate positions or duplicate item IDs within a wheel, out-of-range positions, and IDs that are not valid numbers. Keep the prior/default order and log a clear error if the file is invalid. Unknown IDs can remain reserved so a layout survives a temporarily unavailable mod or item.

After the fixed-position logic is stable, add an in-game arrangement mode or a visual editor. That editor should display all four wheels and pages, allow moving an item to an empty position or swapping two items, and save the same layout file. Editing must not call the game's equip function.

## Implementation steps

1. Add a layout model and a pure placement function that accepts eligible ammo plus saved assignments and returns pages of eight optional item IDs.
2. Change hit testing to use eight fixed sectors. Empty sectors return cancel. Keep item IDs attached to rendered wedges so selection and equip target cannot diverge.
3. Update rendering to show empty wedges clearly and distinguish reserved positions from ordinary empty positions if useful.
4. Load and validate the saved layout at startup; document the format and provide a default/example file.
5. Test placement with missing items, zero quantities, paired-slot exclusions, extra pages, duplicate entries, and different layouts for the four wheels.
6. Check mouse and controller selection, paging, cancellation, and actual equipment switching in-game.

## Compatibility notes

Item IDs may change between a mod's releases. An old assignment should be ignored for equipping when that ID is absent, without remapping it to a different item by name. The layout can then be edited or reset. This feature only changes the wheel's presentation and selection mapping; it does not reorder the game's inventory or alter equipment records.
