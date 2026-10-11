"""Compile a new GNU Intel x86 module to the manager's JSON format."""
import argparse,subprocess,json,re,tempfile,hashlib
from pathlib import Path
from convert_tables import elf
import pefile
ROOT=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('source',type=Path);ap.add_argument('server',type=Path);ap.add_argument('id');ap.add_argument('name');a=ap.parse_args()
if not re.fullmatch('[a-zA-Z0-9_-]+',a.id):raise SystemExit('ID inválido.')
if hashlib.sha256(a.server.read_bytes()).hexdigest()!='11e006786d945d5731cb664fba718a094d8f02648b13fcc20c26c8614ea603fd':raise SystemExit('Servidor incompatível.')
target=ROOT/'plugins'/(a.id+'.json')
if target.exists():raise SystemExit('O ID já existe. Escolha outro.')
pe=pefile.PE(str(a.server));relocs=[e.rva for b in pe.DIRECTORY_ENTRY_BASERELOC for e in b.entries if e.type==3]
with tempfile.TemporaryDirectory() as t:
    obj=Path(t)/'script.o';subprocess.run(['as','--32','-o',str(obj),str(a.source)],check=True);sections,symbols=elf(obj)
if not sections:raise SystemExit('Não há seções .inject ou .patch_RVA.')
imports={}
for d in pe.DIRECTORY_ENTRY_IMPORT:
    for imp in d.imports:
        if imp.name:imports[imp.name.decode()]=imp.address-pe.OPTIONAL_HEADER.ImageBase
for s in sections:
    if s['name'] in ('.inject','.inject_data'):continue
    off=int(s['name'][7:],16);n=len(bytes.fromhex(s['data']));s['rva']=off
    file_section=pe.get_section_by_rva(off)
    if not file_section or off+n>file_section.VirtualAddress+max(file_section.Misc_VirtualSize,file_section.SizeOfRawData):raise SystemExit('Patch fora da seção PE.')
    on_disk=off-file_section.VirtualAddress+n<=file_section.SizeOfRawData
    s['expected']=pe.get_data(off,n).hex() if on_disk else None
    s['expected_relocations']=[r-off for r in relocs if off<=r and r+4<=off+n]
p={'format':1,'id':a.id,'name':a.name,'description':'Módulo personalizado.','parameters':{},'sections':sections,'symbols':symbols,'imports':imports}
import sys
sys.path.insert(0,str(ROOT));from core import validate_plugin
validate_plugin(p);target.write_text(json.dumps(p,ensure_ascii=False,indent=2),encoding='utf-8');print(target)
