"""Windows backend, dedicated to the exact Sub Rosa 024d server executable."""
import ctypes as C, struct, hashlib, os
from ctypes import wintypes as W
from contextlib import contextmanager
from pathlib import Path
from core import SERVER_SHA256, PluginError

if os.name!='nt':raise ImportError('Este módulo exige Windows.')
k=C.WinDLL('kernel32',use_last_error=True)
def api(name,restype,args):
    f=getattr(k,name);f.restype=restype;f.argtypes=args;return f
HANDLE=W.HANDLE;PTR=C.c_void_p;SIZE=C.c_size_t;DWORD=W.DWORD;BOOL=W.BOOL
OpenProcess=api('OpenProcess',HANDLE,[DWORD,BOOL,DWORD]);CloseHandle=api('CloseHandle',BOOL,[HANDLE])
ReadProcessMemory=api('ReadProcessMemory',BOOL,[HANDLE,PTR,PTR,SIZE,C.POINTER(SIZE)])
WriteProcessMemory=api('WriteProcessMemory',BOOL,[HANDLE,PTR,PTR,SIZE,C.POINTER(SIZE)])
VirtualAllocEx=api('VirtualAllocEx',PTR,[HANDLE,PTR,SIZE,DWORD,DWORD]);VirtualProtectEx=api('VirtualProtectEx',BOOL,[HANDLE,PTR,SIZE,DWORD,C.POINTER(DWORD)])
FlushInstructionCache=api('FlushInstructionCache',BOOL,[HANDLE,PTR,SIZE])
GetExitCodeProcess=api('GetExitCodeProcess',BOOL,[HANDLE,C.POINTER(DWORD)])
CreateToolhelp32Snapshot=api('CreateToolhelp32Snapshot',HANDLE,[DWORD,DWORD])
QueryFullProcessImageNameW=api('QueryFullProcessImageNameW',BOOL,[HANDLE,DWORD,W.LPWSTR,C.POINTER(DWORD)])
OpenThread=api('OpenThread',HANDLE,[DWORD,BOOL,DWORD]);SuspendThread=api('SuspendThread',DWORD,[HANDLE]);ResumeThread=api('ResumeThread',DWORD,[HANDLE])
IsWow64Process=api('IsWow64Process',BOOL,[HANDLE,C.POINTER(BOOL)])
CreateRemoteThread=api('CreateRemoteThread',HANDLE,[HANDLE,PTR,SIZE,PTR,PTR,DWORD,C.POINTER(DWORD)])
WaitForSingleObject=api('WaitForSingleObject',DWORD,[HANDLE,DWORD])
GetExitCodeThread=api('GetExitCodeThread',BOOL,[HANDLE,C.POINTER(DWORD)])

class NativeCallError(PluginError):
    def __init__(self,message,uncertain=False):
        super().__init__(message);self.uncertain=uncertain
class PROCESSENTRY32(C.Structure):
    _fields_=[('dwSize',DWORD),('cntUsage',DWORD),('th32ProcessID',DWORD),('th32DefaultHeapID',C.c_size_t),('th32ModuleID',DWORD),('cntThreads',DWORD),('th32ParentProcessID',DWORD),('pcPriClassBase',W.LONG),('dwFlags',DWORD),('szExeFile',W.WCHAR*260)]
class MODULEENTRY32(C.Structure):
    _fields_=[('dwSize',DWORD),('th32ModuleID',DWORD),('th32ProcessID',DWORD),('GlblcntUsage',DWORD),('ProccntUsage',DWORD),('modBaseAddr',PTR),('modBaseSize',DWORD),('hModule',HANDLE),('szModule',W.WCHAR*256),('szExePath',W.WCHAR*260)]
class THREADENTRY32(C.Structure):
    _fields_=[('dwSize',DWORD),('cntUsage',DWORD),('th32ThreadID',DWORD),('th32OwnerProcessID',DWORD),('tpBasePri',W.LONG),('tpDeltaPri',W.LONG),('dwFlags',DWORD)]
for name,cls in [('Process32',PROCESSENTRY32),('Module32',MODULEENTRY32),('Thread32',THREADENTRY32)]:
    suffix='W' if name!='Thread32' else ''
    globals()[name+'First']=api(name+'First'+suffix,BOOL,[HANDLE,C.POINTER(cls)])
    globals()[name+'Next']=api(name+'Next'+suffix,BOOL,[HANDLE,C.POINTER(cls)])

