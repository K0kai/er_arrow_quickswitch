# Build security review — 2026-10-20

[Build and verification instructions](../BUILDING.md).

## Scope and findings

The quarantined ZIP has SHA-256
`94960525081d03e4b2bea83299b796fd5a54622ea7ba6b43ff795f0cafe54943`.
Its DLL matches the installed 0.6.1 DLL, SHA-256
`9779b38b3fb92e418434e315e40d002a22422ffb4fb7e4b5ec105778abbd00a5`.
The archive contains one DLL, the effect-icon asset, documentation and licenses.
The linked imports are Windows graphics, input, shell, file/system and
cryptography APIs. No network library is imported; this is a limited static
observation, not proof that a binary is safe.

The DLL uses MinHook to intercept the game's HUD, raw input and Direct3D calls.
Upstream MinHook allocates writable executable trampoline memory, patches code
and temporarily suspends threads. These operations are expected for this mod,
but the VirusTotal report does not establish whether they caused its detections.
No hook behavior or antivirus visibility was removed to change scan results.

## Changes made

- Enabled compiler and linker Control Flow Guard. Stack protection is explicit;
  ASLR, high-entropy ASLR and DEP remain enabled and are now verified after linking.
- Added Windows version metadata identifying Arrow Wheel, its version, original
  filename and source repository. The DLL is still unsigned.
- Restricted the game Oodle decoder's dependency lookup to its own directory and
  Windows System32, instead of the broader default search directories.
- Included the x64 MinHook source and license under `third_party/minhook`.
  All eleven dependency files match the official 1.3.4 archive, allowing line-ending
  normalization. The build no longer depends on the ignored local vendor checkout.
- Added a detour/trampoline compatibility test under CFG and a PE protection check.
- Added `build.cmd --test` to produce a separate testing DLL without replacing
  the installed DLL or generating a release package.

Upstream reference: https://github.com/TsudaKageyu/minhook/tree/v1.3.4
Compiler reference: https://learn.microsoft.com/en-us/cpp/build/reference/guard-enable-control-flow-guard
Loader reference: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw

## Verification and limits

The test build passed configuration, input, binding, controller, switching,
names, icon-format, hook/trampoline and D3D12/WARP rendering checks, including
HDR10/scRGB rendering paths. Its PE protections and version metadata were checked.
The DLL loads and refuses an unsupported executable before installing hooks.
The author subsequently tested the hardened DLL in-game and reported it working.
Its [VirusTotal report](https://www.virustotal.com/gui/file/61c6a775ab348e5b8b62044b5fcf16f15a27ee9962128565c4dcdadd7602df97)
showed 3/71 detections: Google `Detected`, Ikarus `Trojan.Win64.Krypt`, and
Microsoft `Trojan:Win32/Wacatac.C!ml`. This was a direct DLL scan; the earlier
10/68 result was for a ZIP, so the counts are not directly comparable. Neither
result establishes safety or identifies the code responsible for a detection.

Test DLL: `dist/ArrowQuickSwitch-testing.dll`
SHA-256: `61c6a775ab348e5b8b62044b5fcf16f15a27ee9962128565c4dcdadd7602df97`

The original release ZIP was preserved. The hardened DLL was installed for the
author's in-game test after backing up the previous DLL locally. These changes improve
build identification, source availability and exploit mitigations; they do not
establish a false positive or guarantee fewer detections. Nexus/vendor review
still needs the exact flagged binary and source matching that build. The reviewed runtime source is included in this repository. Public-source cleanup
changes only build portability, documentation and packaging; no subsequent runtime
behavior change is part of that cleanup. New build hashes may differ from the
reviewed DLL and must be recorded separately.
