"""Run the pure math functions of the client on made inputs, with the clean code and the original.

No recording calls these functions. Each call gets vectors, matrices and numbers: random values,
edge values (zero, negative zero, tiny, huge, NaN, infinity) and shapes that make a hit (a line that
crosses a triangle or a sphere). The harness compares the result and the memory behind each pointer.

Usage: python -I instrumentation/client/synth_pure.py <recording dir> <work dir> [function,...]
The recording gives the memory image. The functions must be in a list of adopted functions.
"""
import csv
import hashlib
import importlib.util
import inspect
import json
import math
import os
import random
import re
import struct
import sys
import threading
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation' / 'client'))
sys.path.insert(0, str(ROOT / 'instrumentation' / 'lift'))
from memory import safe_jobs  # noqa: E402
import synth_textures as st  # noqa: E402

# Parameter kinds: v vector in, o vector out, m 3 by 3 matrix, q quaternion, f float, F float out,
# i small int, I int of any size, u mask of 32 bits, s text buffer out, k list of 8 words of 32 bits,
# l count of words (1 to 8), D degree of a curve (0 to 4), w four floats out, P four vectors.
# World kinds, for the collision functions: W a point of the world (x and z from 0 to 2048, y from 0 to 80;
# every fifth call uses the street grid), X a second point within 40 units of the last W (the end of a line),
# r a radius, t a team (-1 to 3), n an object, human or item index (live indices, -1 or 4095).
# Area kinds: a an area (0 to 3), A an area that can be out of range (0 to 3, 4, 5, 0xffffffff), c a block
# coordinate (edges of sectors and chunks, a random one, or -1), B a block word, e a face (-1 to 7).
# Packet kinds: U a pointer to one 32-bit word, b a bit count (0 to 32, sometimes outside), S a pointer to
# 64 bytes, J an item size (0 to 4), K an item count (0 to 8). Other kinds: p a player index (0 to 31),
# N a name text, L a file list index (0 to 3), x an amount of shares or money (small, large, negative).
PROTOTYPES = {
    'savemodelcsm': 'pN',
    'loadmodelcsm': 'pN',
    'addhalfboxtomodel': 'vmrrre',
    'createsunglassmodel': 'p',
    'createfootmodel': 'prrr',
    'createrotormodel': 'prrr',
    'createheadmodel': 'p',
    'tesselatebeziertriangle': 'offfH',
    'sethumanmodelvert': 'ppfff',
    'sethumanmodelvertface': 'ppfff',
    'addwaisttomodel': 'vrrr',
    'createcellphonemodel': 'prrr',
    'createkeymodel': 'p',
    'e': 'N',
    'get_marker': '',
    'get16': '',
    'get16le': '',
    'initWGLLoadFunctions': '',
    'fround': 'ff',
    'net_calcdistancefactor': 'f',
    'getenginetorque': 'f',
    'raydistancesphere2': 'vvr',
    'pointintersectcircle': 'oFvvr',
    'checkbonds': '',
    'human_calculateanglesobject': '',
    'human_walksimulation2': '',
    'createscenario_mission': '',
    'createscenario_coop': '',
    'createscenario_driving': '',
    'scenario_createaicar': '',
    'scenario_addcheckpoint': '',
    'resetparser': '',
    'getfloat': '',
    'checkfilespec': 'NN',
    'unpackunitvector': 'kog',
    'unpackposition': 'o',
    'unpackscalar': 'org',
    'setuphumanangles': 'p',
    'setupobjecttypes': '',
    'createmessage': 'hNpp',
    'newspaper_addline': 'Sph',
    'scenario_getnextmission': 'h',
    'scenario_isseekmission': 'h',
    'addwindowatlas': 'ph',
    'building_iswindow': 'hhhhh',
    'building_reloadtextures': '',
    'loadtexture2': 'pNhhhhh',
    'gettextlength': 'Nf',
    'checkbigendian': '',
    'addcitystreet': 'hhhhN',
    'addcityblock': 'hhhh',
    'getplayticket': '',
    'settwosidedstencil': 'uuiuuuuuuu',
    'saveaccountsserver': '',
    'loadaccountsclient': '',
    'area_getblocktexture': 'Accce',
    'addbeziersurfacetexture': 'PPEEEEh',
    'area_interior_createblock': 'AcccBk',
    'area_interior_setblock': 'AcccB',
    'area_interior_getblock': 'Accc',
    'area_interior_getblocklink': 'Accc',
    'area_interior_getblocktexture': 'Accce',
    'sphereintersectnormalblock': 'ooFvrzBr',
    'sphereintersectspecblock': 'ooFvrzBr',
    'circleintersectnormalblock': 'ooFvvrzBr',
    'circleintersectspecblock': 'ooFvvrzBr',
    'bondtimetolive': '',
    'deletebond': 'p',
    'createcityobject': 'hWr',
    'sphereintersectcityobjecttype': 'ooFhWmWr',
    'packunitvector': 'kvg',
    'packorientation': 'vg',
    'unpackorientation': 'og',
    'packangle': 'yg',
    'unpackangle': 'og',
    'packposition': 'v',
    'packpositiondelta': 'vvrg',
    'unpackpositiondelta': 'ovrg',
    'packscalar': 'yrg',
    'playersellstock': 'px',
    'createevent_bullet': 'hvvh',
    'createevent_createobject': 'p',
    'createevent_updatehuman': 'p',
    'createevent_updatestocks': '',
    'createevent_updatecarmarket': 'h',
    'testtimer': '',
    'deletehuman': 'p',
    'sphereintersectpoint': 'ooFvrv',
    'setitempositionhuman': 'pp',
    'item_billvalue': 'p',
    'item_addbill': 'phh',
    'item_removebill': 'ph',
    'linkitemtoitem': 'pp',
    'item_addhumanlink': 'pph',
    'circleintersectlevel': 'vvr',
    'circleintersectlandscape': 'vvr',
    'logicsimulation_driving': '',
    'logic_addteamcash': 'hx',
    'logic_teammissioncomplete': 'hh',
    'objecttimetolive': '',
    'getobjectrotation2': 'opv',
    'getobjectrotation3': 'op',
    'sphereintersectobject': 'pWr',
    'objectwheelcollision': 'ph',
    'particletimetolive': '',
    'particlesimulation': '',
    'scenario_setmissioninactive': 'p',
    'setuptimers': '',
    'lineintersectline2d': 'ooFvvvv',
    'sphereintersectline2d': 'ooFvvvr',
    'lineintersectcircle2d': 'ooFvvvr',
    'getint': '',
    'comparestrings': 'NN',
    'disablesocket': 'h',
    'cs_pread': 'SJK',
    'getnextcityobject': '',
    'getnexthuman': '',
    'getnextitem': '',
    'getnextobject': '',
    'getnextplayer': '',
    'deletecityobject': 'n',
    'deletebullet': 'n',
    'player_addaction_item': 'pxx',
    'player_addaction_chat': 'pC',
    'addplayerwaypoint': 'pfff',
    'createclientaccount': 'UUx',
    'accountdeathtax': 'n',
    'playerdebt': 'p',
    'addpointstoteam': 'hx',
    'logic_splitteamcash': 'hx',
    'negvector': 'ov',
    'crossproduct': 'ovv',
    'transposematrix': 'mm',
    'copyquaternion': 'qq',
    'quaterniontoorientation': 'mq',
    'dotproduct2d': 'vv',
    'vectorlength2d': 'v',
    'normalizevector2d': 'ov',
    'rotatevector2d': 'vf',
    'addvectors2d': 'ovv',
    'subtractvectors2d': 'ovv',
    'scalevector2d': 'ovf',
    'scaleaddvectors2d': 'ovvf',
    'tangentvector2d': 'ov',
    'genrand_int31': '',
    'genrand_real1': '',
    'genrand_real2': '',
    'genrand_real3': '',
    'genrand_res53': '',
    'getrandomint': '',
    'getrandomfloat': '',
    'createbullet': 'ivvn',
    'createparticle': 'ivvfn',
    'createplayer': '',
    'deleteplayer': 'p',
    'item_removehumanlink': 'nni',
    'item_removeitemlink': 'nn',
    'linkitem': 'nnni',
    'human_calcarmtorque': 'n',
    'calculateplayervoice': 'ip',
    'gettimeofday': 'ss',
    'applydamageobject': 'nI',
    'scenario_armhuman': 'pii',
    'logic_gunstore_buy': 'pi',
    'createaccount': 'p',
    'economy_carmarket': '',
    'itemweaponsimulation': 'n',
    'playerai': 'p',
    'createitem': 'ivvm',
    'economysimulation': '',
    'bulletsimulation': '',
    'objectenginesimulation2': '',
    'systemtimer': '',
    'logic_player': 'p',
    'objectsimulation_car': 'n',
    'objectsimulation_heli': 'n',
    'ai_playercar': 'pn',
    'ai_car_nextwaypoint': 'p',
    'city_createelevator': 'v',
    'createlandscape': '',
    'createtrack': 'ivff',
    'buildsomeshit': 'v',
    'buildcitystreet': 'i',
    'buildcityinterection': 'i',
    'area_createblock': 'AcccBk',
    'loadblock': 'N',
    'human_calculatearmangles': 'n',
    'human_calculatelegangles': 'n',
    'human_collisionhuman': 'n',
    'human_buildmuscles': 'n',
    'eventsimulation': '',
    'itemsimulation': '',
    'itemlogic': '',
    'createarea': 'AvEEEr',
    'playersimulation': '',
    'economy_gunmarket': '',
    'setupbullettypes': '',
    'createhuman': 'vmp',
    'item_physicssimulation': 'n',
    'createbond': 'iinn',
    'loadsaveoptions': 'i',
    'objecttrainsimulation': 'n',
    'player_saveinventory': 'p',
    'city_addintersection': 'ccc',
    'cityobject_addbox': 'iffffff',
    'area_block_addbox': 'ffffffu',
    'pointinsidebuilding': 'nvf',
    'human_createmuscleball': 'nnnv',
    'createjointbond': 'nnnv',
    'player_calcassets': 'p',
    'logic_suits_buy': 'pi',
    'saveaccountsclient': '',
    'lineintersecthuman': 'nWXr',
    'addobjectrotation': 'nvvf',
    'init_genrand': 'u',
    'getrandomvector3d': 'off',
    'resetorientation': 'm',
    'createevent_sound': 'ivff',
    'objectwheelsimulation2': '',
    'object_rotorsimulation': 'n',
    'area_createsblocks': '',
    'building_setupfloorplannewer': '',
    'building_setupbuild': '',
    'player_loadinventory': 'p',
    'resetgame': '',
    'newspaper_load': '',
    'human_itemsimulation': 'n',
    'logicsimulation': '',
    'logic_farbslist': 'i',
    'logic_createcar': 'iti',
    'logic_setteammanager': 'tp',
    'logic_cardealer_buy': 'pi',
    'logic_playeractions': 'p',
    'logicsimulation_round': '',
    'logicsimulation_world': '',
    'objectwheelcollision2': 'nn',
    'createscenario_world': '',
    'scenario_createguards': 'nii',
    'scenario_createtraffic2': 'i',
    'scenario_createchase': 'ii',
    'scenario_createcar': 'iiiiif',
    'serverrecv': '',
    'humansimulation': '',
    'objectsimulation': '',
    'serversend': '',
    'createlevel': '',
    'createobject': 'ivvmi',
    'createvehicle': 'ivm',
    'createmission_acquisition': 'iEQ',
    'createmission_transaction': 'iEQ',
    'createmission_threeway': 'iEQ',
    'server_playermessage': 'pN',
    'pbitwrite': 'Ub',
    'pbitwritenego': 'Ub',
    'pbitread': 'Ub',
    'pbitreadnego': 'Ub',
    'cs_pwrite': 'SJK',
    'playerdeathtax': 'p',
    'playerteamkilltax': 'pd',
    'playerbuystock': 'px',
    'scenario_getnumofmissions': 't',
    'bullettimetolive': '',
    'itemtimetolive': '',
    'removeflieextentions': 'L',
    'getenginetorque2': 'n',
    'account_nameexists': 'N',
    'area_getblock': 'Accc',
    'area_marksector': 'Accc',
    'area_setblock': 'AcccB',
    'area_setblockcolor': 'AcccB',
    'area_deleteblock': 'Accc',
    'area_interior_setobject': 'AcccB',
    'area_getnextchunk': 'a',
    'area_getnextsector': '',
    'area_createchunk': 'Accc',
    'area_createsector': 'Acccc',
    'lineintersectobject': 'nWXi',
    'lineintersecttrack': 'WXi',
    'lineintersecttrackrail': 'WXi',
    'item_sphereintersect': 'nWr',
    'lineintersectcityobject': 'ooFnWX',
    'lineintersectcityobjecttype': 'ooFnWmXX',
    'lineintersectarea': 'ooFAWX',
    'lineintersectnormalblock': 'ooFWXzBf',
    'lineintersectspecblock': 'ooFWXzBf',
    'setupbody': '',
    'setupbuildingtypes': '',
    'setupcityobjecttypes': '',
    'setupitemtypes': '',
    'setupobjecttypes_cars': '',
    'setupobjecttypes_helicopters': '',
    'setupobjectypes_train': '',
    'area_block_rotatesurface': 'Te',
    'area_copyrotateblock': 'cz',
    'area_block_addbasicsurface': 'cv',
    'area_block_addbeam': 'Tffffffck',
    'area_block_addwindow': 'Tffffcc',
    'area_block_adddoor': 'Tff',
    'area_block_addoutdoor': 'Tff',
    'area_block_addcorner': 'Tff',
    'area_block_addwindowvertical': 'Tii',
    'area_block_addwindowhorizontal': 'Tiiii',
    'area_createinteriorblocks': '',
    'createevent_bullethit': 'iivv',
    'createevent_updateobject': 'niIvv',
    'createevent_updateitem': 'n',
    'createevent_updateiteminfo': 'n',
    'createevent_updateplayer': 'n',
    'createevent_updateplayer_finance': 'n',
    'createevent_updateblock_quad': 'Accccevv',
    'sphereintersectlandscape': 'Wr',
    'lineintersectlandscape': 'WX',
    'sphereintersectlevel': 'Wr',
    'lineintersectlevel': 'WXi',
    'lineintersectall': 'WX',
    'pointinsidebase': 'Wtf',
    'pointinsidebasesafe': 'Wtf',
    'angulardistance': 'ff',
    'calculatebarycentric': 'ovvvv',
    'circleintersecttriangle': 'ooFvvfvvv',
    'geteulerangles2': 'om',
    'lineintersectplane': 'ovFvvv',
    'lineintersectplanetwosided': 'ovFvvv',
    'lineintersectsphere': 'ooFvvvf',
    'lineintersecttriangle': 'ovFvvvvv',
    'lineintersecttriangletwosided': 'ovFvvvvvi',
    'multiply3x3matrices': 'mmm',
    'multiplyquaternion': 'qqq',
    'orientationtoquaternion': 'qm',
    'pointdistanceline': 'vvv',
    'projectorientation': 'mmm',
    'projectvector': 'ovm',
    'rotateorientation': 'mvf',
    'rotatevector': 'vvf',
    'sphereintersectline': 'ooFvfvv',
    'sphereintersecttriangle': 'ovFvfvvv',
    'sphereintersecttrianglefull': 'ooFvfvvv',
    'genrand_int32': '',
    'init_by_array': 'kl',
    'getrandomvector2d': 'off',
    'sprintf_comma': 'sI',
    'tesselatebezier': 'Dfffff',
    'tesselatebezierderivative': 'Dfffff',
    'objecttype_calcquadweights': 'wvP',
    'calculatefriction': 'vvf',
}


