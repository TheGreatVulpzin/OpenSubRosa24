"""Portable plugin engine. Windows operations live in win32_backend.py."""
import json, struct, hashlib, math
from pathlib import Path

SERVER_SHA256='11e006786d945d5731cb664fba718a094d8f02648b13fcc20c26c8614ea603fd'
class PluginError(RuntimeError): pass

def overlap(a,b): return a[0]<b[0]+b[1] and b[0]<a[0]+a[1]

def validate_plugin(p):
    if p.get('format')!=1 or not isinstance(p.get('id'),str):raise PluginError('Formato de plugin inválido.')
    if p.get('unavailable'):return
    if p.get('kind', 'patch') not in ('patch', 'dll'):raise PluginError('Tipo de plugin desconhecido.')
    if p.get('kind')=='dll':
        from dll_support import validate_dll_spec
        validate_dll_spec(p)
    if p.get('thread'):raise PluginError('createThread não é suportado pelo gerenciador.')
    if p.get('command'):
        data=next((s for s in p['sections'] if s['name']=='.inject_data'),None)
        key=p['command'].get('quantity_parameter')
        if data is None or len(bytes.fromhex(data['data']))<20 or key not in p.get('parameters',{}) or p['parameters'][key]['type']!='int':raise PluginError('Protocolo de comando inválido.')
    names=set();ranges=[]
    for s in p['sections']:
        if s['name'] in names:raise PluginError('Seção duplicada.')
        names.add(s['name']);data=bytes.fromhex(s['data'])
        if not data or len(data)>65536:raise PluginError('Seção vazia ou grande demais.')
        if s['name'] not in ('.inject','.inject_data'):
            rva=s['rva']
            if not isinstance(rva,int) or rva<0 or rva+len(data)>0x25f3b000:raise PluginError('Endereço fora do servidor.')
            if any(overlap((rva,len(data)),x) for x in ranges):raise PluginError('Sobreposição no próprio plugin.')
            ranges.append((rva,len(data)))
            if s.get('expected') is not None and len(bytes.fromhex(s['expected']))!=len(data):raise PluginError('Original com tamanho inválido.')
        for r in s.get('relocations',[]):
            if r['type'] not in (1,2) or r['offset']<0 or r['offset']+4>len(data):raise PluginError('Relocação inválida.')
    for spec in p.get('parameters',{}).values():
        for site in spec['sites']:
            s=next((s for s in p['sections'] if s['name']==site['section']),None)
            if s is None or site['offset']<0 or site['offset']+site.get('width',4)>len(bytes.fromhex(s['data'])):raise PluginError('Parâmetro fora da seção.')

def load_plugins(folder):
    result={};errors=[]
    for path in sorted(Path(folder).glob('*.json')):
        try:
            p=json.loads(path.read_text(encoding='utf-8'));validate_plugin(p)
            if p['id'] in result:raise PluginError('ID duplicado.')
            if p.get('kind')=='dll':p['_plugin_folder']=str(Path(folder).resolve())
            result[p['id']]=p
        except Exception as e:errors.append(f'{path.name}: {e}')
    return result,errors

def parameter_values(plugin,provided):
    values={}
    for name,spec in plugin.get('parameters',{}).items():
        raw=provided.get(name,spec['default'])
        if spec['type']=='float' and isinstance(raw,str):raw=raw.replace(',','.')
        try:v=float(raw) if spec['type']=='float' else int(raw)
        except (ValueError,TypeError):raise PluginError(f'Valor inválido: {spec["label"]}')
        if not math.isfinite(v) or not spec['min']<=v<=spec['max']:raise PluginError(f'{spec["label"]}: use {spec["min"]} a {spec["max"]}.')
        values[name]=v
    return values

