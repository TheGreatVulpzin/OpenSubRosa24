"""Write a call file for client functions that no recording reaches.

The harness replays "R <rva> args" lines on the snapshot of a recording. A function with plain
integer arguments needs no recorded call: this script writes the calls. The state is the state
of the snapshot, so a case reaches the paths that this state allows. Cases that change the state
come later (see docs/client.md).

Usage: python -I instrumentation/client/synth_calls.py <out dir> [case,...]
Then:  harness_client.exe build/hook/subrosa_fixed.exe <recording>/sv_snapshot.bin <out dir>/sv_args.txt <name>
The environment of the harness is the same as for the gate (docs/client.md).
"""
import sys
from pathlib import Path

HUMANSIMULATION = 0x85b70
HUMAN_ITEMSIMULATION = 0x8f730
CALCULATEPLAYERVOICE = 0xfbe70
SCENARIO_CREATECHASE = 0xdd100
CREATEHEADMODEL2 = 0xb80b0
TEMPMESH_OPTIMIZE = 0x4c320
TEMPMESH_OPTIMIZE2 = 0x4cd90
MODEL_EXTRUDEEYE2 = 0xb7aa0
MODEL_EXTRUDEQUADFACE = 0xb1380
CREATEMODEL_CITYOBJECT_DESK = 0xb06c0
GETINPUTLETTER = 0x1046c0
SETUPJOYSTICKS = 0xfeea0
CHECKJOYSTICK = 0xff020
UNPACKOBJECTPACKET = 0xc1dc0


def call(rva, *args):
    return f'R {rva:x}' + ''.join(f' V:{a:x}' for a in args) + '\nZ r:0\n'


def cases():
    return {
        'humansimulation': [call(HUMANSIMULATION)] * 3,
        # The index of a human. Slots past the live humans are empty: the function reads zeros.
        'human_itemsimulation': [call(HUMAN_ITEMSIMULATION, i) for i in range(8)],
        # (listener, player)
        'calculateplayervoice': [call(CALCULATEPLAYERVOICE, a, b) for a in range(2) for b in range(4)],
        # (unused, id stored in the new player)
        'scenario_createchase': [call(SCENARIO_CREATECHASE, 0, x) for x in (0, 5, 9)],
        # The model slot. No code of the client calls this function.
        'createheadmodel2': [call(CREATEHEADMODEL2, m) for m in (0, 3)],
        # The temporary mesh of the snapshot: the last sector or interior mesh.
        'tempmesh_optimize': [call(TEMPMESH_OPTIMIZE)],
        'tempmesh_optimize2': [call(TEMPMESH_OPTIMIZE2)],
        # (quad, bone) and (quad) on the model in work of the snapshot.
        'model_extrudeeye2': [call(MODEL_EXTRUDEEYE2, q, b) for q in (0, 1) for b in (0, 1)],
        'model_extrudequadface': [call(MODEL_EXTRUDEQUADFACE, q) for q in (0, 2)],
        'createmodel_cityobject_desk': [call(CREATEMODEL_CITYOBJECT_DESK, m) for m in (0, 3)],
        # The key presses of the frame of the snapshot. The argument is not used.
        'getinputletter': [call(GETINPUTLETTER, 0)] * 4,
        # SDL of the harness has no joystick: the count is 0.
        'setupjoysticks': [call(SETUPJOYSTICKS)],
        'checkjoystick': [call(CHECKJOYSTICK)] * 2,
        # The packet reader of the snapshot: the calls read the bits that come next.
        'unpackobjectpacket': [call(UNPACKOBJECTPACKET)] * 2,
    }


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    allc = cases()
    wanted = sys.argv[2].split(',') if len(sys.argv) > 2 else list(allc)
    text = ''.join(''.join(allc[w]) for w in wanted)
    (out / 'sv_args.txt').write_text(text, encoding='utf-8', newline='\n')
    print(f'{sum(len(allc[w]) for w in wanted)} calls -> {out / "sv_args.txt"}')


if __name__ == '__main__':
    main()
