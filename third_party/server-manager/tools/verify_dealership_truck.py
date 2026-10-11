"""Emulate stock hook and native purchase logic; vehicle/key allocators stubbed."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from core import Engine,parameter_values,SERVER_SHA256
server=Path(sys.argv[1]);assert hashlib.sha256(server.read_bytes()).hexdigest()==SERVER_SHA256
p=json.loads((ROOT/'plugins/dealership_truck.json').read_text());checks=0
for base in (0x400000,0x10000000):
 u=Uc(UC_ARCH_X86,UC_MODE_32);code=0x50000000;data=0x50010000;stack=0x70000000;stop=0x71000000
 for at,size in [(base,0x62000),(code,0x1000),(data,0x1000),(stack,0x10000),(stop,0x1000),(base+0xe2c8000,0x280000),(base+0xac9b000,0x1000),(base+0xededd00&~4095,0x250000),(base+0x9102000,0x4000),(base+0x90dc000,0x1000),(base+0xac50000,0x90000)]:
  # ac9b is contained in the vehicle model table region.
  if at==base+0xac9b000:continue
  u.mem_map(at,size)
 pe=pefile.PE(str(server));pe.relocate_image(base)
 for sec in pe.sections:
  if sec.VirtualAddress<0x62000:u.mem_write(base+sec.VirtualAddress,bytes(sec.get_data()))
 def put(at,value):u.mem_write(at,struct.pack('<I',value&0xffffffff))
 def get(at):return struct.unpack('<I',u.mem_read(at,4))[0]
 def shop(i):return base+0xe2c8bfc+i*0x9dc
 def count(i):return get(shop(i)+0x8c8)
 def stock(i):return [struct.unpack('<3I',u.mem_read(shop(i)+0x8cc+j*12,12)) for j in range(count(i))]
 def seed(i,offers,kind=2):
  put(shop(i),kind);put(shop(i)+0x8c8,len(offers))
  for j,row in enumerate(offers):u.mem_write(shop(i)+0x8cc+j*12,struct.pack('<3I',*row))
 addresses={'.inject':code,'.inject_data':data}
 for s in p['sections']:
  if 'rva' in s:addresses[s['name']]=base+s['rva']
 class Backend:pass
 backend=Backend();backend.base=base
 def install(price=12000):
  for name,image in Engine(backend)._image(p,addresses,parameter_values(p,{'price':price})).items():u.mem_write(addresses[name],image)
 vehicle_calls=[]
 def finish(value=0):
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,get(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
 def hook(vm,at,size,unused):
  sp=vm.reg_read(UC_X86_REG_ESP)
  if at==stop:vm.emu_stop()
  elif at in (base+0xa2a0,base+0xbdb0,base+0x64a0,base+0x65f0,base+0x5ce18):finish()
  elif at==base+0x31800:vehicle_calls.append(get(sp+4));finish(3)
  elif at==base+0x1b070:finish(5)
  elif at==base+0x1b870:finish(1)
  elif at==stop+0x100:finish(9)
 u.hook_add(UC_HOOK_CODE,hook);put(base+0x5e070,stop+0x100);u.mem_write(stop+0x100,b'\xc3')
 def call(at,args=(),preserve=False):
  sp=stack+0xe000;u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),stop,*args));u.reg_write(UC_X86_REG_ESP,sp)
  registers=[UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
  if preserve:registers += [UC_X86_REG_ECX,UC_X86_REG_EDX]
  for n,r in enumerate(registers):u.reg_write(r,0x12340000+n)
  u.emu_start(at,stop+1,count=1000000)
  assert u.reg_read(UC_X86_REG_EIP)==stop
  assert u.reg_read(UC_X86_REG_ESP)==sp+4
  for n,r in enumerate(registers):assert u.reg_read(r)==0x12340000+n
 def tick():call(code,preserve=True)
 install();put(base+0x90dc560,2);put(base+0xe2c8bb8,3)
 seed(0,[]);seed(1,[(0,500,1)]*8);seed(2,[(0,700,2)],kind=7)
 tick();assert stock(0)==[(9,12000,0)];checks+=1
 assert stock(1)[:7]==[(0,500,1)]*7 and stock(1)[7]==(9,12000,0);checks+=1
 assert stock(2)==[(0,700,2)];checks+=1
 before=stock(0);tick();assert stock(0)==before and count(1)==8;checks+=1
 seed(0,[(9,7000,4),(0,1000,3)]);tick();assert stock(0)==[(9,12000,4),(0,1000,3)];checks+=1
 seed(0,[(0,1000,1)]);tick();assert stock(0)==[(0,1000,1),(9,12000,0)];checks+=1
 put(shop(0)+0x8c8,9);before=bytes(u.mem_read(shop(0),0x9dc));tick();assert bytes(u.mem_read(shop(0),0x9dc))==before;checks+=1
 put(shop(0)+0x8c8,0xffffffff);before=bytes(u.mem_read(shop(0),0x9dc));tick();assert bytes(u.mem_read(shop(0),0x9dc))==before;checks+=1
 seed(0,[]);put(base+0xe2c8bb8,1025);tick();assert count(0)==0;checks+=1
 put(base+0xe2c8bb8,0);tick();assert count(0)==0;checks+=1
 put(base+0xe2c8bb8,3);put(base+0x90dc560,1);tick();assert count(0)==0;checks+=1
 put(base+0x90dc560,2);install(17000);tick();assert stock(0)==[(9,17000,0)] and get(base+0xac9b2d8)==17000;checks+=1
 # Actual purchase handler: input offer is 1-based; allocator calls are stubbed.
 buyer=base+0xededd00;put(buyer+0x40,20000);put(buyer+0x50,0);put(buyer+0xd8,0);put(buyer+0x78,0)
 call(base+0x4e0d0,[0,1]);assert vehicle_calls==[9] and get(buyer+0x40)==3000 and count(0)==0;checks+=1
 assert get(base+0x91022c0+5*0x1ac+0x114)==3;checks+=1
 tick();assert stock(0)==[(9,17000,0)];checks+=1
 call(base+0x4e0d0,[0,1]);assert vehicle_calls==[9] and get(buyer+0x40)==3000 and count(0)==1;checks+=1
 # The native catalog refresh may remove offers; next hook restores availability.
 install();call(base+0x5700);tick();assert any(row[0]==9 for row in stock(0)) and count(0)<=8;checks+=1
print(f'{checks} dealership checks passed in two bases; native purchase logic, stock and limits tested. Vehicle/key allocators and network simulated; no live Windows test.')