def error(message):raise PluginError(message+' (Windows '+str(C.get_last_error())+')')
def enumeration(flags,pid,cls,first,next_):
    for attempt in range(8):
        snap=CreateToolhelp32Snapshot(flags,pid)
        if snap!=C.c_void_p(-1).value or C.get_last_error()!=24:break
    if snap==C.c_void_p(-1).value:error('Não foi possível enumerar processos/threads')
    try:
        ent=cls();ent.dwSize=C.sizeof(cls)
        ok=first(snap,C.byref(ent))
        if not ok:
            if C.get_last_error()!=18:error('Falha ao iniciar enumeração')
            return
        while ok:
            yield cls.from_buffer_copy(ent)
            ent.dwSize=C.sizeof(cls);ok=next_(snap,C.byref(ent))
        if C.get_last_error()!=18:error('Falha durante enumeração')
    finally:CloseHandle(snap)

def servers():
    return [(e.th32ProcessID,e.szExeFile) for e in enumeration(2,0,PROCESSENTRY32,Process32First,Process32Next) if e.szExeFile.lower()=='subrosadedicated024d.exe']

class WindowsBackend:
    def __init__(self,pid):
        self.pid=pid;self.handle=OpenProcess(0x0438,False,pid)
        if not self.handle:error('Não foi possível acessar o servidor. Use o mesmo nível de administrador')
        try:
            buf=C.create_unicode_buffer(32768);n=DWORD(len(buf))
            if not QueryFullProcessImageNameW(self.handle,0,buf,C.byref(n)):error('Não foi possível identificar o executável')
            self.path=Path(buf.value)
            if self.path.name.lower()!='subrosadedicated024d.exe' or hashlib.sha256(self.path.read_bytes()).hexdigest()!=SERVER_SHA256:raise PluginError('Executável incompatível. Use o servidor 024d enviado nesta conversa.')
            mods=list(enumeration(0x18,pid,MODULEENTRY32,Module32First,Module32Next))
            self.base=next(m.modBaseAddr for m in mods if m.szModule.lower()=='subrosadedicated024d.exe')
            self.allocations=[]
            self.dll_handle=None;self.native_calls=0
            # Thread context layout is the 32-bit CONTEXT (716 bytes).
            current=api('GetCurrentProcess',HANDLE,[])();wow=BOOL()
            if not IsWow64Process(current,C.byref(wow)):error('Falha ao detectar arquitetura')
            self.context_api=api('Wow64GetThreadContext' if C.sizeof(PTR)==8 else 'GetThreadContext',BOOL,[HANDLE,PTR])
            # No operation is permitted against a terminated process.
            if not self.alive():raise PluginError('O servidor já foi encerrado.')
        except Exception:
            CloseHandle(self.handle);self.handle=None;raise
    def alive(self):
        code=DWORD()
        return bool(self.handle and GetExitCodeProcess(self.handle,C.byref(code)) and code.value==259)
    def read(self,addr,n):
        buf=C.create_string_buffer(n);done=SIZE()
        if not ReadProcessMemory(self.handle,addr,buf,n,C.byref(done)) or done.value!=n:error('Falha ao ler memória')
        return buf.raw
    def pointer(self,addr):return struct.unpack('<I',self.read(addr,4))[0]
    def write(self,addr,data):
        old=DWORD();n=len(data)
        if not VirtualProtectEx(self.handle,addr,n,0x40,C.byref(old)):error('Falha ao liberar região para escrita')
        failure=None
        try:
            done=SIZE();buf=C.create_string_buffer(data)
            if not WriteProcessMemory(self.handle,addr,buf,n,C.byref(done)) or done.value!=n:error('Falha ao escrever memória')
            if self.read(addr,n)!=data:raise PluginError('Verificação da escrita falhou.')
            if not FlushInstructionCache(self.handle,addr,n):error('Falha ao atualizar cache de instruções')
        except Exception as e:failure=e
        finally:
            ignored=DWORD()
            if not VirtualProtectEx(self.handle,addr,n,old.value,C.byref(ignored)):
                failure=PluginError('Não foi possível restaurar a proteção da memória.')
        if failure:raise failure
    def allocate(self,n):
        addr=VirtualAllocEx(self.handle,None,n,0x3000,4)
        if not addr or addr+n>0x100000000:raise PluginError('Falha ao alocar memória de 32 bits.')
        self.allocations.append((addr,n));return addr
    def executable(self,addr,n):
        old=DWORD()
        if not VirtualProtectEx(self.handle,addr,n,0x20,C.byref(old)):error('Falha ao tornar o código executável')
    def _modules(self):
        return list(enumeration(0x18,self.pid,MODULEENTRY32,Module32First,Module32Next))
    def _export(self,module,name,depth=0):
        # Resolve inside the 32-bit target. Local Python may be 64-bit, and exports
        # such as kernel32!LoadLibraryExW may forward into KernelBase.
        if depth>8:raise PluginError('Encaminhamento circular de exportação.')
        modules=self._modules()
        wanted=module.lower()
        if not wanted.endswith('.dll'):wanted+='.dll'
        mod=next((m for m in modules if m.szModule.lower()==wanted),None)
        if mod is None:raise PluginError('Módulo do Windows não encontrado: '+wanted)
        base=mod.modBaseAddr;size=mod.modBaseSize
        def read(rva,n):
            if rva<0 or rva+n>size:raise PluginError('Tabela PE fora do módulo remoto.')
            return self.read(base+rva,n)
        if read(0,2)!=b'MZ':raise PluginError('Módulo remoto sem cabeçalho PE.')
        nt=struct.unpack('<I',read(0x3c,4))[0]
        if read(nt,4)!=b'PE\0\0' or struct.unpack('<H',read(nt+24,2))[0]!=0x10b:raise PluginError('Módulo remoto não é PE32.')
        rva,n=struct.unpack('<II',read(nt+24+96,8))
        if not rva or n<40:raise PluginError('Tabela de exportações ausente.')
        fields=struct.unpack('<IIHHIIIIIII',read(rva,40))
        ordinal_base,count,names,eat,ntable,otable=fields[5:11]
        if not 1<=count<=65536 or not 0<=names<=65536:raise PluginError('Tabela de exportações inválida.')
        def text(at):
            out=bytearray()
            for i in range(512):
                b=read(at+i,1)[0]
                if not b:return out.decode('ascii')
                out.append(b)
            raise PluginError('Nome de exportação inválido.')
        index=None
        if isinstance(name,int):index=name-ordinal_base
        else:
            for i in range(names):
                ptr=struct.unpack('<I',read(ntable+4*i,4))[0]
                if text(ptr)==name:index=struct.unpack('<H',read(otable+2*i,2))[0];break
        if index is None or not 0<=index<count:raise PluginError('Exportação remota não encontrada: '+str(name))
        target=struct.unpack('<I',read(eat+4*index,4))[0]
        if not 0<target<size:raise PluginError('Exportação remota inválida.')
        if rva<=target<rva+n:
            forwarded=text(target)
            target_module,target_name=forwarded.rsplit('.',1)
            return self._export(target_module,int(target_name[1:]) if target_name.startswith('#') else target_name,depth+1)
        return base+target
    def remote_call(self,address,argument,pointer_result=False):
        if not self.alive():raise PluginError('O servidor já foi encerrado.')
        if not 0<address<0x100000000 or not 0<=argument<0x100000000:raise PluginError('Chamada DLL fora do espaço de 32 bits.')
        if not self.dll_handle:
            self.dll_handle=OpenProcess(0x043a,False,self.pid)
            if not self.dll_handle:error('Não foi possível carregar DLLs neste servidor')
        thread=CreateRemoteThread(self.dll_handle,None,0,address,argument,0,None)
        if not thread:error('Não foi possível iniciar a função da DLL')
        try:
            status=WaitForSingleObject(thread,5000)
            if status!=0:
                raise NativeCallError('A chamada nativa não terminou em 5 segundos. Ela não foi interrompida; reinicie o servidor antes de continuar.',True)
            result=DWORD()
            if not GetExitCodeThread(thread,C.byref(result)):
                raise NativeCallError('Não foi possível obter o resultado da DLL. Reinicie o servidor.',True)
            if not pointer_result and result.value>=0xc0000000:
                raise NativeCallError('A chamada nativa falhou com exceção '+hex(result.value)+'. Reinicie o servidor.',True)
            return result.value
        finally:CloseHandle(thread)
    def load_dll(self,path,pe,spec):
        path=Path(path).resolve()
        matching=[m for m in self._modules() if m.szModule.lower()==path.name.lower()]
        for mod in matching:
            if os.path.normcase(str(Path(mod.szExePath).resolve()))!=os.path.normcase(str(path)):
                raise PluginError('Já existe uma DLL com este nome e outro caminho no servidor.')
        if matching:base=matching[0].modBaseAddr
        else:
            if self.native_calls>=64:raise PluginError('Limite de carregamentos DLL atingido. Reinicie o servidor.')
            from dll_support import LOAD_LIBRARY_EX_STUB
            address=self._export('kernel32.dll','LoadLibraryExW')
            payload=(str(path)+'\0').encode('utf-16-le')
            filename=self.allocate(len(payload));self.write(filename,payload)
            argument=self.allocate(12);self.write(argument,struct.pack('<III',address,filename,0x1100))
            adapter=self.allocate(len(LOAD_LIBRARY_EX_STUB));self.write(adapter,LOAD_LIBRARY_EX_STUB);self.executable(adapter,len(LOAD_LIBRARY_EX_STUB))
            self.native_calls+=1
            base=self.remote_call(adapter,argument,True)
            if not base:raise PluginError('Windows não carregou a DLL. Confira caminho, arquitetura e dependências.')
        # Validate a module already resident too; never assume it matches a new file.
        try:
            if self.read(base,2)!=b'MZ':raise PluginError('DLL remota inválida.')
            nt=self.pointer(base+0x3c)
            if self.pointer(base+nt+24+56)!=pe.image_size:raise PluginError('Outra versão da DLL está carregada. Reinicie o servidor.')
            for name in {spec['configure'],spec['stop'],spec['identity'],*([spec['prepare_stop']] if spec.get('prepare_stop') else []),*spec['symbols'].values()}:
                if self._export(path.name,name)!=base+pe.exports[name]:raise PluginError('Exportações da DLL residente não correspondem ao arquivo.')
            if self.read(base+pe.exports[spec['identity']],65)!=spec['build_id'].encode('ascii')+b'\0':
                raise PluginError('Outra compilação da DLL já está residente. Reinicie o servidor.')
        except Exception as exc:
            # A reused module has not been initialized by this operation. A newly
            # loaded module may have run DllMain; reject further edits on uncertainty.
            if not matching:raise NativeCallError(str(exc),True) from exc
            raise
        return base
    @contextmanager
    def pause(self,forbidden):
        held={}
        try:
            # Repeat until the set stabilizes, so newly created threads are also suspended.
            stable=False
            for _ in range(8):
                entries=[e for e in enumeration(4,0,THREADENTRY32,Thread32First,Thread32Next) if e.th32OwnerProcessID==self.pid]
                new=[e for e in entries if e.th32ThreadID not in held]
                if not new:stable=True;break
                for ent in new:
                    h=OpenThread(0x005a,False,ent.th32ThreadID)
                    if not h:error('Falha ao acessar thread do servidor')
                    if SuspendThread(h)==0xffffffff:CloseHandle(h);error('Falha ao pausar thread do servidor')
                    held[ent.th32ThreadID]=h
            if not held or not stable:raise PluginError('Não foi possível estabilizar as threads do servidor. Tente novamente.')
            for h in held.values():
                context=C.create_string_buffer(716)
                struct.pack_into('<I',context,0,0x10001)
                if not self.context_api(h,context):error('Não foi possível verificar a execução das threads')
                ip=struct.unpack_from('<I',context,184)[0]
                if any(a<=ip<a+n for a,n in forbidden):raise PluginError('O servidor está executando um trecho de mod. Tente novamente em instantes.')
            yield
        finally:
            failures=[]
            for h in reversed(list(held.values())):
                if ResumeThread(h)==0xffffffff:failures.append(C.get_last_error())
                CloseHandle(h)
            if failures:raise PluginError('Falha ao retomar threads: '+str(failures)+'. Encerre e reinicie o servidor.')
    def close(self):
        if getattr(self,'dll_handle',None):CloseHandle(self.dll_handle);self.dll_handle=None
        if self.handle:CloseHandle(self.handle);self.handle=None
