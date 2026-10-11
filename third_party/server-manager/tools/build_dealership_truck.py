"""Build a dealership stock plugin; allocations run on the server thread."""
import hashlib,json,struct,subprocess,sys,tempfile
from pathlib import Path
import pefile
from convert_tables import elf
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from core import SERVER_SHA256,validate_plugin
server=Path(sys.argv[1])
assert hashlib.sha256(server.read_bytes()).hexdigest()==SERVER_SHA256
pe=pefile.PE(str(server))
with tempfile.TemporaryDirectory() as td:
 obj=Path(td)/'truck.o'
 subprocess.run(['as','--32',str(ROOT/'tools/dealership_truck.s'),'-o',str(obj)],check=True)
 sections,symbols=elf(obj)
for s in sections:
 if s['name'] not in ('.inject','.inject_data'):
  s['rva']=int(s['name'][7:],16)
  s['expected']=pe.get_data(s['rva'],len(bytes.fromhex(s['data']))).hex() if s['rva']==0x5cbd2 else None
  s['expected_relocations']=[]
plugin={'format':1,'id':'dealership_truck','name':'Caminhão na concessionária','description':'Mantém um caminhão no catálogo de cada concessionária durante a partida, usando o loop do servidor. Preço configurável (padrão $12000). Se o catálogo estiver cheio, substitui a última oferta. Não cria veículo físico até a compra. Desativar interrompe a reposição; ofertas e veículos já criados permanecem até a atualização normal do jogo.','parameters':{'price':{'label':'Preço do caminhão','type':'int','default':12000,'min':0,'max':1000000,'sites':[{'section':'.inject_data','offset':0},{'section':'.patch_ac9b2d8','offset':0}]}},'sections':sections,'symbols':symbols,'imports':{}}
validate_plugin(plugin)
(ROOT/'plugins/dealership_truck.json').write_text(json.dumps(plugin,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Built dealership_truck.json')
