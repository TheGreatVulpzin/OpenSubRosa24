"""Execute death DLL, hooks and stock death/message code using Unicorn.
No live Windows or game client test. Requires pefile, unicorn and the 024d exe.
"""
import hashlib, struct, sys
from pathlib import Path
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from core import Engine,load_plugins,SERVER_SHA256
from dll_support import inspect_dll
server=Path(sys.argv[1]);assert hashlib.sha256(server.read_bytes()).hexdigest()==SERVER_SHA256
plugins,errors=load_plugins(ROOT/'plugins');assert not errors
checks=0
for base,dbase in [(0x400000,0x68000000),(0x10000000,0x71000000)]:
 u=Uc(UC_ARCH_X86,UC_MODE_32);ctx=0x60000000;code=0x60010000;stack=0x7f000000;stop=0x7e000000
 for at,size in [(base,0x61000),(ctx,0x1000),(code,0x4000),(stack,0x10000),(stop,0x4000),(base+0x90dc000,0x6000),(base+0xeded000,0x242000),(base+0x15cb000,0x580000),(base+0x761e000,0x1000),(base+0xf437000,0x1000),(base+0x93dd000,0x10000),(base+0x1409000,0x2000),(base+0x17529000,0x1000),(base+0x5592000,0x10000)]:u.mem_map(at,size)
 p=plugins['death_messages'];dllpath,meta=inspect_dll(p)
 pe=pefile.PE(str(server));pe.relocate_image(base)
 for sec in pe.sections:
  if sec.VirtualAddress<0x61000:u.mem_write(base+sec.VirtualAddress,bytes(sec.get_data()))
 dll=pefile.PE(str(dllpath));dll.relocate_image(dbase);u.mem_map(dbase,meta.image_size);u.mem_write(dbase,dll.get_memory_mapped_image())
 api={}
 for desc in dll.DIRECTORY_ENTRY_IMPORT:
  assert desc.dll==b'KERNEL32.dll'
  for entry in desc.imports:
   at=stop+0x1000+len(api)*0x20;api[at]=entry.name.decode();u.mem_write(entry.address,struct.pack('<I',at))
 def put(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
 def get(at):return struct.unpack('<I',u.mem_read(at,4))[0]
 def human(i):return base+0x15cbf80+i*0x2b64
 def player(i):return base+0xededd00+i*0x2408
 penalties=[];finish_at=[stop]
 def ret(value=0,n=0):
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+4*n);u.reg_write(UC_X86_REG_EIP,get(sp))
 def hook(vm,at,size,arg):
  if at==finish_at[0]:vm.emu_stop();return
  sp=vm.reg_read(UC_X86_REG_ESP)
  if at in api:
   if api[at]=='GetModuleHandleW':ret(base,1)
   elif api[at]=='Sleep':ret(0,1)
   else:raise AssertionError(api[at])
  elif at==base+0x5ce42:
   dest,src,n=get(sp+4),get(sp+8),get(sp+12);vm.mem_write(dest,bytes(vm.mem_read(src,n)));ret(dest)
  elif at in (base+0x5be0,base+0x16960,base+0x64a0):penalties.append((at-base,get(sp+4)));ret()
 u.hook_add(UC_HOOK_CODE,hook)
 def call(name,args=(),stdcall=False):
  finish_at[0]=stop;sp=stack+0xe000;u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),stop,*[x&0xffffffff for x in args]));u.reg_write(UC_X86_REG_ESP,sp)
  u.emu_start(dbase+meta.exports[name],stop+1,count=200000)
  assert u.reg_read(UC_X86_REG_EIP)==stop
  assert u.reg_read(UC_X86_REG_ESP)==sp+4+(4*len(args) if stdcall else 0)
  return u.reg_read(UC_X86_REG_EAX)
 def install(ident):
  global ctx
  ctx+=0x100
  p=plugins[ident];addresses={'.inject':code,'.inject_data':ctx,'_dll_exports':{'__dll_death':dbase+meta.exports['SR_Death'],'__dll_cause':dbase+meta.exports['SR_Cause']}}
  for sec in p['sections']:
   if 'rva' in sec:addresses[sec['name']]=base+sec['rva']
  for name,data in Engine(type('Backend',(),{'base':base})())._image(p,addresses,{}).items():u.mem_write(addresses[name],data);u.ctl_remove_cache(addresses[name],addresses[name]+len(data))
  result=call('SR_Configure',[ctx],True);assert result==0,(ident,result,bytes(u.mem_read(ctx,24)).hex())
 def reset_body(body,pid,health=-100):
  u.mem_write(human(body),b'\0'*0x2b64);put(human(body),1);put(human(body)+8,pid);put(human(body)+12,-1);put(human(body)+0x24,3600);put(human(body)+0x28,health)
  if 0<=pid<256:put(player(pid),1);put(player(pid)+0x78,body);u.mem_write(player(pid)+4,b'Player\0')
 def count():return get(base+0xf437318)
 def message():
  at=base+0x93dd880+(count()-1)*0x80;n=get(at+0x20)
  assert n<=63 and get(at)==2 and get(at+0x24)==0 and get(at+0x28)==0xffffffff
  assert bytes(u.mem_read(at+0x40+n,1))==b'\0'
  return bytes(u.mem_read(at+0x40,n)).decode()
 put(base+0x90dc560,2);put(base+0x90dc578,10);put(base+0x90dc548,4);put(base+0x761ea60,1)
 for ident,mode in [('death_messages',1),('instant_death',2),('death_messages_instant',3)]:
  install(ident);checks+=1
  for body,pid in [(0,-1),(1,0),(511,255),(2,256),(3,-7),(4,0x7fffffff)]:
   reset_body(body,pid);before=count();call('SR_Death',[body*0x2b64]);assert count()==before+bool(mode&1);checks+=1
   if mode&1:assert 'morreu' in message();checks+=1
   assert get(human(body)+0x24)==(2699 if mode&2 else 3600);checks+=1
   call('SR_Death',[body*0x2b64]);assert count()==before+bool(mode&1);checks+=1
   if pid<0 or pid>=256:assert get(human(body)+8)==0xffffffff;checks+=1
  # Respawn keeps a new body attached while the old corpse is processed.
  reset_body(5,1);put(player(1)+0x78,7);before=count();call('SR_Death',[5*0x2b64]);assert get(player(1)+0x78)==7 and get(human(5)+8)==0xffffffff;checks+=1
  call('SR_Death',[5*0x2b64]);assert count()==before+bool(mode&1);checks+=1
  # Safe bounded names, literal percent signs, attribution and bleeding delay.
  reset_body(6,2,50);u.mem_write(player(2)+4,b'%n'*12+b'X'*8);put(player(3),1);u.mem_write(player(3)+4,b'Killer\0')
  put(base+0x17529a40,1);put(base+0x1409ba8,3)
  call('SR_Cause',[6*0x2b64,1,0]);call('SR_Death',[6*0x2b64]);put(human(6)+0x28,-10);before=count();call('SR_Death',[6*0x2b64]);assert count()==before+bool(mode&1);checks+=1
  if mode&1:assert message().startswith('Killer matou %n') and 'X' not in message();checks+=1
  # Alive respawn reuses the slot and permits exactly one new message.
  put(human(6)+0x28,100);call('SR_Death',[6*0x2b64]);put(human(6)+0x28,-100);before=count();call('SR_Death',[6*0x2b64]);assert count()==before+bool(mode&1);checks+=1
  # Malformed offsets and sources never read past the pools.
  before=count()
  for offset in (1,512*0x2b64,0xffffffff):call('SR_Death',[offset]);call('SR_Cause',[offset,1,0])
  assert count()==before;checks+=1
  reset_body(8,-1)
  for source in (1,0x5c,0xffffffff):call('SR_Cause',[8*0x2b64,1,source])
  for source in (1,1024*0x8230,0xffffffff):call('SR_Cause',[8*0x2b64,3,source])
  call('SR_Death',[8*0x2b64]);checks+=1
  # Execute every trampoline: exact damage, flags, registers and stack survive.
  regs=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
  for rva,size,reg,damage in [(0x49409,6,UC_X86_REG_EBX,20),(0x16a6f,6,UC_X86_REG_EAX,20),(0x52b92,7,UC_X86_REG_ESI,120),(0x52307,7,UC_X86_REG_ESI,120),(0x53be6,10,UC_X86_REG_ESI,200)]:
   reset_body(9,4,100);expected=[123+i for i in range(len(regs))];expected[regs.index(reg)]=9*0x2b64
   if rva==0x49409:expected[6]=20;expected[4]=0
   if rva==0x16a6f:expected[2]=20
   sp=stack+0xe000;put(sp+12,0xffffffff)
   for rr,v in zip(regs,expected):u.reg_write(rr,v)
   u.reg_write(UC_X86_REG_ESP,sp);finish_at[0]=base+rva+size
   u.emu_start(base+rva,finish_at[0]+1,count=200000)
   assert get(human(9)+0x28)==((100-damage)&0xffffffff);assert [u.reg_read(rr) for rr in regs]==expected;assert u.reg_read(UC_X86_REG_ESP)==sp;checks+=3
  # Run stock death processing after the hook, including native penalties.
  reset_body(10,5);penalties.clear();before=count()
  def stock_death():
   finish_at[0]=base+0x53eb6;u.reg_write(UC_X86_REG_ESI,10*0x2b64);u.reg_write(UC_X86_REG_ESP,stack+0xe000);u.emu_start(base+0x53d37,finish_at[0]+1,count=200000)
   assert u.reg_read(UC_X86_REG_EIP)==finish_at[0]
  stock_death();checks+=1
  if mode&2:
   assert penalties==[(0x5be0,5),(0x16960,5),(0x64a0,5)] and get(player(5)+0x78)==0xffffffff and get(human(10)+8)==0xffffffff;checks+=1
   stock_death();assert len(penalties)==3 and count()==before+bool(mode&1);checks+=1
  else:
   assert not penalties and get(player(5)+0x78)==10;checks+=1
   put(human(10)+0x24,2699);stock_death();assert penalties==[(0x5be0,5),(0x16960,5),(0x64a0,5)] and get(player(5)+0x78)==0xffffffff;checks+=1
   stock_death();assert len(penalties)==3 and count()==before+bool(mode&1);checks+=1
  # All bad owners also survive the original death continuation.
  for bad_owner in (-1,-7,256,0x7fffffff):
   reset_body(10,bad_owner);put(human(10)+0x24,2699);penalties.clear();stock_death();assert not penalties and get(human(10)+8)==0xffffffff;checks+=1
  # A valid owner whose current body differs must keep that new body.
  reset_body(10,5);put(player(5)+0x78,25);put(human(10)+0x24,2699);penalties.clear();stock_death();assert not penalties and get(player(5)+0x78)==25;checks+=1
  # Expired attribution, map resets and active NPC player slots.
  reset_body(12,6,100);call('SR_Cause',[12*0x2b64,1,0]);put(base+0x90dc578,get(base+0x90dc578)+2000);put(human(12)+0x28,-100);call('SR_Death',[12*0x2b64])
  if mode&1:assert 'Killer' not in message();checks+=1
  put(base+0x90dc560,1);call('SR_Death',[12*0x2b64]);put(base+0x90dc560,2);before=count();call('SR_Death',[12*0x2b64]);assert count()==before+bool(mode&1);checks+=1
  reset_body(13,7);put(player(7)+0x1a38,1);call('SR_Death',[13*0x2b64])
  if mode&1:assert message().startswith('um NPC');checks+=1
  # Disable stops notifications and allows configuring another mode.
  assert call('SR_PrepareStop',[ctx],True)==0;assert call('SR_Stop',[ctx],True)==0;before=count();reset_body(11,-1);call('SR_Death',[11*0x2b64]);assert count()==before;checks+=3
print(f'{checks} native death assertions passed at two bases: NPC/player/invalid owners, respawn, bounded names, duplicate suppression, damage hooks, stock events and single native penalties. No live Windows/client test.')
