# Game systems

This folder describes how each rewritten game system works. Each page lists the records, the flow of a tick or an event, the functions with their RVAs and the odd behavior that the code comments name.

| Page | Files | System |
| --- | --- | --- |
| [math.md](math.md) | `src/common/vectors.c`, `random.c`, `intersect.c` | Vectors, orientations, random numbers and intersection tests. |
| [network.md](network.md) | `src/net/packet.c`, `events.c`, `server.c`, `netpack.c`, `commands.c`, `master.c`, `client.c` | The bit stream, the events, the server receive and send, priorities, voice, chat commands and the client decoder. |
| [accounts.md](accounts.md) | `src/server/accounts.c` | The accounts in `user/server.srk`. |
| [items.md](items.md) | `src/game/items.c`, `item_types.c`, `item_logic.c`, `item_physics.c`, `human_items.c`, `bullets.c`, `bullet_sim.c` | Items, their places and motion, guns, phones, cash bills and bullets. |
| [physics.md](physics.md) | `src/game/particles.c`, `solver.c`, `bonds.c` | Particles, bonds and the bond solver. |
| [humans.md](humans.md) | `src/game/humans.c`, `human_body.c`, `human_pose.c`, `human_arms.c` | The people: bones, walking, vehicles, arms and death. |
| [collision.md](collision.md) | `src/game/collide.c`, `collide_area.c`, `area.c`, `rigid.c` | Collision tests against the level, objects, humans and items. |

Related pages:

- [protocol.md](../protocol.md): the wire format of the network.
- [formatos.md](../formatos.md): the file formats.
- [plano.md](../plano.md): the groups of the rewrite.

The RVAs are relative to the module base of the 0.24d dedicated server. A function marked `static` in the code has no RVA in these tables.
