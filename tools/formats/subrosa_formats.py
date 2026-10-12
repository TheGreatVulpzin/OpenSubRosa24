"""Readers for the Sub Rosa 0.24 file formats.

Formats (all little-endian, 4-byte fields unless noted):
  .srk  server accounts (user/server.srk) and client accounts (client.srk)
  .sbl  block: building piece (wall, door, stairs) in data/block/
  .sbb  building: 3D grid of cells in data/building/
  .sbc  city: streets, intersections, tile map and buildings (data/city.sbc)
  .csm  3D model (data/weapon/*.csm)

Source: decompilation of subrosa.x64 (Linux 0.24b client, with symbols):
saveaccountsserver/loadaccountsserver, saveaccountsclient, loadblock,
building_save/building_load, leveleditor_save/leveleditor_load,
savemodelcsm/loadmodelcsm. See docs/formatos.md.

Usage as a script:
  python -I tools/formats/subrosa_formats.py <file> [...]
"""
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path


class FormatError(ValueError):
    pass


class Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0

    def take(self, n):
        if self.pos + n > len(self.data):
            raise FormatError(f'end of file: asked for {n} bytes at {self.pos}, size {len(self.data)}')
        b = self.data[self.pos:self.pos + n]
        self.pos += n
        return b

    def i32(self, n=None):
        if n is None:
            return struct.unpack('<i', self.take(4))[0]
        return list(struct.unpack(f'<{n}i', self.take(4 * n)))

    def f32(self, n=None):
        if n is None:
            return struct.unpack('<f', self.take(4))[0]
        return list(struct.unpack(f'<{n}f', self.take(4 * n)))

    def i16(self, n):
        return list(struct.unpack(f'<{n}h', self.take(2 * n)))

    def cstr(self, n):
        return self.take(n).split(b'\0', 1)[0].decode('latin-1')

    def end(self):
        if self.pos != len(self.data):
            raise FormatError(f'{len(self.data) - self.pos} bytes left over (read up to {self.pos})')


# ---------------------------------------------------------------- .srk (server)

@dataclass
class ServerAccount:
    id: tuple          # pair of client integers (player+0x24, +0x28). Identifies the player.
    name: str          # 32 bytes. The game cuts at 24 characters and adds "(n)" if the name already exists.
    money: int         # a new account starts with 1000
    unknown_30: int    # not read in version 1. Value 5 in Black Houser.
    look_a: int        # rand() % 3 at creation (look, likely)
    look_b: int        # rand() & 7
    look_c: int        # rand() & 3
    name_raw: bytes = b''  # 32 raw bytes. The game writes the whole buffer, with garbage after the 0.


@dataclass
class ServerSrk:
    version: int       # 1 to 3. The game writes 3.
    key: int           # global "account": random number generated on the first boot
    accounts: list = field(default_factory=list)


def read_server_srk(data: bytes) -> ServerSrk:
    r = Reader(data)
    version = r.i32()
    if not 0 < version < 4:
        raise FormatError(f'unknown version {version}')
    srk = ServerSrk(version, r.i32() & 0xffffffff)
    for _ in range(r.i32()):
        ident = (r.i32() & 0xffffffff, r.i32() & 0xffffffff)
        name_raw = r.take(32)
        name = name_raw.split(b'\0', 1)[0].decode('latin-1')
        money = r.i32()
        if version < 2:
            extra = (0, 0, 0, 0)  # the game rolls the look on load
        else:
            extra = tuple(r.i32(4))
        srk.accounts.append(ServerAccount(ident, name, money, *extra, name_raw=name_raw))
    r.end()
    return srk


def write_server_srk(srk: ServerSrk) -> bytes:
    out = [struct.pack('<iIi', 3, srk.key, len(srk.accounts))]
    for a in srk.accounts:
        raw = a.name_raw
        if not raw or raw.split(b'\0', 1)[0].decode('latin-1') != a.name:
            raw = a.name.encode('latin-1')[:31]  # name changed: write it clean
        out.append(struct.pack('<II32si4i', a.id[0], a.id[1], raw,
                               a.money, a.unknown_30, a.look_a, a.look_b, a.look_c))
    return b''.join(out)


