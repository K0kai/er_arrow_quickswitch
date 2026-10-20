"""Generate the built-in INI and migration key list from one default definition."""

import argparse
import configparser
from pathlib import Path
import re


DEFAULT_INI = """[Controls]
; Key names, modifier combinations, or legacy Windows virtual-key numbers.
; Examples: Ctrl+E, Ctrl+Shift+Q, Shift+Mouse4, Alt+Mouse5.
; Use Ctrl, Shift, Alt or Win before the primary key. Restart after changes.
; Extra modifiers prevent a match. Hold modifiers, then press the primary key.
; F8 = log current arrows/state.
; Switching is wheel-only. Legacy NextKey/PreviousKey entries are ignored.
; For a side mouse button use 5 (Mouse4) or 6 (Mouse5).
; The mod does not suppress these keys' existing game bindings.
DiagnosticKey=119
; Open the in-game settings panel during gameplay. None or blank disables it.
; Restore a binding here to enable the panel again. This shortcut reloads live.
SettingsKey=F10
; Hold to open the arrow wheel, move the mouse, release to equip.
Arrow1WheelKey=Key1
Arrow2WheelKey=Key2
Bolt1WheelKey=Key3
Bolt2WheelKey=Key4

[Controller]
; Optional XInput controller bindings. None disables that wheel's controller shortcut.
; Hold a button or chord, point with the right stick, release to equip.
; Examples: LB+DPadUp, LT+DPadRight, L3, View, Menu, A.
; XInput controller index is 0-3. Steam Input should expose an XInput controller.
; These buttons still reach the game; choose chords that do not conflict with gameplay.
ControllerIndex=0
Arrow1WheelButton=None
Arrow2WheelButton=None
Bolt1WheelButton=None
Bolt2WheelButton=None
; Set either page button to None if it conflicts with a wheel chord.
NextPageButton=RB
PreviousPageButton=LB
RightStickDeadzonePercent=25
TriggerThreshold=180

[Switching]
Enabled=1
; Wait for left/right mouse release before applying a pending switch.
DeferWhileMouseHeld=1
MouseReleaseDelayMs=150
RequestTimeoutMs=3000
; 0 allows normal/great arrows or bolts/ballista bolts in the same wheel.
; 1 only offers ammo of the type currently equipped in that slot.
CycleSameFamily=0

[Indicator]
; Enabled controls the whole overlay. ShowAmmoText hides only the text below the wheel.
Enabled=1
ShowAmmoText=1
AlwaysShow=0
DurationMs=3500
ScalePercent=100
FontFamily=Cambria
NameFontSize=22
DetailFontSize=11
TextColor=#F4EBD6
DetailColor=#C7B585
; Position relative to the wheel in reference pixels at 1080p.
Gap=24
OffsetX=0
OffsetY=0
FadeInMs=180
FadeOutMs=600

[Theme]
; Save changes with the wheel closed. Appearance reloads within about half a second.
; Colors use #RRGGBB. Opacity affects surfaces, not text or icons.
WheelOpacityPercent=72
WheelScalePercent=100
OffsetX=0
OffsetY=0
BackgroundColor=#141815
SelectedFillColor=#49402D
AccentColor=#A18C61
SelectionColor=#E1C484
TextColor=#F4EBD6
MutedTextColor=#BDB49F
CountColor=#D9CFB9
SelectedTextColor=#FFE6A6
FontFamily=Cambria
DetailFontFamily=Segoe UI
NameFontSize=18
CountFontSize=16
EffectFontSize=11
CenterFontSize=22
TextShadow=1
ShowIcons=1
ShowItemIcons=1
ItemIconScalePercent=100
; Optional mod root containing menu/hi or menu/low. Blank uses loader/game files.
IconSourceDirectory=
ShowEffectIcons=1
ShowEffectLabels=0
ShowCounts=1
IconScalePercent=90
MarkerScalePercent=100
ShowSelectionMarker=1
BorderScalePercent=100
BrightnessPercent=100
ContentBrightnessPercent=115
SelectionGlowPercent=70
IconBackdropOpacityPercent=45
DecorativeRings=1
AnimateSelection=1
; HDR only: brightness of UI white, in nits.
HdrPaperWhiteNits=200
; Cancelling uses this mouse distance; the drawn center matches it (30-140).
CancelRadius=78
; Optional 64x64 transparent PNG, relative to the DLL folder or an absolute path.
SelectionImage=

[Audio]
; Use a game menu cue when the wheel opens and a wedge is highlighted.
; Set to 0 for a silent wheel. No separate sound files are needed.
Enabled=1
; Limits how often the hover sound can play while moving across wedges (50-300 ms).
HoverCooldownMs=90
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--header", action="store_true", help="write a C++ header for the mod")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build_script = (root / "build-switcher.cmd").read_text(encoding="utf-8")
    versions = set(re.findall(r"ArrowQuickSwitch-(\d+\.\d+\.\d+)\.dll", build_script))
    if len(versions) != 1:
        raise ValueError("Expected exactly one mod version in build-switcher.cmd")
    version = versions.pop()
    document = DEFAULT_INI + f"\n[Meta]\nConfigVersion={version}\n"
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read_string(document)
    assert config["Switching"].getint("CycleSameFamily") == 0
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.header:
        settings = [
            f'    {{L"{section}", L"{key}", L"{value}"}},'
            for section in config.sections() if section != "Meta"
            for key, value in config[section].items()
        ]
        header = (
            "#pragma once\n#include <array>\n"
            "namespace arrow_switch::default_config {\n"
            "struct Setting { const wchar_t* section; const wchar_t* key; const wchar_t* value; };\n"
            f'inline constexpr wchar_t version[] = L"{version}";\n'
            f'inline constexpr char ini[] = R"AQSINI({document})AQSINI";\n'
            f"inline constexpr std::array<Setting, {len(settings)}> settings{{{{\n"
            + "\n".join(settings)
            + "\n}};\n}\n"
        )
        args.output.write_text(header, encoding="utf-8")
    else:
        args.output.write_text(document, encoding="utf-8", newline="\r\n")


if __name__ == "__main__":
    main()