CACHE_FILE = ROOT / 'build' / 'step_client' / 'synth_cache.json'
# SYNTH_SERVER=1 runs the calls on a copy of the snapshot of a game that hosts a server: server.active is 1
# and the game type is 3. Without it the client snapshot has no server, and the server functions return at once.
SERVER_EDITS = [(0x75a5800, 'I', (1,)), (0x9065308, 'I', (3,))]
TEST_SOURCES = ('instrumentation/tests/harness.c', 'instrumentation/client/synth_textures.py',
                'instrumentation/client/synth_pure.py', 'analysis/client_win/params.csv')
RESULT = re.compile(r'(\d+) calls same, (\d+) different, (\d+) known differences, (\d+) lifted crashed')


def closure_hashes(names):
    """Hash of the clean code of each function (tools/re/clean_closure.py)."""
    spec = importlib.util.spec_from_file_location('clean_closure', ROOT / 'tools' / 're' / 'clean_closure.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.hashes(names)


def case_hash(name, kinds, count):
    """What a result depends on besides the clean code: the kinds of arguments, their generator,
    the number of calls, the finite switch and the sources of the test tools."""
    h = hashlib.sha256()
    h.update(f'{name}|{kinds}|{count}|{os.environ.get("SYNTH_FINITE", "")}|{os.environ.get("SYNTH_SERVER", "")}'.encode())
    h.update(inspect.getsource(make_args).encode())
    h.update(inspect.getsource(number).encode())
    for item in TEST_SOURCES:
        h.update((ROOT / item).read_bytes().replace(b'\r\n', b'\n'))
    return h.hexdigest()


def load_cache():
    try:
        return json.loads(CACHE_FILE.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return {}


def rvas():
    out = {}
    for row in csv.DictReader((ROOT / 'analysis/client_win/functions.csv').open(encoding='utf-8')):
        if row['name']:
            out[row['name']] = int(row['rva'], 16)
    return out


def parameter_kinds():
    """Read the argument descriptors of the compiled dispatch table."""
    table = (ROOT / 'src/lifted/client/lifted_table.c').read_text(encoding='utf-8')
    return dict(re.findall(r'\{ "(\w+)", 0x[0-9a-f]+, .*?, \d+, LF_RET_\w+, \d+, "([pv]*)",', table))


def number(rng, kind):
    """One float: mostly small random values, sometimes an edge value."""
    pick = rng.random()
    if pick < 0.12:
        # SYNTH_FINITE=1 keeps NaN, infinity and huge values out of the arguments.
        if os.environ.get('SYNTH_FINITE'):
            return rng.uniform(-6.0, 6.0)
        return rng.choice(st.EDGES)
    if pick < 0.2:
        return rng.choice([0.0, 1.0, -1.0, 0.5])
    return rng.uniform(-6.0, 6.0)


def unit(rng):
    v = [rng.uniform(-1.0, 1.0) for _ in range(3)]
    n = math.sqrt(sum(x * x for x in v)) or 1.0
    return [x / n for x in v]


def orientation(rng):
    """Three orthogonal unit rows, or random junk now and then."""
    if rng.random() < 0.2:
        return [number(rng, 'f') for _ in range(9)]
    a = unit(rng)
    b = unit(rng)
    c = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
    n = math.sqrt(sum(x * x for x in c)) or 1.0
    c = [x / n for x in c]
    d = [c[1] * a[2] - c[2] * a[1], c[2] * a[0] - c[0] * a[2], c[0] * a[1] - c[1] * a[0]]
    return a + c + d


def make_args(rng, kinds, index):
    """Build the arguments of one call. Every few calls use shapes that cross, so the hit paths run."""
    args = []
    hit = index % 3 == 0
    base = [rng.uniform(-3.0, 3.0) for _ in range(3)]
    last_world = (1000.0, 10.0, 1000.0)
    for kind in kinds:
        if kind == 'H':
            args.append('P:0:' + struct.pack('<10i', *(rng.randrange(38) for _ in range(10))).hex())
        elif kind == 'h':
            args.append(rng.randrange(3))
        elif kind == 'g':
            args.append(rng.choice([1, 2, 3, 4, 6, 8, 9, 12, 15, 16, 20, 24, 31]))
        elif kind == 'y':
            args.append((number(rng, 'f'),))
        elif kind == 'C':
            length = rng.choice([0, 1, 2, 62, 63, 64, 65, 127])
            args.append('P:0:' + (b'x' * length + b'\0').hex())
        elif kind == 'U':
            args.append('P:0:' + struct.pack('<I', rng.choice([0, 1, 0xff, 0xffff, 0xffffffff, rng.randrange(0, 2**32)])).hex())
        elif kind == 'b':
            args.append(rng.choice([0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 24, 31, 32, 32, rng.randrange(1, 33), rng.randrange(1, 33), 33, -1]))
        elif kind == 'S':
            args.append('P:0:' + bytes(rng.randrange(0, 256) for _ in range(64)).hex())
        elif kind == 'J':
            args.append(rng.choice([0, 1, 2, 4, 4, 3]))
        elif kind == 'K':
            args.append(rng.choice([0, 1, 2, 3, 5, 8, 8]))
        elif kind == 'p':
            args.append(rng.choice([0, 1, 2, 3, 5, 9, 15, 31, rng.randrange(0, 32)]))
        elif kind == 'N':
            name = rng.choice([b'', b'a', b'Vulpzin', b'Player 1', b'x' * 24, b'x' * 40, bytes(rng.randrange(97, 123) for _ in range(rng.randrange(1, 30)))])
            args.append('P:0:' + (name + b'\0').hex())
        elif kind == 'Q':
            args.append('P:0:' + b''.join(struct.pack('<i', rng.choice([0, 1, 2, 0, 1, 2, -1])) for _ in range(8)).hex())
        elif kind == 'L':
            args.append(rng.choice([0, 1, 2, 3]))
        elif kind == 'd':
            args.append(rng.choice([0, 1, 5, 10, 50, 100, 500, 1000, 5000, 20000]))
        elif kind == 'x':
            args.append(rng.choice([0, 1, -1, 10, 100, 99999, 100000, 1000000, 2147483647, -2147483648, rng.randrange(-1000, 100000)]))
        elif kind == 'T':
            args.append(rng.choice([0, 1, 2, 3, 0, 1, 2, 3, 4, -1]))
        elif kind == 'z':
            args.append('P:0:' + struct.pack('<3i', *[rng.choice([1, 2, 4, 8, 3, 0]) for _ in range(3)]).hex())
        elif kind == 'a':
            args.append(rng.choice([0, 1, 2, 3]))
        elif kind == 'A':
            args.append(rng.choice([0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0xffffffff]))
        elif kind == 'c':
            args.append(rng.choice([0, 1, 7, 8, 63, 64, 65, 127, 128, 255, 256, 511, -1, rng.randrange(0, 600),
                                    rng.randrange(0, 600), rng.randrange(0, 70)]))
        elif kind == 'B':
            args.append(rng.choice([0, 1, 2, 0xff, 0xffff, 0xffffffff, rng.randrange(0, 2**32), rng.randrange(0, 2**16)]))
        elif kind == 'e':
            args.append(rng.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7]))
        elif kind == 'W':
            x, z = rng.uniform(0, 2048), rng.uniform(0, 2048)
            if index % 5 == 0:
                x, z = round(x / 8) * 8.0, round(z / 8) * 8.0
            last_world = (x, rng.uniform(0, 80), z)
            args.append(last_world)
        elif kind == 'X':
            args.append(tuple(last_world[k] + rng.uniform(-40, 40) for k in range(3)))
        elif kind == 'r':
            args.append(rng.choice([0.0, 0.05, 0.25, 0.5, 1.0, 2.0, 4.0]))
        elif kind == 't':
            args.append(rng.choice([-1, 0, 1, 2, 3]))
        elif kind == 'n':
            args.append(rng.choice([-1, 0, 1, 2, 3, 5, 9, 4095]))
        elif kind == 'o':
            args.append(tuple([123.5] * 3))
        elif kind == 'F':
            args.append(tuple([77.25]))
        elif kind == 'v':
            if hit and rng.random() < 0.7:
                args.append(tuple(base[k] + rng.uniform(-2.5, 2.5) for k in range(3)))
            else:
                args.append(tuple(number(rng, 'f') for _ in range(3)))
        elif kind == 'm':
            args.append(tuple(orientation(rng)))
        elif kind == 'q':
            args.append(tuple(number(rng, 'f') for _ in range(4)))
        elif kind == 'f':
            args.append(abs(number(rng, 'f')) if rng.random() < 0.5 else number(rng, 'f'))
        elif kind == 'i':
            args.append(rng.choice([0, 1, 2, -1]))
        elif kind == 'I':
            args.append(rng.choice([0, 1, -1, 999, 1000, 123456, 1000000, 2147483647, -2147483648, rng.randrange(-10**9, 10**9)]))
        elif kind == 'u':
            args.append(rng.choice([0, 0xff, 0xffff, 0x3ff, 0xffffffff, rng.randrange(0, 2**32)]))
        elif kind == 's':
            args.append('P:0:' + (b'U' * 64).hex())
        elif kind == 'k':
            args.append('P:0:' + b''.join(struct.pack('<I', rng.randrange(0, 2**32)) for _ in range(8)).hex())
        elif kind == 'D':
            args.append(rng.choice([0, 1, 2, 3, 4]))
        elif kind == 'E':
            args.append(rng.choice([1, 2, 3]))
        elif kind == 'w':
            args.append(tuple([55.5] * 4))
        elif kind == 'P':
            args.append(tuple(number(rng, 'f') for _ in range(12)))
        elif kind == 'l':
            args.append(rng.randrange(1, 9))
    return args