# ---------------------------------------------------------------- .srk (client)

def read_client_srk(data: bytes):
    """client.srk: version 1, list of records of 4 integers (likely: server key + id)."""
    r = Reader(data)
    version = r.i32()
    if version != 1:
        raise FormatError(f'unknown version {version}')
    entries = [tuple(x & 0xffffffff for x in r.i32(4)) for _ in range(r.i32())]
    r.end()
    return {'version': version, 'entries': entries}


# ---------------------------------------------------------------- .sbl (block)

@dataclass
class BlockSurface:
    ints: list         # 6 integers: type/texture/flags (file order: +8, +0, +0xc, +4, +0x150, +0x154)
    points: list       # 4x4 control points (x, y, z): bicubic Bézier surface
    uvs: list          # 4x4 texture coordinates (u, v)


@dataclass
class Block:
    version: int
    header: list       # 10 integers. The last one is the number of surfaces.
    surfaces: list
    boxes: list        # version 2: collision boxes (8 corners + 6 integers + 1 integer)


def read_sbl(data: bytes) -> Block:
    r = Reader(data)
    version = r.i32()
    if not 1 <= version <= 2:
        raise FormatError(f'unknown version {version}')
    header = r.i32(10)
    surfaces = []
    for _ in range(header[9]):
        ints = r.i32(6)
        points, uvs = [], []
        for _i in range(16):
            points.append(tuple(r.f32(3)))
            uvs.append(tuple(r.f32(2)))
        surfaces.append(BlockSurface(ints, points, uvs))
    boxes = []
    if version >= 2:
        for _ in range(r.i32()):
            corners = [tuple(r.f32(3)) for _c in range(8)]
            boxes.append({'corners': corners, 'faces': r.i32(6), 'flag': r.i32()})
    r.end()
    return Block(version, header, surfaces, boxes)


# ---------------------------------------------------------------- .sbb (building)

@dataclass
class BuildingCell:
    a: int             # +0x00. Cell is occupied when a or b != 0.
    b: int             # +0x04
    c: int             # +0x10 (version >= 7 only)
    d: int             # +0x08
    e: int             # +0x0c
    f: int             # +0x14
    shorts: list       # 16 16-bit values (+0x18 to +0x36): textures/pieces per side (likely)
    g: int             # +0x38
    h: int             # +0x3c (zeroed in version < 8)
    i: int             # +0x40 (version >= 10 only)
    j: int             # +0x44 (version >= 10 only)


@dataclass
class Building:
    version: int       # 6 to 10. The game writes 10.
    name: str          # 64 bytes
    size_x: int        # columns (default 24)
    size_z: int        # rows (default 24)
    floors: int        # floors (default 4)
    list_a: list       # 64-byte names (likely: textures)
    list_b: list       # 64-byte names
    cells: dict        # (floor, row, column) -> BuildingCell, non-empty cells only
    extras: list       # version >= 9: 16 records of 6 integers


def read_sbb(data: bytes) -> Building:
    r = Reader(data)
    version = r.i32()
    if not 6 <= version <= 10:
        raise FormatError(f'unknown version {version}')
    name = r.cstr(64)
    size_x, size_z, floors = r.i32(3)
    list_a = [r.cstr(64) for _ in range(r.i32())]
    list_b = [r.cstr(64) for _ in range(r.i32())]
    cells = {}
    # The grid includes the border: rows 0..size_z and columns 0..size_x.
    for fl in range(floors):
        for row in range(size_z + 1):
            for col in range(size_x + 1):
                a, b = r.i32(2)
                c = r.i32() if version > 6 else 0
                d, e, f = r.i32(3)
                shorts = r.i16(16)
                g, h = r.i32(2)
                if version < 8:
                    h = 0
                i = j = 0
                if version > 9:
                    i, j = r.i32(2)
                cell = BuildingCell(a, b, c, d, e, f, shorts, g, h, i, j)
                if a or b or c or d or e or f or any(shorts) or g or h or i or j:
                    cells[(fl, row, col)] = cell
    extras = [r.i32(6) for _ in range(16)] if version > 8 else []
    r.end()
    return Building(version, name, size_x, size_z, floors, list_a, list_b, cells, extras)


