"""Independent relocation check against GNU ld. Developer verification only."""
from pathlib import Path
import tempfile,subprocess,sys,struct
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from core import Engine,load_plugins,parameter_values
from convert_tables import elf
ROOT=Path(__file__).resolve().parents[1]
plugins,errors=load_plugins(ROOT/'plugins');assert not errors
class Backend:
    base=0x400000
    def pointer(self,a):return 0x70001234
checked=0
with tempfile.TemporaryDirectory() as t:
    for base in (0x400000,0x10000000):
        b=Backend();b.base=base;engine=Engine(b)
        for ident,p in plugins.items():
            if p.get('unavailable') or p.get('kind')=='dll' or ident in ('max_players','npc_count','clunky_ragdolls'):continue
            obj=ROOT/'tools'/('npc_spawner.o' if ident=='npc_spawner' else 'generated_'+ident+'.o');addresses={s['name']:(0x50000000 if s['name']=='.inject' else (0x50010000 if s['name']=='.inject_data' else base+s['rva'])) for s in p['sections']}
            out=Path(t)/'linked.elf'
            cmd=['ld','-m','elf_i386','-e','0','--defsym',f'__server={base}','--defsym','sprintf=0x70001234','-o',str(out),str(obj)]
            for name,address in addresses.items():cmd+=['--section-start',name+'='+hex(address)]
            subprocess.run(cmd,check=True,capture_output=True)
            images=engine._image(p,addresses,parameter_values(p,{}))
            for sec in p['sections']:
                raw=Path(t)/'section.bin';subprocess.run(['objcopy','--dump-section',sec['name']+'='+str(raw),str(out)],check=True,capture_output=True)
                assert raw.read_bytes()==images[sec['name']],(ident,hex(base),sec['name'])
                checked+=1
print(f'{checked} section images agree with GNU ld at both server base addresses.')
