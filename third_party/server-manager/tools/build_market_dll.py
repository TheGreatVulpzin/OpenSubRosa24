"""Build the 024d Black Market DLL and manifest with an i686 MinGW compiler."""
import argparse, hashlib, json, struct, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from core import validate_plugin
from dll_support import inspect_dll
from convert_tables import elf

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True,help='Executavel 024d para validar os novos hooks de vida.')
    parser.add_argument('--cc',default='i686-w64-mingw32-gcc')
    parser.add_argument('--combined',action='store_true',default=True,help='Compila o modulo unificado (padrao).')
    args=parser.parse_args();SERVER=args.server
    from core import SERVER_SHA256
    if hashlib.sha256(SERVER.read_bytes()).hexdigest()!=SERVER_SHA256:raise SystemExit('Servidor incompatível.')
    sdk=ROOT/'native_sdk';target=ROOT/('plugins/native/sr_combined.dll' if args.combined else 'plugins/native/sr_black_market.dll')
    definition='combined.def'
    flags=['-shared','-O2','-Wall','-Wextra','-Werror','-fno-builtin','-fno-stack-protector','-nostdlib','-Wl,--entry,_DllMain@12','-Wl,--dynamicbase','-Wl,--nxcompat','-Wl,--no-insert-timestamp','-s']
    if args.combined:flags.append('-DSR_COMBINED')
    build=hashlib.sha256(b''.join((sdk/n).read_bytes() for n in ['black_market.c',definition,'black_market_hook.s']+(['combined_commands.h','extra_commands.h','npc_command.h','immortal.h','immortal_hook.s'] if args.combined else []))+json.dumps(flags).encode()+subprocess.check_output([args.cc,'-dumpfullversion'])).hexdigest()
    subprocess.run([args.cc,*flags,'-DSR_BUILD_ID="'+build+'"',str(sdk/'black_market.c'),str(sdk/definition),'-o',str(target),'-lkernel32'],check=True)
    with tempfile.TemporaryDirectory() as folder:
        obj=Path(folder)/'hook.o'
        subprocess.run(['as','--32',str(sdk/'black_market_hook.s'),'-o',str(obj)],check=True)
        sections,symbols=elf(obj)
    for sec in sections:
        if sec['name']=='.patch_market':sec.update(rva=0x5cc0c,expected='e8affcfeff')
        if sec['name']=='.patch_attack':sec.update(rva=0x5cbfd,expected='e85e44ffff')
        if sec['name']=='.patch_npc':sec.update(rva=0x5cb7e,expected='e8cd6afdff')
    # Header 20, four ints 16, two xyz positions 24, damage/range 8 + debug 4 = 72.
    ctx=struct.pack('<IIIIIiiii6fifi',72,1,0,0,1,125,70,400,200,692,25.725,354,1220,25.725,680,25,1.5,0)
    sections.append({'name':'.inject_data','data':ctx.hex(),'relocations':[{'offset':8,'type':1,'symbol':{'name':'__server','index':0,'value':0}}]})
    parameters={}
    def param(key,label,typ,default,low,high,offset):
        parameters[key]={'label':label,'type':typ,'default':default,'min':low,'max':high,'sites':[{'section':'.inject_data','offset':offset}]}
    param('knife_price','Preço da faca','int',125,0,1000000,20)
    param('bandage_price','Preço da bandagem','int',70,0,1000000,24)
    param('knife_wait','Intervalo após comprar faca (ticks)','int',400,0,36000,28)
    param('bandage_wait','Intervalo após comprar bandagem (ticks)','int',200,0,36000,32)
    for n,point in enumerate([(692,25.725,354),(1220,25.725,680)]):
        for j,axis in enumerate('xyz'):
            param(f'point{n+1}_{axis}',f'Ponto {n+1}: {axis.upper()}','float',point[j],-99999,99999,36+n*12+j*4)
    param('knife_damage','Dano da faca (0 = desativar ataque)','int',25,0,100,60)
    param('knife_range','Alcance da faca','float',1.5,0.1,3,64)
    param('knife_debug','Diagnóstico da faca (0 = não; 1 = sim)','int',0,0,1,68)
    plugin={'format':1,'kind':'dll','id':'native_black_market','name':'Black Market (DLL)','description':'Dois pontos de compra por telefone. Disque 24 para faca ou 12 para bandagem. Preços, posições e dano configuráveis. Ao desativar remove os telefones no próximo tick; itens comprados permanecem.','dll':{'abi':1,'file':'native/sr_black_market.dll','sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'build_id':build,'configure':'SR_Configure','prepare_stop':'SR_PrepareStop','stop':'SR_Stop','identity':'SR_BuildId','symbols':{'__dll_tick':'SR_Tick','__dll_attack':'SR_AttackTick'},'context_section':'.inject_data'},'parameters':parameters,'sections':sections,'imports':{},'symbols':symbols}
    if args.combined:
        plugin.update(id='native_combined',name='Black Market + Chat + Comandos (DLL)',description='Uma ativacao injeta a DLL com mercado, faca, chat no console e comandos admin. Use /admin <senha> e /bmhelp. Nao combine com os modulos separados de mercado/chat.')
        plugin['dll']['file']='native/sr_combined.dll'
        plugin['dll']['symbols']['__dll_chat']='SR_ChatHook'
        plugin['dll']['symbols']['__dll_npc']='SR_NpcTick'
        section=next(s for s in sections if s['name']=='.inject')
        offset=len(bytes.fromhex(section['data']))
        assert offset==80
        section['data']+=(bytes.fromhex('8b44240483ec40e9')+struct.pack('<i',0x49947-4)).hex()
        section['relocations'].append({'offset':offset+8,'type':2,'symbol':{'name':'__server','index':0,'value':0}})
        # Merge the fourth trampoline into the existing executable allocation.
        # Keep the original chat trampoline at offset 80 for ABI compatibility.
        npc=next(s for s in sections if s['name']=='.inject_npc')
        npc_offset=len(bytes.fromhex(section['data']))
        section['data']+=npc['data']
        section['relocations'] += [dict(rel,offset=rel['offset']+npc_offset) for rel in npc['relocations']]
        sections.remove(npc)
        # Append health guards without changing the existing chat/NPC offsets.
        import pefile
        with tempfile.TemporaryDirectory() as folder:
            obj=Path(folder)/'health.o'
            subprocess.run(['as','--32',str(sdk/'immortal_hook.s'),'-o',str(obj)],check=True)
            hs,_=elf(obj)
        health=next(sec for sec in hs if sec['name']=='.inject_health')
        health_offset=len(bytes.fromhex(section['data']))
        section['data']+=health['data']
        section['relocations'] += [dict(rel,offset=rel['offset']+health_offset) for rel in health['relocations']]
        pe=pefile.PE(str(SERVER))
        relocations=[entry.rva for block in pe.DIRECTORY_ENTRY_BASERELOC for entry in block.entries if entry.type==3]
        for sec in hs:
            if sec is health:continue
            rva=int(sec['name'].split('_')[-1],16);size=len(bytes.fromhex(sec['data']))
            sec.update(rva=rva,expected=pe.get_data(rva,size).hex(),expected_relocations=[at-rva for at in relocations if rva<=at and at+4<=rva+size])
            sections.append(sec)
        for sec in sections:
            for rel in sec['relocations']:
                sym=rel['symbol']
                if sym.get('section')=='.inject_health':sym['section']='.inject';sym['value']+=health_offset
        plugin['dll']['symbols']['__dll_health']='SR_LockHealth' 
        for sec in sections:
            for rel in sec['relocations']:
                sym=rel['symbol']
                if sym.get('section')=='.inject_npc':
                    sym['section']='.inject';sym['value']+=npc_offset
        context=next(s for s in sections if s['name']=='.inject_data');raw=bytearray.fromhex(context['data']);struct.pack_into('<I',raw,0,80);raw+=struct.pack('<ii',0,1);context['data']=raw.hex()
        context['relocations'].append({'offset':12,'type':1,'symbol':{'name':'original_chat','index':1,'value':offset,'section':'.inject'}})
        sections.append({'name':'.patch_chat','rva':0x49940,'expected':'8b44240483ec40','data':'e9fcffffff9090','relocations':[{'offset':1,'type':2,'symbol':{'name':'__dll_chat','index':0,'value':0}}]})
        param('show_id','Chat: mostrar ID (0 = nao; 1 = sim)','int',0,0,1,72)
        param('commands_enabled','Comandos admin (0 = nao; 1 = sim)','int',1,0,1,76)
    validate_plugin(plugin);inspect_dll(plugin,ROOT/'plugins')
    (ROOT/('plugins/native_combined.json' if args.combined else 'plugins/native_black_market.json')).write_text(json.dumps(plugin,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Compiled:',target)
if __name__=='__main__':main()
