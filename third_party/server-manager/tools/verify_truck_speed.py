"""Run the original engine torque function with/without truck-only patches."""
import hashlib,json,math,struct,sys
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from core import Engine,parameter_values,SERVER_SHA256
server=Path(sys.argv[1]);assert hashlib.sha256(server.read_bytes()).hexdigest()==SERVER_SHA256
p=json.loads((ROOT/'plugins/truck_speed.json').read_text());checks=0
for base in (0x400000,0x10000000):
 u=Uc(UC_ARCH_X86,UC_MODE_32);code=0x50000000;data=0x50010000;stack=0x70000000;stop=0x71000000;vehicle=base+0x5592600
 for at,size in [(base,0x62000),(code,0x1000),(data,0x1000),(stack,0x10000),(stop,0x1000),(base+0x5592000,0x20000)]:u.mem_map(at,size)
 pe=pefile.PE(str(server));pe.relocate_image(base)
 for sec in pe.sections:
  if sec.VirtualAddress<0x62000:u.mem_write(base+sec.VirtualAddress,bytes(sec.get_data()))
 def write(at,value):
  u.mem_write(at,value);u.ctl_remove_cache(at,at+len(value))
 def put(at,value):u.mem_write(at,struct.pack('<I',value&0xffffffff))
 def fput(at,value):u.mem_write(at,struct.pack('<f',value))
 addresses={'.inject':code,'.inject_data':data}
 for s in p['sections']:
  if 'rva' in s:addresses[s['name']]=base+s['rva']
 class Backend:pass
 b=Backend();b.base=base
 images=Engine(b)._image(p,addresses,parameter_values(p,{}))
 originals={s['rva']:bytes(u.mem_read(base+s['rva'],6)) for s in p['sections'] if 'rva' in s}
 def install(speed=2,power=2):
  for name,image in Engine(b)._image(p,addresses,parameter_values(p,{'speed':speed,'power':power})).items():write(addresses[name],image)
 def restore():
  for rva,image in originals.items():write(base+rva,image)
 # Native speed cutoff is derived directly from the torque helper's constants.
 seconds=struct.unpack('<d',u.mem_read(base+0x5e6f8,8))[0]
 divisor=struct.unpack('<d',u.mem_read(base+0x5e2f0,8))[0]*struct.unpack('<d',u.mem_read(base+0x5e9a0,8))[0]
 cutoff=divisor/(seconds*seconds)
 assert cutoff>0 and math.isfinite(cutoff)
 def run(at,kind,rpm,torque_input=None):
  put(vehicle+4,kind);fput(vehicle+0x5e50,rpm);fput(vehicle+0x5e54,.75)
  probe=code+0x400;result=data+0x800
  raw=b'\xdb\xe3' # fninit
  if torque_input is None:
   raw+=b'\x6a\x00'
   raw+=b'\xe8'+struct.pack('<i',at-(probe+len(raw)+5))
   raw+=b'\x83\xc4\x04'
  else:
   fput(data+0x804,torque_input)
   raw+=b'\xd9\x05'+struct.pack('<I',data+0x804)+b'\x31\xd2'
   raw+=b'\xe8'+struct.pack('<i',at-(probe+len(raw)+5))
  raw+=b'\xd9\x1d'+struct.pack('<I',result)+b'\xc3'
  write(probe,raw);sp=stack+0xe000;put(sp,stop);u.reg_write(UC_X86_REG_ESP,sp)
  u.emu_start(probe,stop,count=100000)
  assert u.reg_read(UC_X86_REG_ESP)==sp+4
  return struct.unpack('<f',u.mem_read(result,4))[0]
 # Execute native throttle multiply then stop before the rest of vehicle sim.
 u.mem_write(base+0x325c1,b'\xc3')
 baseline={}
 for kind in (0,7,9):
  for fraction in (0,.1,.25,.5,.8,1,1.2,1.6,2):
   baseline[kind,fraction]=run(base+0x23fa0,kind,cutoff*fraction)
 assert baseline[9,.5]>0 and baseline[9,1.2]==0
 for speed in (1,2,3,4):
  install(speed,2)
  for fraction in (.1,.5,.8,1.2,1.6,2):
   boosted=run(base+0x23fa0,9,cutoff*fraction)
   restore();expected=run(base+0x23fa0,9,cutoff*fraction/speed);install(speed,2)
   assert math.isclose(boosted,expected,rel_tol=1e-5,abs_tol=1e-6),(speed,fraction,boosted,expected);checks+=1
 install(2,2)
 assert run(base+0x23fa0,9,cutoff*1.2)>0;checks+=1
 for kind in (0,7):
  for fraction in (.1,.5,.8,1.2,1.6):
   assert run(base+0x23fa0,kind,cutoff*fraction)==baseline[kind,fraction];checks+=1
 for power in (1,2,3,4):
  install(2,power)
  for kind in (0,7,9):
   result=run(base+0x325bb,kind,0,12)
   assert math.isclose(result,9*(power if kind==9 else 1),rel_tol=1e-6);checks+=1
 restore();assert run(base+0x23fa0,9,cutoff*1.2)==0;checks+=1
 assert run(base+0x325bb,9,0,12)==9;checks+=1
print(f'{checks} engine checks passed at two bases. Native torque curve, truck-only RPM range, force multiplier and restoration tested; no live speed measurement.')
