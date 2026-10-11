"""Add release metadata after the developer converter; do not run in the manager."""
from pathlib import Path
import json,struct,pefile,sys,subprocess
ROOT=Path(__file__).resolve().parents[1]
if len(sys.argv)!=2:raise SystemExit('Uso: python finalize_plugins.py CAMINHO_DO_SERVIDOR.exe')
pe=pefile.PE(sys.argv[1])
relocs=[e.rva for b in pe.DIRECTORY_ENTRY_BASERELOC for e in b.entries if e.type==3]

def params(p,key,label,default,kind,lo,hi,values=None,section=None):
    sites=[]
    patterns=values or [(default,1)]
    for s in p['sections']:
        if section and s['name']!=section:continue
        data=bytes.fromhex(s['data'])
        for val,scale in patterns:
            needle=struct.pack('<f' if kind=='float' else '<i',val)
            start=0
            while (at:=data.find(needle,start))!=-1:
                # Parameter constants may not overlap relocations.
                if not any(at<r['offset']+4 and r['offset']<at+4 for r in s.get('relocations',[])):
                    sites.append({'section':s['name'],'offset':at,'scale':scale})
                start=at+4
    assert sites,(p['id'],key)
    p['parameters'][key]={'label':label,'default':default,'type':kind,'min':lo,'max':hi,'sites':sites}
for path in (ROOT/'plugins').glob('*.json'):
    p=json.loads(path.read_text())
    if p['id']=='npc_spawner':continue
    if p['id']=='dealership_truck':
        subprocess.run([sys.executable,str(ROOT/'tools/build_dealership_truck.py'),sys.argv[1]],check=True)
        continue
    p['description']='Conversão do script enviado. Teste em uma partida privada antes de usar no servidor habitual.'
    ident=p['id']
    if ident in ('vehicle_eject','dealership_truck'):
        p={'format':1,'id':ident,'name':p['name'],'description':'Tabela original preservada em source_tables. Ainda não portada para ativação segura.','unavailable':('O script de ejeção usa flags e índices sem validação; precisa ser refeito.' if ident=='vehicle_eject' else 'O script cria uma thread que chama a rotina da concessionária. Precisa ser transferido para o loop do servidor.')}
    else:
        if ident=='clunky_ragdolls':
            s=p['sections'][0];s['data']='eb3a';s['relocations']=[];s['expected']=pe.get_data(s['rva'],2).hex()
        if ident=='starting_money':
            p['name']='Dinheiro inicial';params(p,'money','Dinheiro inicial',2500,'int',0,100000000,section='.patch_e027')
            p['description']='Altera o valor de inicialização usado pela tabela do dinheiro. Não substitui o saldo atual de todos os jogadores.'
        if ident=='punch':
            params(p,'damage','Multiplicador do dano por velocidade',100.0,'float',0,10000,section='.inject')
            params(p,'head_multiplier','Multiplicador na cabeça',5.0,'float',0,100,section='.inject')
            params(p,'knockback','Força do empurrão',8.5,'float',0,50,[(-8.5,-1)],'.inject')
            params(p,'threshold','Limiar usado pelo script',3.0,'float',0,100,[(-3.0,-1)],'.inject')
            params(p,'cooldown','Intervalo em unidades do script',70,'int',0,10000,section='.inject')
            p['name']='Sistema de socos';p['description']='Mantém a fórmula do script: dano por velocidade relativa, bônus na cabeça e empurrão. O limiar é comparado após multiplicar pelo dano, como na tabela original. Desativar interrompe os próximos socos; não reverte dano já causado.'
        if ident=='safe_spawn':
            for key,value in [('max_z',2048.),('min_z',1428.),('max_x',784.),('min_x',608.)]:params(p,key,key,value,'float',0,100000,section='.inject')
        if ident=='spawn_protection_fix':params(p,'velocity_squared','Limiar da velocidade ao quadrado',0.02,'float',0,100,section='.inject')
        if ident=='dealership_spawn':
            for key,value,sec in [('z',485.,'.patch_2561d744'),('x',26.,'.patch_2561d748'),('y',655.,'.patch_2561d74c')]:params(p,key,'Coordenada '+key,value,'float',-100000,100000,section=sec)
        for s in p['sections']:
            if s['name']=='.inject':continue
            if s.get('expected') is not None:
                s['expected_relocations']=[r-s['rva'] for r in relocs if s['rva']<=r and r+4<=s['rva']+len(bytes.fromhex(s['data']))]
    path.write_text(json.dumps(p,ensure_ascii=False,indent=2))
# Administrator values from the third table. Bytes are represented as 4-byte integers
# only when the table also specifies 4 bytes; live byte values get their own type.
for ident,name,rva,default,kind,lo,hi,expected,desc in [
 ('max_players','Máximo de jogadores',0x761eabc,32,'byte',1,32,None,'Muda o limite ativo; não expulsa automaticamente jogadores que já estão conectados.'),
 ('npc_count','Quantidade de NPCs',0x5baea,32,'byte',0,128,pe.get_data(0x5baea,1).hex(),'Altera a constante usada na criação de NPCs. Não cria/remove imediatamente os NPCs existentes.')]:
 p={'format':1,'id':ident,'name':name,'description':desc,'imports':{},'symbols':[],'parameters':{'value':{'label':name,'default':default,'type':kind,'min':lo,'max':hi,'sites':[{'section':'.patch_'+format(rva,'x'),'offset':0,'width':1}]}},'sections':[{'name':'.patch_'+format(rva,'x'),'rva':rva,'data':struct.pack('B',default).hex(),'expected':expected,'relocations':[]}]}
 (ROOT/'plugins'/(ident+'.json')).write_text(json.dumps(p,ensure_ascii=False,indent=2))
