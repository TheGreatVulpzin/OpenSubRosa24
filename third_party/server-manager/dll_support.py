"""PE32 inspection for native plugins; no Windows imports or third-party modules."""
import hashlib
import re
import struct
from pathlib import Path
from core import PluginError

# WINAPI thread entry: argument points to {LoadLibraryExW, UTF16 path, flags}.
# The adapter passes three arguments and returns the module handle (ret 4).
LOAD_LIBRARY_EX_STUB = bytes.fromhex('8b442404ff70086a00ff7004ff10c20400')


def validate_dll_spec(plugin):
    spec = plugin.get('dll')
    if not isinstance(spec, dict) or spec.get('abi') != 1:
        raise PluginError('Plugin DLL precisa usar ABI 1.')
    file = spec.get('file', '')
    path = Path(file)
    if not file or '\\' in file or ':' in file or path.is_absolute() or '..' in path.parts or path.suffix.lower() != '.dll':
        raise PluginError('Use um caminho relativo .dll dentro da pasta plugins.')
    for key in ('sha256', 'build_id'):
        if not isinstance(spec.get(key), str) or not re.fullmatch('[0-9a-f]{64}', spec[key]):
            raise PluginError('Identificação da DLL inválida: ' + key)
    for key in ('configure', 'stop', 'identity'):
        if not isinstance(spec.get(key), str) or not spec[key]:
            raise PluginError('Exportação obrigatória ausente: ' + key)
    if 'prepare_stop' in spec and (not isinstance(spec['prepare_stop'], str) or not spec['prepare_stop']):
        raise PluginError('Exportação prepare_stop inválida.')
    symbols = spec.get('symbols', {})
    if not isinstance(symbols, dict) or not symbols or any(not isinstance(k, str) or not isinstance(v, str) or not v for k, v in symbols.items()):
        raise PluginError('Símbolos da DLL inválidos.')
    if any(k == '__server' or k in plugin.get('imports', {}) for k in symbols):
        raise PluginError('Nome reservado usado por um símbolo DLL.')
    section = next((s for s in plugin.get('sections', []) if s['name'] == spec.get('context_section')), None)
    offset = spec.get('context_offset', 0)
    if section is None or section['name'] != '.inject_data' or not isinstance(offset, int) or offset < 0 or offset + 16 > len(bytes.fromhex(section['data'])):
        raise PluginError('Contexto da DLL deve ficar dentro de .inject_data.')
    if plugin.get('command') and offset < 20:
        raise PluginError('Reserve os primeiros 20 bytes para o protocolo de comando; coloque o contexto DLL depois deles.')


class PE32:
    def __init__(self, data):
        self.data = data
        try:
            if len(data) > 32 * 1024 * 1024 or data[:2] != b'MZ':
                raise ValueError('cabeçalho DOS')
            nt = self.unpack('<I', 0x3c)[0]
            if data[nt:nt + 4] != b'PE\0\0':
                raise ValueError('assinatura PE')
            machine, count = self.unpack('<HH', nt + 4)
            opt_size, flags = self.unpack('<HH', nt + 20)
            opt = nt + 24
            if machine != 0x14c or not flags & 0x2000 or self.unpack('<H', opt)[0] != 0x10b or not 1 <= count <= 96 or opt_size < 104:
                raise ValueError('precisa ser DLL x86/PE32')
            self.image_size = self.unpack('<I', opt + 56)[0]
            self.header_size = self.unpack('<I', opt + 60)[0]
            if not 4096 <= self.image_size <= 128 * 1024 * 1024:
                raise ValueError('tamanho da imagem')
            self.export_rva, self.export_size = self.unpack('<II', opt + 96)
            self.sections = []
            for i in range(count):
                at = opt + opt_size + 40 * i
                virtual_size, rva, size, raw = self.unpack('<IIII', at + 8)
                characteristics = self.unpack('<I', at + 36)[0]
                if raw + size > len(data) or rva + max(virtual_size, size) > self.image_size:
                    raise ValueError('seção fora da imagem')
                self.sections.append((rva, virtual_size, size, raw, characteristics))
            if not self.export_rva or self.export_size < 40:
                raise ValueError('DLL sem as exportações do SDK')
            fields = struct.unpack('<IIHHIIIIIII', self.at(self.export_rva, 40))
            functions, names, eat, name_table, ordinals = fields[6:11]
            if not 1 <= functions <= 65536 or not 1 <= names <= 65536:
                raise ValueError('tabela de exportações')
            self.exports = {}
            for i in range(names):
                name_rva = struct.unpack('<I', self.at(name_table + i * 4, 4))[0]
                ordinal = struct.unpack('<H', self.at(ordinals + i * 2, 2))[0]
                if ordinal >= functions:
                    raise ValueError('ordinal inválido')
                target = struct.unpack('<I', self.at(eat + ordinal * 4, 4))[0]
                if not 0 < target < self.image_size or self.export_rva <= target < self.export_rva + self.export_size:
                    raise ValueError('exportação inválida ou encaminhada')
                name = self.cstring(name_rva)
                if name in self.exports:
                    raise ValueError('exportação duplicada')
                self.exports[name] = target
        except (ValueError, struct.error, UnicodeError, IndexError) as exc:
            raise PluginError('DLL incompatível ou danificada: ' + str(exc)) from exc

    def unpack(self, fmt, offset):
        return struct.unpack_from(fmt, self.data, offset)

    def at(self, rva, size):
        if rva < 0 or size < 0:
            raise ValueError('RVA negativo')
        if rva + size <= min(self.header_size, len(self.data)):
            return self.data[rva:rva + size]
        for start, virtual, length, raw, flags in self.sections:
            if start <= rva and rva + size <= start + length:
                return self.data[raw + rva - start:raw + rva - start + size]
        raise ValueError('RVA sem dados no arquivo')

    def cstring(self, rva):
        out = bytearray()
        for i in range(512):
            b = self.at(rva + i, 1)[0]
            if not b:
                return out.decode('ascii')
            out.append(b)
        raise ValueError('nome de exportação longo demais')

    def executable(self, rva):
        return any(start <= rva < start + max(virtual, length) and flags & 0x20000000 for start, virtual, length, raw, flags in self.sections)


def inspect_dll(plugin, folder=None):
    validate_dll_spec(plugin)
    spec = plugin['dll']
    root = Path(folder or plugin.get('_plugin_folder') or Path(__file__).parent / 'plugins').resolve()
    path = (root / spec['file']).resolve()
    if not path.is_relative_to(root) or not path.is_file():
        raise PluginError('DLL não encontrada na pasta plugins: ' + spec['file'])
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != spec['sha256']:
        raise PluginError('DLL diferente do arquivo indicado no JSON. Recompile/atualize o manifesto.')
    pe = PE32(data)
    functions = [spec['configure'], spec['stop'], *([spec['prepare_stop']] if spec.get('prepare_stop') else []), *spec['symbols'].values()]
    for name in [*functions, spec['identity']]:
        if name not in pe.exports:
            raise PluginError('DLL não exporta ' + name + '.')
    if any(not pe.executable(pe.exports[name]) for name in functions):
        raise PluginError('Uma exportação de função aponta para dados.')
    identity = pe.exports[spec['identity']]
    try:
        if pe.at(identity, 65) != spec['build_id'].encode('ascii') + b'\0':
            raise PluginError('Build ID da DLL não corresponde ao manifesto.')
    except ValueError as exc:
        raise PluginError('Build ID inválido.') from exc
    return path, pe
