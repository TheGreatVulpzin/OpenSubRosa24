"""Emulate collision hook plus real stock damage routine (non-exploding cases)."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from core import Engine,parameter_values,SERVER_SHA256
server=Path(sys.argv[1]);assert hashlib.sha256(server.read_bytes()).hexdigest()==SERVER_SHA256
plugin=json.loads((ROOT/'plugins/vehicle_eject.json').read_text());checks=0
for base in (0x400000,0x10000000):
 u=Uc(UC_ARCH_X86,UC_MODE_32);code=0x50000000;stack=0x70000000
 h=base+0x15cbf80;p=base+0xededd00;v=base+0x5592600
 for at,size in [(base,0x62000),(code,0x1000),(stack,0x10000)]:u.mem_map(at,size)
 for at,size in [(h,512*0x2b64),(p,256*0x2408),(v,1024*0x8230)]:
  start=at&~4095;u.mem_map(start,((at-start+size+4095)//4096)*4096)
 pe=pefile.PE(str(server));pe.relocate_image(base)
 for sec in pe.sections:
  if sec.VirtualAddress<0x62000:u.mem_write(base+sec.VirtualAddress,bytes(sec.get_data()))
 def put(at,val):u.mem_write(at,struct.pack('<I',val&0xffffffff))
 def get(at):return struct.unpack('<I',u.mem_read(at,4))[0]
 addresses={'.inject':code,'.patch_57060':base+0x57060}
 images=Engine(type('Backend',(),{'base':base})())._image(plugin,addresses,parameter_values(plugin,{}))
 original=bytes(u.mem_read(base+0x57060,5))
 def install():
  for name,image in images.items():u.mem_write(addresses[name],image);u.ctl_remove_cache(addresses[name],addresses[name]+len(image))
 install()
 # Ensure native receives exactly the same arguments, flags and registers.
 regs=(UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP)
 expected=[];entries=[];stop_native=[False]
 def entry(uc,address,size,ctx):
  if address==base+0x22e20:
   assert [uc.reg_read(r) for r in regs]==expected
   assert uc.reg_read(UC_X86_REG_EFLAGS)&0xcd5==0x246&0xcd5
   sp=uc.reg_read(UC_X86_REG_ESP);entries.append((get(sp+4),get(sp+8)))
   if stop_native[0]:uc.emu_stop()
 u.hook_add(UC_HOOK_CODE,entry)
 occupants=[(0,0,1),(127,255,1),(128,-1,1),(511,256,1),(10,1,1),(11,2,1),(12,3,0),(13,4,1)]
 def run(vehicle,kind=0,active=1,patched=True):
  global checks
  u.mem_write(h,b'\0'*(512*0x2b64));u.mem_write(p,b'\0'*(256*0x2408))
  valid=0<=vehicle<1024
  if valid:put(v+vehicle*0x8230,active);put(v+vehicle*0x8230+4,kind);put(v+vehicle*0x8230+12,1000)
  for body,player,status in occupants:
   at=h+body*0x2b64;put(at,status);put(at+8,player);put(at+0x14,vehicle if body!=13 else 900)
   for bone in range(14):
    for axis in range(3):put(at+0xe0+bone*0xe0+axis*4,0x3f800000+axis)
  for player,body,status in [(0,0,1),(255,127,1),(1,99,1),(2,11,0),(3,12,1),(4,13,1)]:
   put(p+player*0x2408,status);put(p+player*0x2408+0x78,body);put(p+player*0x2408+0x1a44,77)
  before=bytes(u.mem_read(h,512*0x2b64));sp=stack+0xe000;put(sp,vehicle);put(sp+4,10)
  expected[:]=[0x10101010+i for i in range(len(regs))]
  for reg,val in zip(regs,expected):u.reg_write(reg,val)
  u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EFLAGS,0x246);entries.clear()
  if not valid:
   # Invalid arguments cannot arise at the stock call site; stop before the
   # stock routine, which has no bounds checks, to test wrapper rejection.
   stop_native[0]=True
   u.emu_start(base+0x57060,base+0x57065,count=100000)
   stop_native[0]=False
   assert bytes(u.mem_read(h,len(before)))==before;checks+=1;return
  u.emu_start(base+0x57060,base+0x57065,count=100000)
  assert entries==[(vehicle,10)] and u.reg_read(UC_X86_REG_ESP)==sp;checks+=1
  assert get(v+vehicle*0x8230+4)==kind;checks+=1
  assert get(v+vehicle*0x8230+12)==(1000 if kind==12 else 990);checks+=1
  eject=patched and active!=0 and kind!=11
  for body,player,status in occupants:
   at=h+body*0x2b64;changed=eject and status==1 and body!=13
   assert get(at+0x14)==(0xffffffff if changed else (vehicle if body!=13 else 900));checks+=1
   for bone in range(14):
    assert [get(at+0xe0+bone*0xe0+axis*4) for axis in range(3)]==([0,0,0] if changed else [0x3f800000+i for i in range(3)])
   checks+=1
  for player in (0,255,1,2,3,4):
   assert get(p+player*0x2408+0x1a44)==(1 if eject and player in (0,255) else 77);checks+=1
 for args in [(0,0,1),(1023,9,1),(7,11,1),(7,12,1),(7,0,0),(-1,0,1),(1024,0,1)]:run(*args)
 u.mem_write(base+0x57060,original);u.ctl_remove_cache(base+0x57060,base+0x57065)
 run(7,0,1,False)
print(f'{checks} native checks passed at two bases: collision ejection, all 512 body slots, invalid owners, inactive slots, model exemption, original damage/arguments/registers and restoration. No live Windows/client test.')
