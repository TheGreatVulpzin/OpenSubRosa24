# Bonds and the solver

`bondsimulation2` in `src/game/bonds.c` runs 32 passes over particles, bones, items and wheel bodies. Particle springs and bone joints use `src/game/solver.c`. The same functions serve both programs.

The client stores each human in a `BondHumanClient` workspace of 0x500 bytes. Each workspace has five arrays of sixteen vector slots: velocity, velocity change, angular momentum, angular velocity and angular change. Each slot holds three floats and four padding bytes. The solver uses fifteen slots. It does not write slot sixteen or the padding.

The access macros in `bond_storage.h` select the workspace fields. The client adapter defines these macros through `tools/re/gen_client_storage.py`. It reads rotation axes and inverse inertia directly from `Bone`. Human activity comes from `Human.active` and `Human.has_physics`.

Each pass clears the changes of all fifteen bones of an active human with physics. A bone takes its changes only when `Bone.in_solver` is positive. The pass reads the full `Bond.mode` value. It does not copy that value into `BondOpt.mode`.

Type 16 connects bones of two humans with a spring. The direction comes from the bone positions. The spring combines its length error with relative velocity along that direction. `BondOpt.damping` supplies the damping. The bone masses divide the impulse between the two velocity changes.

The balance clamp preserves NaN after the vertical rotation term. Other balance operations follow the shared solver.
