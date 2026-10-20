# Build and review instructions

## 1. Install the build tools

Use **64-bit Windows 10 or Windows 11**. Install:

1. **Python 3.11 or newer** from https://www.python.org/downloads/windows/.
   Enable the installer option that makes `python` available on PATH.
2. **Visual Studio 2022 or newer Build Tools** from
   https://visualstudio.microsoft.com/downloads/ (under Tools for Visual Studio).
   Select **Desktop development with C++**, including the **MSVC x64/x86 build
   tools** and a **Windows 10 or Windows 11 SDK**.
3. **Git for Windows**, if cloning the repository. Downloading the repository's
   source ZIP from GitHub and extracting it also works; Git is not used by the build.

No Python packages, CMake, WinRAR, game installation or external mod profile are
required. Dependency sources and licenses are included under `third_party`:
Dear ImGui 1.91.9b, MinHook 1.3.4 and the miniz 3.0.2 standalone inflater.
Documented local dependency changes are described in their included README files.

## 2. Get the source

Clone the repository, or extract its source ZIP:

```bat
git clone https://github.com/K0kai/er_arrow_quickswitch.git
cd er_arrow_quickswitch
```

For a source ZIP, open Command Prompt in the extracted folder containing
`build.cmd`. Reopen Command Prompt after installing Python/build tools so PATH
changes take effect. Check Python:

```bat
python --version
```

## 3. Build

Close the game if replacing an installed DLL, then run:

```bat
build.cmd
```

The script discovers Visual Studio using its bundled `vswhere` tool and sets up
the x64 compiler. It also works from an x64 Native Tools Command Prompt. If needed,
set `AQS_VCVARS64` to the full path of your installation's `vcvars64.bat`.

The build generates embedded default configuration and Windows version metadata,
compiles the included dependencies and mod source, and runs the automated tests.
A successful run exits with code 0 and reports the output DLL. Any failed build
or test stops the script with a nonzero exit code.

**Output:** `dist/ArrowQuickSwitch-0.6.1.dll`.

The tests cover configuration migration, keyboard/controller bindings, wheel
selection, switching policy, text fitting, icon formats, hooking with Control
Flow Guard enabled, and D3D12/WARP rendering. Graphics checks run on the software
WARP adapter, including HDR10/scRGB paths, without launching Elden Ring.

To preserve an existing DLL and build a separate test copy instead:

```bat
build.cmd --test
```

**Test output:** `dist/ArrowQuickSwitch-testing.dll`.

Generated objects, executables and preview images remain under `build`; temporary
compiler files use `tmp`. Builds do not edit game files or local ME3 profiles and
do not create a release ZIP.

## 4. Check a binary

After a normal build:

```bat
python tools\verify_binary.py dist\ArrowQuickSwitch-0.6.1.dll
python tools\smoke_test_switcher.py dist\ArrowQuickSwitch-0.6.1.dll
```

Use `dist\ArrowQuickSwitch-testing.dll` in both commands for a test build.
The first checks PE protections and version metadata without loading the DLL.
The second loads it in an isolated Python process and verifies that unsupported
hosts are rejected before hooks are installed. Neither launches the game.

To record the output's SHA-256 from PowerShell:

```powershell
Get-FileHash .\dist\ArrowQuickSwitch-0.6.1.dll -Algorithm SHA256
```

The [security review](docs/BUILD_SECURITY_REVIEW.md) records the previously tested
DLL hash and scan results. Rebuilds are not guaranteed to be byte-identical:
compiler/SDK versions, linker timestamps and the release/test filename can change
the hash. Record the compiler version and source commit when comparing builds.

## 5. Package for distribution

When ready to distribute, run:

```bat
build-release.cmd
```

This runs the normal build and tests, then creates
`release/ArrowQuickSwitch-0.6.1.zip` using built-in Windows/.NET ZIP support.
It verifies the archive layout and checks every compressed file against the staged
original using SHA-256. It packages the DLL, documentation and dependency licenses
in `licenses/`, and never includes the user's INI or development launchers.

Extracted game artwork is excluded from the source repository. An optional local
legacy effect-badge sheet is packaged only when present under `assets`. Without
it, the wheel uses built-in fallback symbols; arrow/bolt item artwork is loaded
from the user's game/mod installation at runtime.

## In-game verification

Follow the ME3 entry in the repository README and launch offline on the supported
**Elden Ring 1.17.1 / executable 2.7.1.0**. Other versions, including 1.17.2, are
rejected. Check all four wheels, paging/cancellation, F10 settings input, saved
bindings, artwork, sounds and normal input after closing the panel. Automated
checks alone do not establish compatibility with a new game or mod version.
