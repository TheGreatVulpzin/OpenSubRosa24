"""Build bounded death notifications and delegate death handling to stock code."""
import argparse,hashlib,json,struct,subprocess,sys,tempfile
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from core import validate_plugin,SERVER_SHA256
from dll_support import inspect_dll
from convert_tables import elf
ap=argparse.ArgumentParser();ap.add_argument('server',type=Path);ap.add_argument('--cc',default='i686-w64-mingw32-gcc');args=ap.parse_args()
assert hashlib.sha256(args.server.read_bytes()).hexdigest()==SERVER_SHA256
sdk=ROOT/'native_sdk';asm=['.intel_syntax noprefix','.code32','.section .inject,"ax"']
sites=[(0x53d37,7,'cmp DWORD PTR [esi+__server+0x15cbfa8],-5','esi',0),
       (0x49409,6,'sub DWORD PTR [ebx+__server+0x15cbfa8],ebp','ebx',1),
       (0x16a6f,6,'sub DWORD PTR [eax+__server+0x15cbfa8],ecx','eax',2),
       (0x52b92,7,'add DWORD PTR [esi+__server+0x15cbfa8],-120','esi',3),
       (0x52307,7,'add DWORD PTR [esi+__server+0x15cbfa8],-120','esi',4),
       (0x53be6,10,'mov DWORD PTR [esi+__server+0x15cbfa8],-100','esi',5)]
regoff={'eax':28,'ebx':16,'esi':4}
for rva,size,replay,reg,kind in sites:
 asm+=[f'hook_{rva:x}:']
 if kind:asm+=[replay]
 asm+=['pushfd','pushad','mov ebp,esp','sub esp,544','and esp,-16','fxsave [esp]','fninit','fldcw [esp]','sub esp,16',f'mov eax,DWORD PTR [ebp+{regoff[reg]}]','mov DWORD PTR [esp],eax']
 if kind:
  asm+=[f'mov DWORD PTR [esp+4],{kind}','mov DWORD PTR [esp+8],-1']
  if kind==1:asm+=['mov eax,DWORD PTR [ebp+4]','mov DWORD PTR [esp+8],eax']
  if kind==3:asm+=['mov eax,DWORD PTR [ebp+48]','mov DWORD PTR [esp+8],eax']
  asm+=['call __dll_cause']
 else:asm+=['call __dll_death']
 asm+=['add esp,16','fxrstor [esp]','mov esp,ebp','popad','popfd']
 if not kind:asm+=[replay]
 asm+=[f'jmp __server+{hex(rva+size)}']
for rva,size,*rest in sites:asm+=[f'.section .patch_{rva:x},"ax"',f'jmp hook_{rva:x}',f'.fill {size-5},1,0x90']
source='\n'.join(asm)+'\n';(sdk/'death_messages_hook.s').write_text(source)
flags=['-shared','-O2','-Wall','-Wextra','-Werror','-fno-builtin','-fno-stack-protector','-nostdlib','-Wl,--entry,_DllMain@12','-Wl,--dynamicbase','-Wl,--nxcompat','-Wl,--no-insert-timestamp','-s']
build=hashlib.sha256((sdk/'death_messages.c').read_bytes()+(sdk/'death_messages.def').read_bytes()+source.encode()+json.dumps(flags).encode()+subprocess.check_output([args.cc,'-dumpfullversion'])).hexdigest()
target=ROOT/'plugins/native/sr_death_messages.dll'
subprocess.run([args.cc,*flags,'-DSR_BUILD_ID="'+build+'"',str(sdk/'death_messages.c'),str(sdk/'death_messages.def'),'-o',str(target),'-lkernel32'],check=True)
pe=pefile.PE(str(args.server));relocs=[e.rva for b in pe.DIRECTORY_ENTRY_BASERELOC for e in b.entries if e.type==3]
with tempfile.TemporaryDirectory() as folder:
 obj=Path(folder)/'hooks.o';subprocess.run(['as','--32',str(sdk/'death_messages_hook.s'),'-o',str(obj)],check=True);sections,symbols=elf(obj)
for sec in sections:
 if sec['name']=='.inject':continue
 rva=int(sec['name'][7:],16);size=len(bytes.fromhex(sec['data']));sec.update(rva=rva,expected=pe.get_data(rva,size).hex(),expected_relocations=[x-rva for x in relocs if rva<=x and x+4<=rva+size])
for ident,name,mode in [('death_messages','Somente mensagens de morte',1),('instant_death','Somente morte instantânea',2),('death_messages_instant','Mensagens de Morte + Morte instantânea',3)]:
 ss=json.loads(json.dumps(sections));ss.append({'name':'.inject_data','data':struct.pack('<6I',24,1,0,0,1,mode).hex(),'relocations':[{'offset':8,'type':1,'symbol':{'name':'__server','value':0,'index':0}}]})
 p={'format':1,'kind':'dll','id':ident,'name':name,'description':'Rotina corrigida: mensagens limitadas a 63 caracteres, índices validados e uma mensagem por morte. O modo instantâneo adianta o processamento nativo da morte. Penalidades e separação do corpo ficam com o jogo. Reinicie o servidor antes de atualizar.','parameters':{},'sections':ss,'symbols':symbols,'imports':{},'dll':{'abi':1,'file':'native/sr_death_messages.dll','sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'build_id':build,'configure':'SR_Configure','prepare_stop':'SR_PrepareStop','stop':'SR_Stop','identity':'SR_BuildId','symbols':{'__dll_death':'SR_Death','__dll_cause':'SR_Cause'},'context_section':'.inject_data'}}
 validate_plugin(p);inspect_dll(p,ROOT/'plugins');(ROOT/'plugins'/f'{ident}.json').write_text(json.dumps(p,ensure_ascii=False,indent=2)+'\n')
print('Built three corrected death modes:',target)
