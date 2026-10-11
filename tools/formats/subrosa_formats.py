"""Leitores dos formatos de arquivo do Sub Rosa 0.24.

Formatos (todos little-endian, campos de 4 bytes salvo indicação):
  .srk  contas do servidor (user/server.srk) e do cliente (client.srk)
  .sbl  bloco: peça de construção (parede, porta, escada) em data/block/
  .sbb  prédio: grade 3D de células em data/building/
  .sbc  cidade: ruas, cruzamentos, mapa de tiles e prédios (data/city.sbc)
  .csm  modelo 3D (data/weapon/*.csm)

Fonte: decompilação do subrosa.x64 (cliente Linux 0.24b, com símbolos):
saveaccountsserver/loadaccountsserver, saveaccountsclient, loadblock,
building_save/building_load, leveleditor_save/leveleditor_load,
savemodelcsm/loadmodelcsm. Ver docs/formatos.md.

Uso como script:
  python -I tools/formats/subrosa_formats.py <arquivo> [...]
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
            raise FormatError(f'fim do arquivo: pediu {n} bytes em {self.pos}, tamanho {len(self.data)}')
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
            raise FormatError(f'sobraram {len(self.data) - self.pos} bytes (lido até {self.pos})')


# ---------------------------------------------------------------- .srk (servidor)

@dataclass
class ServerAccount:
    id: tuple          # par de inteiros do cliente (player+0x24, +0x28). Identifica o jogador.
    name: str          # 32 bytes. O jogo corta em 24 caracteres e soma "(n)" se o nome já existe.
    money: int         # conta nova começa com 1000
    unknown_30: int    # não lido na versão 1. Valor 5 no Black Houser.
    look_a: int        # rand() % 3 na criação (aparência, provável)
    look_b: int        # rand() & 7
    look_c: int        # rand() & 3
    name_raw: bytes = b''  # 32 bytes brutos. O jogo grava o buffer inteiro, com lixo depois do 0.


@dataclass
class ServerSrk:
    version: int       # 1 a 3. O jogo grava 3.
    key: int           # global "account": número aleatório gerado no primeiro boot
    accounts: list = field(default_factory=list)


def read_server_srk(data: bytes) -> ServerSrk:
    r = Reader(data)
    version = r.i32()
    if not 0 < version < 4:
        raise FormatError(f'versão {version} desconhecida')
    srk = ServerSrk(version, r.i32() & 0xffffffff)
    for _ in range(r.i32()):
        ident = (r.i32() & 0xffffffff, r.i32() & 0xffffffff)
        name_raw = r.take(32)
        name = name_raw.split(b'\0', 1)[0].decode('latin-1')
        money = r.i32()
        if version < 2:
            extra = (0, 0, 0, 0)  # o jogo sorteia a aparência ao carregar
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
            raw = a.name.encode('latin-1')[:31]  # nome mudou: grava limpo
        out.append(struct.pack('<II32si4i', a.id[0], a.id[1], raw,
                               a.money, a.unknown_30, a.look_a, a.look_b, a.look_c))
    return b''.join(out)


# ---------------------------------------------------------------- .srk (cliente)

def read_client_srk(data: bytes):
    """client.srk: versão 1, lista de registros de 4 inteiros (provável: chave do servidor + id)."""
    r = Reader(data)
    version = r.i32()
    if version != 1:
        raise FormatError(f'versão {version} desconhecida')
    entries = [tuple(x & 0xffffffff for x in r.i32(4)) for _ in range(r.i32())]
    r.end()
    return {'version': version, 'entries': entries}


# ---------------------------------------------------------------- .sbl (bloco)

@dataclass
class BlockSurface:
    ints: list         # 6 inteiros: tipo/textura/flags (ordem do arquivo: +8, +0, +0xc, +4, +0x150, +0x154)
    points: list       # 4x4 pontos de controle (x, y, z): superfície Bézier bicúbica
    uvs: list          # 4x4 coordenadas de textura (u, v)


@dataclass
class Block:
    version: int
    header: list       # 10 inteiros. O último é o número de superfícies.
    surfaces: list
    boxes: list        # versão 2: caixas de colisão (8 cantos + 6 inteiros + 1 inteiro)


def read_sbl(data: bytes) -> Block:
    r = Reader(data)
    version = r.i32()
    if not 1 <= version <= 2:
        raise FormatError(f'versão {version} desconhecida')
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


# ---------------------------------------------------------------- .sbb (prédio)

@dataclass
class BuildingCell:
    a: int             # +0x00. Célula ocupada quando a ou b != 0.
    b: int             # +0x04
    c: int             # +0x10 (só versão >= 7)
    d: int             # +0x08
    e: int             # +0x0c
    f: int             # +0x14
    shorts: list       # 16 valores de 16 bits (+0x18 a +0x36): texturas/peças por lado (provável)
    g: int             # +0x38
    h: int             # +0x3c (zerado em versão < 8)
    i: int             # +0x40 (só versão >= 10)
    j: int             # +0x44 (só versão >= 10)


@dataclass
class Building:
    version: int       # 6 a 10. O jogo grava 10.
    name: str          # 64 bytes
    size_x: int        # colunas (padrão 24)
    size_z: int        # linhas (padrão 24)
    floors: int        # andares (padrão 4)
    list_a: list       # nomes de 64 bytes (provável: texturas)
    list_b: list       # nomes de 64 bytes
    cells: dict        # (andar, linha, coluna) -> BuildingCell, só células não vazias
    extras: list       # versão >= 9: 16 registros de 6 inteiros


def read_sbb(data: bytes) -> Building:
    r = Reader(data)
    version = r.i32()
    if not 6 <= version <= 10:
        raise FormatError(f'versão {version} desconhecida')
    name = r.cstr(64)
    size_x, size_z, floors = r.i32(3)
    list_a = [r.cstr(64) for _ in range(r.i32())]
    list_b = [r.cstr(64) for _ in range(r.i32())]
    cells = {}
    # A grade inclui a borda: linhas 0..size_z e colunas 0..size_x.
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


# ---------------------------------------------------------------- .sbc (cidade)

@dataclass
class City:
    version: int       # o jogo só aceita 6
    intersections: list  # (x, z, tipo?) - 3 inteiros
    streets: list        # 5 inteiros + nome de 32 bytes
    tiles: bytes         # 1024 x 1024 bytes (1 MB)
    buildings: list      # nome do .sbb (64 bytes) + 4 inteiros (posição e rotação, provável)


def read_sbc(data: bytes) -> City:
    r = Reader(data)
    version = r.i32()
    if version != 6:
        raise FormatError(f'versão {version} desconhecida')
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


# ---------------------------------------------------------------- .csm (modelo)

@dataclass
class Model:
    version: int
    num_vertices: int
    unknown_4: int
    num_triangles: int
    vertices: list     # (x, y, z)
    vec_b: list        # (x, y, z). No ak47 começa com (1, 0, 0): tangente, provável
    vec_c: list        # (x, y, z). No ak47 começa com (0, 1, 0): bitangente, provável
    tri_vertices: list  # 3 índices de vértice por triângulo
    tri_normals: list   # normal da face (3 floats). Valores perto de -1.0 / 1.0 confirmam.
    tri_c: list         # 3 inteiros por triângulo (provável: índices de textura/UV)


def read_csm(data: bytes) -> Model:
    r = Reader(data)
    magic = r.take(4)
    if magic != b'CSMo':
        raise FormatError(f'assinatura {magic!r} inválida')
    version = r.i32()
    if version != 1:
        raise FormatError(f'versão {version} desconhecida')
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
        return f'servidor v{obj.version}, {len(obj.accounts)} contas'
    if isinstance(obj, Block):
        return f'bloco v{obj.version}, {len(obj.surfaces)} superfícies, {len(obj.boxes)} caixas'
    if isinstance(obj, Building):
        return (f'prédio v{obj.version} "{obj.name}" {obj.size_x}x{obj.size_z}x{obj.floors}, '
                f'{len(obj.cells)} células, listas {len(obj.list_a)}/{len(obj.list_b)}')
    if isinstance(obj, City):
        return (f'cidade v{obj.version}, {len(obj.intersections)} cruzamentos, {len(obj.streets)} ruas, '
                f'{len(obj.buildings)} prédios')
    if isinstance(obj, Model):
        return f'modelo v{obj.version}, {obj.num_vertices} vértices, {obj.num_triangles} triângulos'
    return str(obj)[:120]


def main(paths):
    bad = 0
    for p in map(Path, paths):
        try:
            print(f'OK    {p.name}: {summary(read_any(p))}')
        except (FormatError, KeyError, struct.error) as e:
            bad += 1
            print(f'ERRO  {p.name}: {e}')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
