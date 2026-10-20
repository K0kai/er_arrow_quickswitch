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

## Follow-up: window input experiment

The current experimental source replaces the mod's `GetAsyncKeyState` polling
with state updated by the game window's keyboard and mouse-button messages.
The existing raw-input path also supplies mouse buttons when legacy messages
are unavailable. Raw movement, controller input and rendering remain unchanged.
The window handler attaches independently of panel creation and survives graphics
resource teardown. Focus loss clears held states; returning held-key repeats
cannot activate shortcuts until a fresh press. Dear ImGui's upstream backend
still uses `GetKeyState` for its own modifier handling.

The experiment passed state, real window-subclass, hook, graphics and DLL-host
checks. The compiled DLL imports were checked: `GetAsyncKeyState` is absent.
In-game verification and antivirus rescanning of this build are pending.

Experimental DLL SHA-256: `3735e2fd532d96530913b1755c70c6dcbdf265afcf86a508cab82d05d26e5b45`.
The previous hardened build is preserved locally for comparison and rollback.
These changes limit input tracking to the game window; no reduced detection
count is claimed.

## Follow-up: trampoline memory protection experiment

MinHook's persistent read/write/execute trampoline pool has been replaced with
a local allocator that commits one read/write page per hook, builds the
trampoline, then seals it read/execute and flushes its instruction cache before
publishing it. Mutable allocation metadata stays on the non-executable process
heap. New hooks never reopen an existing trampoline page for writes.

The near-address allocation search and hook/trampoline instruction handling are
retained. The memory tradeoff is a committed 4 KiB page per hook rather than a
shared pool of 64-byte slots, plus allocation-granularity address-space reservation.
Target function patches still temporarily require writable/executable access and
thread suspension. No input, HUD, Direct3D or resize hook has been removed.
The game executable does not directly import `GetRawInputData`, so replacing that
system-function hook with a simple game import-table change was not pursued.

Tests checked initial RW/non-executable buffers, sealed RX trampolines, CFG calls,
creating another hook while an original trampoline runs on a worker thread,
independent pages, removal, queued enabling, uninitialization and reinitialization.
The input, hook, PE-protection, unsupported-host and graphics checks passed.
In-game behavior and antivirus results for this build remain unverified.

Experimental DLL SHA-256: `5b1a946388932de94b118d315af34f34ac075d966b7bfee284f173585d56112d`.
The installed DLL and release archive were not replaced. Local source changes
have not been pushed. Details of modified dependency files are in
`third_party/minhook/README_AQS.md`.

## Follow-up: batched activation and callback lifetime

HUD and raw-input activation now share one `MH_ApplyQueued` call, reducing the
normal core startup suspension/resumption passes from two to one. Rendering
hooks already activate in one separate batch. Hook targets and count are unchanged.

After a partially failed activation, core and rendering hooks are queued for
disabling together. Their trampoline allocations are retained for process
lifetime, since an in-flight callback can still need its original trampoline
even after the function entry patch is removed. Before any activation attempt,
creation/queueing failures may still remove disabled hooks safely. Rollback
status is logged; failed core activation disables switching.

Real hook tests confirmed that two queued hooks do not change targets before
apply, activate/deactivate together, and retain callable RX originals after
disable. Configuration, input, controller, switching, graphics, PE-protection
and unsupported-host checks passed. Failure-path review does not constitute
injection testing of operating-system protection failures. In-game testing and
antivirus results for this build are pending; no reduced detection count is claimed.

Test DLL SHA-256: `02184db66abcc01f4f19728d43ee07c7c76a04238db8ac68a9c427debd298ddb`.
The installed DLL and release archive were preserved.

## Follow-up: runtime icon asset validation

The batched-hook DLL scan subsequently reported 2/71 detections (Google and
Ikarus); Elastic was undetected. This does not attribute a detection to a
specific component.

The next review focused on icon asset validation rather than changing cryptography
or imports to affect signatures. DCX headers now reject unsupported codecs, zero
compressed lengths and invalid output sizes before allocating the output buffer.
TPF texture names must terminate within their bounded encoding; partial UTF-16
and unterminated names are rejected. AES metadata offsets are checked before
addition/read. Archive read/decompression failure clears the output buffer so
callers cannot reuse malformed partial data as a cached asset.

Malformed-header/name tests and archive-file regression tests passed, including
corrupt compressed data and successful decompression of a known valid fixture.
Known arrow icon 11200 still extracted successfully from vanilla, Convergence and
Reforged using targeted archive reads. Existing input, hook, graphics, binary
protection and unsupported-host checks passed. This is correctness/security
hardening, not evidence that icon loading caused an antivirus detection.

Test DLL SHA-256: `e9c2cbaedb58707536affab3bd63a0f6a1068bcd48267bee411e4c45509c00d3`.
No release package or installed DLL was replaced. Antivirus rescan and in-game
verification of this build are pending; source changes have not been pushed.

## Temporary diagnostic build without item extraction

`build.cmd --test-no-icons` links a disabled provider in place of the item
artwork worker, archive reader/decryption/decompression, Oodle loader and D3D11
texture decoder. Normal and release builds keep full extraction support. The
compiled diagnostic DLL has no D3D11 or BCrypt imports, Oodle lookup string or
Data0 archive filenames. The normal test DLL hash remained unchanged.

Input, hooks, graphics, PE-protection and host-rejection tests passed. This
isolates the extraction subsystem as a group; a scan difference alone cannot
identify a specific routine or prove safety.

No-icons DLL SHA-256: `9f3f1f5058b5fb094dad270a83a5f5e9a22f9faccfc9c26f942a3f64aa4377da`.
No release was packaged; no installed DLL or INI was replaced.

## Version 0.6.2 release verification

The author confirmed that the full 0.6.2 build passed the in-game test and was
accepted by Nexus. These confirmations supersede the pending in-game status
recorded during the experiments above. Nexus acceptance is not a claim of zero
antivirus detections. Normal builds retain full item artwork extraction; the
no-icons mode remains an explicit diagnostic option only.

The locally verified full 0.6.2 DLL has SHA-256
`a5c2ab6f1cd702c98d421bd8e3da6071533a26a676fb0edb981732ef4d44070b`.
