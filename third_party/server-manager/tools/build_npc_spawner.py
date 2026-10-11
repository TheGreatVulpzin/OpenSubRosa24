"""Rebuild the native pedestrian NPC command module (developer use only)."""
from pathlib import Path
import argparse,subprocess,json,hashlib,sys
from convert_tables import elf
import pefile
ROOT=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('server',type=Path);a=ap.parse_args()
if hashlib.sha256(a.server.read_bytes()).hexdigest()!='11e006786d945d5731cb664fba718a094d8f02648b13fcc20c26c8614ea603fd':raise SystemExit('Servidor incompatível.')
pe=pefile.PE(str(a.server));obj=ROOT/'tools'/'npc_spawner.o'
subprocess.run(['as','--32','-o',str(obj),str(ROOT/'tools'/'npc_spawner.s')],check=True)
sections,symbols=elf(obj)
for s in sections:
    if s['name'].startswith('.patch_'):
        s['rva']=int(s['name'][7:],16);s['expected']=pe.get_data(s['rva'],len(bytes.fromhex(s['data']))).hex();s['expected_relocations']=[]
p={'format':1,'id':'npc_spawner','name':'Spawnar NPC','description':'Cria um NPC humano sem carro a 3 metros à frente do primeiro jogador identificado na lista de conexões. O alvo fica fixo até desativar o módulo. Ative e clique em Spawnar NPCs. O jogador precisa estar vivo e fora de um veículo. Use uma área aberta e plana.','parameters':{'quantity':{'label':'Quantidade por pedido','type':'int','default':1,'min':1,'max':8,'sites':[{'section':'.inject_data','offset':4}]}},'command':{'label':'Spawnar NPCs','quantity_parameter':'quantity'},'sections':sections,'symbols':symbols,'imports':{}}
sys.path.insert(0,str(ROOT));from core import validate_plugin
validate_plugin(p);out=ROOT/'tools'/'npc_spawner-archived.json';out.write_text(json.dumps(p,ensure_ascii=False,indent=2),encoding='utf-8');print(out)