# ---------------------------------------------------------------- .sbc (city)

@dataclass
class City:
    version: int       # the game only accepts 6
    intersections: list  # (x, z, type?) - 3 integers
    streets: list        # 5 integers + 32-byte name
    tiles: bytes         # 1024 x 1024 bytes (1 MB)
    buildings: list      # name of the .sbb (64 bytes) + 4 integers (position and rotation, likely)


def read_sbc(data: bytes) -> City:
    r = Reader(data)
    version = r.i32()
    if version != 6:
        raise FormatError(f'unknown version {version}')
    intersections = [tuple(r.i32(3)) for _ in range(r.i32())]
    streets = []
    for _ in range(r.i32()):
        nums = r.i32(5)
        streets.append({'ints': nums, 'name': r.cstr(32)})
    tiles = r.take(0x100000)
    buildings = []
    for _ in range(r.i32()):
        nm = r.cstr(64)
        buildings.append({'name': nm, 'ints': r.i32(4)})
    r.end()
    return City(version, intersections, streets, tiles, buildings)


# ---------------------------------------------------------------- .csm (model)

@dataclass
class Model:
    version: int
    num_vertices: int
    unknown_4: int
    num_triangles: int
    vertices: list     # (x, y, z)
    vec_b: list        # (x, y, z). In ak47 it starts with (1, 0, 0): tangent, likely
    vec_c: list        # (x, y, z). In ak47 it starts with (0, 1, 0): bitangent, likely
    tri_vertices: list  # 3 vertex indices per triangle
    tri_normals: list   # face normal (3 floats). Values close to -1.0 / 1.0 confirm it.
    tri_c: list         # 3 integers per triangle (likely: texture/UV indices)


def read_csm(data: bytes) -> Model:
    r = Reader(data)
    magic = r.take(4)
    if magic != b'CSMo':
        raise FormatError(f'invalid signature {magic!r}')
    version = r.i32()
    if version != 1:
        raise FormatError(f'unknown version {version}')
    nv, unk, nt = r.i32(3)

    def vec3f(n):
        v = r.f32(n * 3)
        return [tuple(v[k:k + 3]) for k in range(0, len(v), 3)]

    def vec3i(n):
        v = r.i32(n * 3)
        return [tuple(v[k:k + 3]) for k in range(0, len(v), 3)]

    m = Model(version, nv, unk, nt, vec3f(nv), vec3f(nv), vec3f(nv), vec3i(nt), vec3f(nt), vec3i(nt))
    r.end()
    return m


# ---------------------------------------------------------------- CLI

def read_any(path: Path):
    data = path.read_bytes()
    ext = path.suffix.lower()
    if ext == '.srk':
        return read_client_srk(data) if path.name == 'client.srk' else read_server_srk(data)
    return {'.sbl': read_sbl, '.sbb': read_sbb, '.sbc': read_sbc, '.csm': read_csm}[ext](data)


def summary(obj):
    if isinstance(obj, ServerSrk):
        return f'server v{obj.version}, {len(obj.accounts)} accounts'
    if isinstance(obj, Block):
        return f'block v{obj.version}, {len(obj.surfaces)} surfaces, {len(obj.boxes)} boxes'
    if isinstance(obj, Building):
        return (f'building v{obj.version} "{obj.name}" {obj.size_x}x{obj.size_z}x{obj.floors}, '
                f'{len(obj.cells)} cells, lists {len(obj.list_a)}/{len(obj.list_b)}')
    if isinstance(obj, City):
        return (f'city v{obj.version}, {len(obj.intersections)} intersections, {len(obj.streets)} streets, '
                f'{len(obj.buildings)} buildings')
    if isinstance(obj, Model):
        return f'model v{obj.version}, {obj.num_vertices} vertices, {obj.num_triangles} triangles'
    return str(obj)[:120]


def main(paths):
    bad = 0
    for p in map(Path, paths):
        try:
            print(f'OK    {p.name}: {summary(read_any(p))}')
        except (FormatError, KeyError, struct.error) as e:
            bad += 1
            print(f'ERROR {p.name}: {e}')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
