"""Developer test: emulate actual x86 NPC/body/AI routines in a synthetic world.
Requires GNU gcc/binutils, pefile and unicorn. Does not run Windows or clients.
"""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,sys,tempfile
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_INTR
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from core import Engine,parameter_values,SERVER_SHA256
ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('server',type=Path);a=ap.parse_args()
assert hashlib.sha256(a.server.read_bytes()).hexdigest()==SERVER_SHA256,'Incompatible server'
pe=pefile.PE(str(a.server));image=bytearray(0x62000)
for s in pe.sections:
    if s.VirtualAddress<len(image):image[s.VirtualAddress:s.VirtualAddress+s.SizeOfRawData]=s.get_data()[:s.SizeOfRawData]
p=json.loads((ROOT/'tools/npc_spawner-archived.json').read_text())
class Backend:base=0x400000
addresses={s['name']:0x50000000 if s['name']=='.inject' else 0x50010000 if s['name']=='.inject_data' else 0x400000+s['rva'] for s in p['sections']}
images=Engine(Backend())._image(p,addresses,parameter_values(p,{}))
with tempfile.TemporaryDirectory() as td:
    t=Path(td);assembly=['.section .rodata']
    for name,data in [('pe',image),('npc',images['.inject']),('data',images['.inject_data'])]:
        path=t/(name+'.bin');path.write_bytes(data)
        assembly+=['.global '+name+'_blob,'+name+'_end',name+'_blob: .incbin '+json.dumps(str(path)),name+'_end:']
    assembly+=['.section .note.GNU-stack,"",@progbits'];(t/'blobs.s').write_text('\n'.join(assembly)+'\n')
    out=t/'harness'
    subprocess.run(['gcc','-m32','-O2','-fno-pie','-fno-builtin','-fno-stack-protector','-nostdlib','-static','-no-pie','-Wl,-Ttext-segment=0x60000000','-o',str(out),str(ROOT/'tools/native_spawn_harness.c'),str(t/'blobs.s')],check=True)
    b=out.read_bytes()
u=Uc(UC_ARCH_X86,UC_MODE_32);entry,off=struct.unpack_from('<II',b,24);size,num=struct.unpack_from('<HH',b,42)
for i in range(num):
    typ,fo,va,pa,fs,ms,fl,al=struct.unpack_from('<IIIIIIII',b,off+i*size)
    if typ==1:
        start=va&~4095;end=(va+ms+4095)&~4095;u.mem_map(start,end-start);u.mem_write(va,b[fo:fo+fs])
u.mem_map(0x70000000,0x100000);u.reg_write(UC_X86_REG_ESP,0x700ff000);result=[]
def interrupt(uc,n,data):
    if n!=128:raise RuntimeError(f'CPU interrupt {n} at {uc.reg_read(UC_X86_REG_EIP):x}')
    number=uc.reg_read(UC_X86_REG_EAX);bx=uc.reg_read(UC_X86_REG_EBX)
    if number==90:
        addr,sz,prot,flags,fd,offset=struct.unpack('<6I',uc.mem_read(bx,24));uc.mem_map(addr,(sz+4095)&~4095);uc.reg_write(UC_X86_REG_EAX,addr)
    elif number==4:
        count=uc.reg_read(UC_X86_REG_EDX);print(bytes(uc.mem_read(uc.reg_read(UC_X86_REG_ECX),count)).decode().strip());uc.reg_write(UC_X86_REG_EAX,count)
    elif number==1:result.append(bx);uc.emu_stop()
    else:raise RuntimeError(f'Unexpected syscall {number}')
u.hook_add(UC_HOOK_INTR,interrupt);u.emu_start(entry,0,count=30000000)
assert result==[0],f'Native harness failed; check code {result}, EIP={u.reg_read(UC_X86_REG_EIP):x}'
print('32 native assertions passed (synthetic world; Windows/client replication not tested).')
