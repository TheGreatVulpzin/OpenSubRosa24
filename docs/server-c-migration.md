# Independent C server storage

## Reference

The starting branch is `main`, at `eb68519`. The working tree is clean.
The MSVC x86 Release build passes before this change.
The type, network and verifier executables pass. The 12 Python tests pass.
Full gate and timing logs are in `build/c-migration/`.
That directory stays outside version control.

The file `src/lifted/lifted_024d.c` has no function bodies.
The remaining server dependencies are address dispatch in startup and accounts,
data access in the shared runtime, and the image allocation in startup.
The x87 thunks contain handwritten assembly and need a normal runtime home.

## Steps and completion criteria

1. Replace address dispatch with declared C calls. Check startup, accounts and the tick loop.
   Keep the hook build able to call the game when differential tests need that path.
2. Define separate typed storage for each game pool, record, table and counter.
   Generate initial values from the checked data metadata. Keep capacities and pointer aliases.
   Put the memory adapter for hooks, recordings and the harness in `instrumentation/`.
3. Remove image allocation, address tables and lifted sources from the normal target.
   Keep the recording target and differential tools operational.
4. Run the build, full gate, focused tests, synthetic cases and timing checks.
   Add checks for native storage, initialization and the normal target dependency closure.
   Record results without changing gate references.

Completion requires both direct calls and independent data storage.
A renamed address macro or a hidden image block does not meet this requirement.
The normal server must build without an instrumentation include path or lifted source.
No floating point expression or rounding point needs to change.

## Rollback

Each structural step has its own commit. Revert the commits in reverse order.
The recording target retains the image adapter for differential verification.
No saved game or account format changes.