class Engine:
    def __init__(self,backend):
        self.backend=backend;self.active={};self.retained=[];self.poisoned=False;self.dll_modules={}
    def _symbol(self,sym,addresses,plugin):
        if sym['index']==0:
            if sym['name']=='__server':return self.backend.base
            if sym['name'] in addresses.get('_dll_exports',{}):return addresses['_dll_exports'][sym['name']]
            if sym['name'] in plugin.get('imports',{}):return self.backend.pointer(self.backend.base+plugin['imports'][sym['name']])
            raise PluginError('Símbolo não resolvido: '+sym['name'])
        if sym['index']==0xfff1:return sym['value']
        return addresses[sym['section']]+sym['value']
    def _image(self,plugin,addresses,values):
        images={}
        for sec in plugin['sections']:
            data=bytearray.fromhex(sec['data'])
            for rel in sec.get('relocations',[]):
                at=rel['offset'];a=struct.unpack_from('<I',data,at)[0]
                s=self._symbol(rel['symbol'],addresses,plugin)
                v=s+a-(addresses[sec['name']]+at if rel['type']==2 else 0)
                struct.pack_into('<I',data,at,v&0xffffffff)
            images[sec['name']]=data
        for key,spec in plugin.get('parameters',{}).items():
            for site in spec['sites']:
                v=values[key]*site.get('scale',1)
                struct.pack_into('<f' if spec['type']=='float' else ('<B' if spec['type']=='byte' else '<i'),images[site['section']],site['offset'],v if spec['type']=='float' else int(v))
        return {key:bytes(value) for key,value in images.items()}
    def enable(self,plugin,parameters=None):
        if self.poisoned:raise PluginError('Sessão bloqueada após erro de restauração. Reinicie o servidor.')
        ident=plugin['id'];validate_plugin(plugin)
        if plugin.get('unavailable'):raise PluginError(plugin['unavailable'])
        if ident in self.active:raise PluginError('Já está ativado. Desative antes de alterar a configuração.')
        values=parameter_values(plugin,parameters or {});addresses={};ranges=[]
        for sec in plugin['sections']:
            if sec['name'] in ('.inject','.inject_data'):continue
            address=self.backend.base+sec['rva'];n=len(bytes.fromhex(sec['data']))
            for state in self.active.values():
                if any(overlap((address,n),(a,len(b))) for a,b in state['installed']):raise PluginError('Conflito com: '+state['plugin']['name'])
            addresses[sec['name']]=address;ranges.append((address,n))
        native=None
        if plugin.get('kind')=='dll':
            from dll_support import inspect_dll
            path,pe=inspect_dll(plugin)
            for state in self.active.values():
                if state.get('dll_path')==str(path):raise PluginError('Esta DLL já está ativa em outro módulo.')
            native=(path,pe)
        # Limit retained executable blocks to bound repeated hot toggles.
        count=sum(s['name'] in ('.inject','.inject_data') for s in plugin['sections'])
        if count and len(self.retained)+count>256:raise PluginError('Limite de 256 blocos de código/dados atingido. Reinicie o servidor.')
        for sec in plugin['sections']:
            if sec['name'] in ('.inject','.inject_data'):
                addr=self.backend.allocate(len(bytes.fromhex(sec['data'])));addresses[sec['name']]=addr
                self.retained.append((addr,len(bytes.fromhex(sec['data']))))
        if native:
            path,pe=native;spec=plugin['dll'];key=str(path)
            cached=self.dll_modules.get(key)
            if cached and cached['sha256']!=spec['sha256']:raise PluginError('Esta DLL já está residente. Reinicie o servidor antes de trocar seu arquivo.')
            if not cached:
                try:base=self.backend.load_dll(path,pe,spec)
                except Exception as exc:
                    if getattr(exc,'uncertain',False):self.poisoned=True
                    raise
                cached={'base':base,'sha256':spec['sha256']};self.dll_modules[key]=cached
            addresses['_dll_exports']={name:cached['base']+pe.exports[export] for name,export in spec['symbols'].items()}
        images=self._image(plugin,addresses,values)
        for name in ('.inject','.inject_data'):
            if name in images:
                self.backend.write(addresses[name],images[name])
                if name=='.inject':self.backend.executable(addresses[name],len(images[name]))
        dll_state={}
        if native:
            path,pe=native;spec=plugin['dll'];base=self.dll_modules[str(path)]['base']
            context=addresses[spec['context_section']]+spec.get('context_offset',0)
            dll_state={'dll_path':str(path),'dll_stop':base+pe.exports[spec['stop']],'dll_context':context}
            if spec.get('prepare_stop'):dll_state['dll_prepare_stop']=base+pe.exports[spec['prepare_stop']]
            try:self._dll_call(base+pe.exports[spec['configure']],context)
            except Exception:
                if not self.poisoned:self._dll_stop(dll_state)
                raise
        try:
            original=[];installed=[]
            with self.backend.pause(ranges+self.retained):
                for sec in plugin['sections']:
                    if sec['name'] in ('.inject','.inject_data'):continue
                    address=addresses[sec['name']];data=images[sec['name']];old=self.backend.read(address,len(data))
                    expected=sec.get('expected')
                    if expected is not None:
                        expected=bytearray.fromhex(expected)
                        for offset in sec.get('expected_relocations',[]):
                            v=struct.unpack_from('<I',expected,offset)[0]+self.backend.base-0x400000
                            struct.pack_into('<I',expected,offset,v&0xffffffff)
                    if expected is not None and old!=bytes(expected):raise PluginError('Código diferente do esperado em '+hex(sec['rva'])+'. Há outro mod ou versão diferente.')
                    original.append((address,old));installed.append((address,data))
                try:
                    for address,data in installed:self.backend.write(address,data)
                except Exception:
                    self._restore(original);raise
                self.active[ident]={'plugin':plugin,'values':values,'original':original,'installed':installed,'addresses':addresses,**dll_state}
        except Exception:
            if native and not self.poisoned:self._dll_stop(dll_state)
            raise

    def _dll_call(self,address,context):
        try:code=self.backend.remote_call(address,context)
        except Exception as exc:
            if getattr(exc,'uncertain',False):self.poisoned=True
            raise
        if code!=0:raise PluginError('A DLL recusou a operação (código '+str(code)+').')

    def _dll_stop(self,state):
        try:self._dll_call(state['dll_stop'],state['dll_context'])
        except Exception:
            self.poisoned=True
            raise PluginError('A DLL não confirmou a parada. Reinicie o servidor antes de continuar.')

    def _restore(self,patches):
        failures=[]
        for address,data in reversed(patches):
            try:self.backend.write(address,data)
            except Exception as e:failures.append(str(e))
        if failures:self.poisoned=True;raise PluginError('Falha ao restaurar memória: '+'; '.join(failures))
    def disable(self,ident):
        if self.poisoned:raise PluginError('Sessão bloqueada. Reinicie o servidor.')
        state=self.active[ident];ranges=[(a,len(b)) for a,b in state['installed']]
        if state.get('dll_prepare_stop'):
            # Validate first, then let the game thread remove owned objects.
            with self.backend.pause(ranges+self.retained):
                for a,b in state['installed']:
                    if self.backend.read(a,len(b))!=b:raise PluginError('O hook foi alterado; limpeza cancelada.')
            self._dll_call(state['dll_prepare_stop'],state['dll_context'])
        with self.backend.pause(ranges+self.retained):
            for a,b in state['installed']:
                if self.backend.read(a,len(b))!=b:raise PluginError('A memória foi alterada por outro programa; restauração cancelada.')
            try:
                for a,b in reversed(state['original']):self.backend.write(a,b)
            except Exception:
                self._restore(state['installed']);raise
        if state.get('dll_stop'):self._dll_stop(state)
        del self.active[ident]
    def disable_all(self):
        for ident in list(self.active)[::-1]:self.disable(ident)

    def command(self,ident,parameters):
        if self.poisoned:raise PluginError('Sessão bloqueada. Reinicie o servidor.')
        state=self.active[ident];p=state['plugin'];cmd=p.get('command')
        if not cmd:raise PluginError('Este módulo não oferece comando.')
        values=parameter_values(p,parameters)
        n=values[cmd['quantity_parameter']]
        a=state['addresses']['.inject_data']
        with self.backend.pause(self.retained+[(addr,len(data)) for addr,data in state['installed']]):
            for address,installed in state['installed']:
                if self.backend.read(address,len(installed))!=installed:raise PluginError('O hook foi alterado por outro programa; comando cancelado.')
            original=self.backend.read(a,20)
            if struct.unpack_from('<i',original,0)[0]!=0:raise PluginError('Já existe um pedido pendente. Aguarde concluir ou desative o módulo.')
            try:self.backend.write(a,struct.pack('<iiiii',1,n,n,0,0))
            except Exception:
                self._restore([(a,original)]);raise
        state['values']=values
    def command_status(self,ident):
        state=self.active[ident];a=state['addresses']['.inject_data']
        request,n,remaining,created,status=struct.unpack('<iiiii',self.backend.read(a,20))
        return {'pending':bool(request),'quantity':n,'remaining':remaining,'created':created,'status':status}