def main():
    rec, work = Path(sys.argv[1]), Path(sys.argv[2])
    work.mkdir(parents=True, exist_ok=True)
    want = sys.argv[3].split(',') if len(sys.argv) > 3 else list(PROTOTYPES)
    addresses = rvas()
    descriptors = parameter_kinds()
    env = dict(os.environ, HARNESS_RESET='1', HARNESS_GAME_DIR=str(ROOT / 'build' / 'client_test'),
               HARNESS_ENV_DIR=str(work / 'env'), SV_UI_WIDTH='220')
    env['PATH'] = str(ROOT / 'build' / 'client_deps') + os.pathsep + env.get('PATH', '')
    # SYNTH_CALLS: the number of calls for each function (90 by default, 30 to look around fast).
    # SYNTH_JOBS: the number of functions that run at the same time (1 by default, 2 is safe).
    count = int(os.environ.get('SYNTH_CALLS', '90'))
    jobs = safe_jobs(max(1, int(os.environ.get('SYNTH_JOBS', '1'))))

    # The result of a function that passed is kept with the hash of its clean code. The same hash
    # means the same result, so the case is skipped. SYNTH_FORCE=1 runs every case again.
    cache = load_cache()
    lock = threading.Lock()
    passed = {}
    force = bool(os.environ.get('SYNTH_FORCE'))
    names = [item.partition('=')[0] for item in want]
    code = closure_hashes(names)

    def run_one(item):
        # "name=kinds" replaces the prototype of that function for this run.
        name, _, kinds = item.partition('=')
        proto = kinds if kinds else PROTOTYPES[name]
        key = hashlib.sha256((code[name] + case_hash(name, proto, count)).encode()).hexdigest()
        if not force and cache.get(name, {}).get('key') == key:
            passed[name] = True
            return f'{name}: {count} calls: cached, {cache[name]["result"]}'
        rng = random.Random(sum(map(ord, name)))
        calls = []
        snap = None
        scratch = 0x19e00000
        descriptor = descriptors[name]
        for index in range(count):
            args = make_args(rng, proto, index)
            if name in ('get_marker', 'get16', 'get16le'):
                args = [st.image(0x19e20000)]
            for position, value in enumerate(args):
                if position >= len(descriptor) or descriptor[position] == 'p':
                    continue
                if isinstance(value, tuple):
                    raw = struct.pack(f'<{len(value)}f', *value).ljust(256, b'\0')
                elif isinstance(value, str) and value.startswith('P:'):
                    raw = bytes.fromhex(value.split(':', 2)[2])
                else:
                    continue
                if snap is None:
                    snap = st.Snapshot(rec / 'sv_snapshot.bin')
                snap.write(scratch, f'{len(raw)}s', raw)
                args[position] = st.image(scratch)
                scratch += (len(raw) + 31) & ~15
            calls.append(st.call(addresses[name], *args))
        path = work / f'{name}.txt'
        path.write_text(''.join(calls), encoding='utf-8', newline='\n')
        own = dict(env, HARNESS_ENV_DIR=str(work / f'env_{name}'), SV_LOG_FILE=str(work / f'{name}.log'))
        snapshot = rec / 'sv_snapshot.bin'
        if name in ('getint', 'getfloat'):
            if snap is None:
                snap = st.Snapshot(snapshot)
            text = b'prefix -2147483648 end\0' if name == 'getint' else b'prefix -123.125 end\0'
            snap.write(0x13d5e700, 'ii', 0, len(text) - 1)
            snap.write(0x13d5e708, f'{len(text)}s', text)
        if name in ('get_marker', 'get16', 'get16le'):
            if snap is None:
                snap = st.Snapshot(snapshot)
            data = 0x19e21000
            snap.write(0x19e20000, '7I', 0, 0, 0, 0, 0, st.BASE + data, st.BASE + data + 4)
            if name == 'get_marker':
                snap.write(0x19e20000 + 0x369c, 'B', 255)
                snap.write(data, '4B', 255, 255, 0xd8, 0)
            else:
                snap.write(data, '4B', 0x12, 0x34, 0x56, 0x78)
        if name.startswith(('pack', 'unpack')) or name == 'cs_pread':
            if snap is None:
                snap = st.Snapshot(snapshot)
            socket = 0x19d851a0
            snap.write(socket + 0x5c, 'ii', 0, 0)
            snap.write(socket + 0x64, '256s', bytes(256))
            snap.write(socket + 0x10064, 'iii', 256, 0, 0)
            snap.write(socket + 0x10070, '256s', bytes((index * 37 + 23) & 255 for index in range(256)))
        constructors = ('createsunglassmodel', 'createfootmodel', 'createrotormodel',
                        'createheadmodel', 'createcellphonemodel', 'createkeymodel')
        if name in constructors or name in ('savemodelcsm', 'loadmodelcsm'):
            if snap is None:
                snap = st.Snapshot(snapshot)
            for model in range(32):
                record = 0xaff05a0 + model * 0xe04
                for offset in (8, 12, 16, 20, 24, 32, 36, 40, 48):
                    snap.write(record + offset, 'I', 0)
            if name in ('savemodelcsm', 'loadmodelcsm'):
                game = work / f'game_{name}'
                (game / 'model').mkdir(parents=True, exist_ok=True)
                own['HARNESS_GAME_DIR'] = str(game)
            if name == 'savemodelcsm':
                base = 0x19e80000
                arrays = [(8, '9f', [0,0,0, 1,0,0, 0,1,0]),
                          (16, '9f', [0,0,1] * 3), (20, '9f', [1,0,0] * 3),
                          (32, '3i', [0,1,2]), (36, '3f', [0,0,1]), (40, '3i', [-1,-1,-1])]
                for index, (offset, fmt, values) in enumerate(arrays):
                    snap.write(base + index * 256, fmt, *values)
                for model in range(32):
                    record = 0xaff05a0 + model * 0xe04
                    snap.write(record, 'ii', 3, 8)
                    snap.write(record + 0x1c, 'i', 1)
                    for index, (offset, _, _) in enumerate(arrays):
                        snap.write(record + offset, 'I', st.BASE + base + index * 256)
        if name in ('addhalfboxtomodel', 'addwaisttomodel'):
            if snap is None:
                snap = st.Snapshot(snapshot)
            for offset in (0, 0xa0008, 0x20000c, 0x260010):
                snap.write(0x90f7680 + offset, 'i', 0)
        if name == 'tesselatebeziertriangle':
            if snap is None:
                snap = st.Snapshot(snapshot)
            control = random.Random(420)
            for index in range(38):
                snap.write(0x1bf7f60 + index * 12, '3f', *(control.uniform(-2, 2) for _ in range(3)))
        if name in ('sethumanmodelvert', 'sethumanmodelvertface'):
            if snap is None:
                snap = st.Snapshot(snapshot)
            for vertex in range(32):
                snap.write(0x1bbfe04 + vertex * 28 + 4, 'i8s', vertex + 7, b'abcdefgh')
        if os.environ.get('SYNTH_SERVER'):
            if snap is None:
                snap = st.Snapshot(snapshot)
            for rva, fmt, values in SERVER_EDITS:
                snap.write(rva, fmt, *values)
        if snap is not None:
            snapshot = work / f'snapshot_{name}.bin'
            snap.save(snapshot)
        result = st.run(snapshot, path, name, own)
        if snapshot != rec / 'sv_snapshot.bin':
            snapshot.unlink()
        m = RESULT.search(result)
        passed[name] = bool(m and int(m.group(1)) + int(m.group(3)) > 0
                            and int(m.group(2)) == 0 and int(m.group(4)) == 0)
        if passed[name]:
            with lock:
                cache[name] = {'key': key, 'result': result}
                CACHE_FILE.parent.mkdir(parents=True, exist_ok=True)
                CACHE_FILE.write_text(json.dumps(cache, indent=1, sort_keys=True), encoding='utf-8')
        return f'{name}: {len(calls)} calls: ' + result

    if jobs == 1:
        for item in want:
            print(run_one(item), flush=True)
    else:
        from concurrent.futures import ThreadPoolExecutor
        with ThreadPoolExecutor(max_workers=jobs) as pool:
            for line in pool.map(run_one, want):
                print(line, flush=True)
    CACHE_FILE.parent.mkdir(parents=True, exist_ok=True)
    CACHE_FILE.write_text(json.dumps(cache, indent=1, sort_keys=True), encoding='utf-8')
    if not all(passed.get(name, False) for name in names):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
