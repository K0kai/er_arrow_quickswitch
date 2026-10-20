Pinned base: https://github.com/TsudaKageyu/minhook/tree/v1.3.4

Only the files needed for the x64 static build are included, with the upstream
BSD license. Before local changes, all eleven dependency files matched the
official v1.3.4 source archive (allowing CRLF normalization).

Local memory-protection changes:

- `src/buffer.c`: one dedicated virtual-memory page per trampoline, allocated
  read/write and then sealed read/execute. Allocation metadata is kept on the
  non-executable process heap; sealed code pages are never reopened for writes.
  The original near-address search helpers are retained. The tradeoff is one
  committed 4 KiB page per hook instead of sharing 64-byte slots.
- `src/buffer.h`: declares the internal `SealBuffer` step.
- `src/hook.c`: seals and flushes the completed trampoline before publishing it;
  a protection failure aborts creation and frees the buffer.

Original target-code patches still temporarily require writable executable
protection while MinHook suspends other threads. This change does not remove
those patches, thread suspension, or any mod hooks. It is not a claim that
antivirus detections have been resolved.
