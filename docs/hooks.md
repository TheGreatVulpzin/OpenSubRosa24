# Hook DLL (Phase 2)

**Phase 2 is complete.** The method works end to end on the real server: DLL loaded, ASLR handled, hook installed, original called through the trampoline, identical result.

SubVulpes replaces functions of `subrosadedicated024d.exe` with C versions, one at a time.
The mechanism is a proxy `wsock32.dll` in the server folder.

## How it works

1. The 024d imports `WSOCK32.dll`. Windows loads the copy in the folder of the executable first.
2. Our `wsock32.dll` loads the system `wsock32.dll` and forwards the 47 network functions.
3. It checks that the executable is the 024d (build date and PE size). If not, it only forwards the network.
4. It installs the hooks: a 5-byte `JMP` at the start of each original function, to the C version.
5. A trampoline keeps the start of the original function. This lets us call the original.

Code: [`hook/`](../hook/) (proxy, hooks, table) and [`src/server/`](../src/server/) (rewritten functions).

## Replaced functions

| Function | RVA in 024d | File | State |
| --- | --- | --- | --- |
| `createevent_message` | `0x6260` | `src/server/events.c` | **Verified** (2026-10-11): 42 calls with a client connected (welcome, Top 10, corporations, join, chat, admin), 0 differences. |
| `createevent_updatedoor` | `0x6660` | `src/server/events.c` | Batch 1. Not triggered in the 2026-10-11 test. Moved to the next round. |
| `createevent_updatemission` | `0x6760` | `src/server/events.c` | **Verified** (2026-10-11): 1 call, 0 differences. |
| `fixplayername` | `0xa580` | `src/server/players.c` | **Verified** (2026-10-11): 1 call, 0 differences. |

Progress: `python -I tools/progress.py`.

## Before you rewrite a function

1. **Check the name in the code.** Names from the matcher that came from the call graph are often wrong. In batch 1, 4 of 7 `createevent_*` names were wrong. Compare the 024d pseudo-C with the 0.24c one (`build/decomp/`).
2. **Look at the prologue:** `tools/ghidra/PrintPrologue.java` shows the first instructions. The hook prologue must have whole instructions, at least 5 bytes and no relative jump.
3. **Avoid float math for now.** The 024d uses x87. Our compiler uses SSE, and the last bit can change.

## Verification mode

With `verify=1` (default in this phase), each call runs both versions ([`hook/verify.c`](../hook/verify.c)):

1. The game `.data` section (all globals) becomes read-only.
2. The C version runs. Each write raises an exception: the verifier keeps the 4 KB page and allows the write.
3. The verifier keeps the result and undoes the writes. It also restores the `rand()` state of `MSVCR100.dll`.
4. The original runs through the trampoline, the same way.
5. The verifier compares only the pages that one of the versions changed. A difference goes to `subvulpes.log`, with a name (example: `event[1234]+0x40`).

**Learned scope.** To protect the 621 MB of `.data` costs 150 to 200 ms per call. For this reason the verifier learns which globals each function writes:

- In the first 3 verifications of a hook, and every `verify_full_every` (default 50), it protects the whole `.data` and records the written globals.
- In the others, it protects only those globals (the whole array of each one).
- The log shows the time and the type of each verification: `(1.2 ms, scope)` or `(180.0 ms, full)`.

A write outside the learned scope shows up only in the next full round.

**Crash protection.** The new version runs inside `__try` ([`hook/hook_wrap.h`](../hook/hook_wrap.h)). If it crashes, the DLL logs the exception and the address, undoes its writes and runs the original. The hook is marked as broken and uses only the original from then on.

**Heap.** Page protection covers only `.data`. The verifier also copies the committed pages of the `MSVCR100.dll` heap (the `malloc` blocks of the game) before the C version runs. After the C version, it finds the changed pages by comparison and puts the copy back. After the original, it compares the content of the blocks in use. The allocator data (heap header, free lists) does not count: two runs can leave other free lists with the same effect. Without the heap copy, `area_createblock` wrote the new chunk into the chunk grid (a `malloc` block), the original found the chunk, did not create it, and the game kept a chunk count of 0. When the C version frees heap pages (a large block, a decommit), the copy cannot go back: the log shows `freed heap pages ... heap not undone`, and that call is not compared. The log shows the size of the copy (`heap copy of N KB`) each time it grows by a quarter.

Limits: writes to the stack and to other DLLs are not compared. The server must run on one thread.

Verifier test without the game: `build\hook\test_verify.exe` (14 cases).

The game keeps the result of the **original**. So an error in the C version does not affect the match during the test.

## Build

```bat
tools\build_hook.bat
```

Output: `build\hook\wsock32.dll`.

## Test

1. Make a **copy** of the server folder (for example, the Black Houser folder). Do not test in the folder that you use every day.
2. Copy `build\hook\wsock32.dll` to the copy, next to `subrosadedicated024d.exe`.
3. Start the server as usual.
4. Open `subvulpes.log` in the same folder. It must have these lines:
   - `SubVulpes loaded.`
   - `hook createevent_message installed at +0x06260`
   - `N hook(s) installed`
5. Connect a client (you or a friend). Join, leave and use the chat.
6. Close the server with the window button (X), not with the Task Manager. Otherwise the `SUMMARY` lines do not appear.
7. Check the `SUMMARY <hook>: N calls, N verified, 0 differences` lines in the log.

To connect from another PC, the friend must reach the server UDP port (`port=` in `config.txt`, default 27584). Tailscale or ZeroTier work.

## `subvulpes.ini` (optional, in the server folder)

```ini
[hooks]
enabled=1                ; 0 = only forward the network, no hooks
verify=1                 ; 1 = compare with the original on each call
createevent_message=1    ; 0 = disable only this hook (works for any hook name)
verify_every.createevent_message=1   ; verify 1 of every N calls of this hook
verify_full_every=50     ; every N verifications of a hook, do a full round
log_calls=1              ; 1 = log each verified call
verify_heap=1            ; 0 = do not copy and compare the MSVCR100 heap
```

## Remove

Delete `wsock32.dll` from the server folder. The 024d goes back to the Windows `wsock32.dll`.

## Compatibility with the manager

The hook replaces only the first 5 bytes of `createevent_message`. The manager calls `0x626d`, in the middle of the function, and still works. If another program changed the start of a function before, the hook is not installed and the log shows `ERROR ... original bytes differ`.
