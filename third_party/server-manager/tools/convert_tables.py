"""Developer-only converter. Requires GNU binutils and pefile; runtime does not."""
from pathlib import Path
import re, json, struct, subprocess, xml.etree.ElementTree as ET, argparse
ROOT=Path(__file__).resolve().parents[1]
DEFINES={}

def expr(s):
    s=s.replace('"subrosadedicated024d.exe"','__server').replace('subrosadedicated024d.exe','__server')
    s=re.sub(r'\(float\)(-?)([A-Za-z_]\w*)',lambda m:'(float)'+m[1]+DEFINES[m[2]],s)
    s=re.sub(r'\(float\)(-?[0-9]+(?:\.[0-9]+)?)',lambda m:'0x'+struct.pack('<f',float(m[1]))[::-1].hex(),s)
    s=re.sub(r'#([0-9]+)',lambda m:'0x'+format(int(m[1]),'x'),s)
    s=re.sub(r'(?<![\w.])([0-9A-F]+)(?![\w.])',lambda m:'0x'+m[1],s)
    return re.sub(r'st\(0x([0-9])\)',r'st(\1)',s.replace('++','+'))

def assembly(script):
    script=script.split('[ENABLE]',1)[0]+'\n'+script.split('[ENABLE]',1)[1].split('[DISABLE]',1)[0]
    script=re.sub(r'\{.*?\}','',script,flags=re.S)
    DEFINES.clear()
    DEFINES.update(re.findall(r'define\((\w+),([^\n]+)\)',script))
    out=['.intel_syntax noprefix','.code32'];patches=[];thread=None
    for line in script.splitlines():
        line=line.split('//')[0].strip()
        if not line:continue
        if line.startswith(('alloc(', 'dealloc(', 'registersymbol(')):continue
        m=re.fullmatch(r'define\((\w+),(.+)\)',line)
        if m:
            if re.fullmatch(r'-?[0-9]+\.[0-9]+',m[2]):continue
            out.append('.equ '+m[1]+','+expr(m[2]));continue
        m=re.fullmatch(r'createThread\((\w+)\)',line,re.I)
        if m:thread=m[1];continue
        m=re.fullmatch(r'newmem(?:\+([0-9A-F]+))?:',line)
        if m:
            out.extend(['.section .inject,"ax"','.org '+str(int(m[1] or '0',16))]);
            if not m[1]:out.append('newmem:')
            continue
        m=re.fullmatch(r'"?subrosadedicated024d.exe"?\+([0-9A-F]+):',line)
        if m:
            offset=int(m[1],16);name='.patch_'+format(offset,'x');patches.append((name,offset));out.append('.section '+name+',"ax"');continue
        if line.endswith(':'):out.append(line);continue
        op,*rest=line.split(None,1);args=rest[0] if rest else ''
        if op=='db':
            m=re.fullmatch(r"'([^']*)'\s+00",args)
            out.append('.asciz '+json.dumps(m[1]) if m else '.byte '+','.join(expr(x) for x in args.split()));continue
        if op=='dd':out.append('.long '+expr(args));continue
        if op=='nop':out.append('.fill '+expr(args or '1')+',1,0x90');continue
        # Symbols used as immediate addresses need OFFSET in GAS Intel syntax.
        if op in ('push','call','jmp') and re.fullmatch(r'[A-Za-z_]\w*',args) and args not in ('eax','ecx','edx','ebx','esp','ebp','esi','edi'):
            if op=='push':args='OFFSET '+args
        args=expr(args)
        if '[' in args and not re.search(r'\b(?:byte|word|dword|qword)\s+ptr\b',args):
            if op in ('fld','fstp','fsub','fsubr','fmul','fadd','fiadd','fistp'):
                args=args.replace('[','DWORD PTR [')
            elif op in ('cmp','mov','add','sub','inc','dec','test'):
                # Most other memory sizes are inferred from a register, except immediates.
                parts=args.split(',',1)
                if len(parts)==1 or (parts[0].strip().startswith('[') and not re.fullmatch(r'e(?:ax|cx|dx|bx|sp|bp|si|di)',parts[1].strip())):
                    args=args.replace('[','DWORD PTR [')
        if op=='fmul' and args=='st(0)':args='st,st(0)'
        if op=='fadd' and args=='st(0)':args='st,st(0)'
        out.append(op+' '+args)
    return '\n'.join(out)+'\n',patches,thread

def elf(path):
    b=path.read_bytes();head=struct.unpack_from('<16sHHIIIIIHHHHHH',b)
    shoff=head[6];shsize=head[11];count=head[12];stridx=head[13]
    sections=[struct.unpack_from('<IIIIIIIIII',b,shoff+i*shsize) for i in range(count)]
    names=sections[stridx];names=b[names[4]:names[4]+names[5]]
    def z(s,o):return s[o:s.find(b'\0',o)].decode()
    secnames=[z(names,s[0]) for s in sections];symbols=[]
    for s in sections:
        if s[1]==2:
            st=sections[s[6]];strings=b[st[4]:st[4]+st[5]]
            for o in range(s[4],s[4]+s[5],s[9]):
                n,v,size,info,other,idx=struct.unpack_from('<IIIBBH',b,o)
                symbols.append({'name':z(strings,n),'value':v,'section':secnames[idx] if idx<len(sections) else 'ABS','index':idx})
    output=[]
    for i,s in enumerate(sections):
        if not secnames[i].startswith(('.inject','.patch_')):continue
        data=b[s[4]:s[4]+s[5]];relocs=[]
        for rs in sections:
            if rs[1]!=9 or rs[7]!=i:continue
            for o in range(rs[4],rs[4]+rs[5],8):
                addr,info=struct.unpack_from('<II',b,o);sym=symbols[info>>8]
                if info&255 not in (1,2):raise ValueError('Unsupported relocation')
                relocs.append({'offset':addr,'type':info&255,'symbol':sym})
        if data:output.append({'name':secnames[i],'data':data.hex(),'relocations':relocs})
    return output,symbols

NAMES={262:'safe_spawn',258:'critical_health',257:'spawn_protection_fix',256:'clunky_ragdolls',208:'aggressive_bots',236:'vehicle_eject',237:'dealership_spawn',238:'dealership_truck',242:'death_messages_instant',243:'instant_death',248:'death_messages',364:'punch',264:'starting_money'}
def main(exe):
    import pefile
    pe=pefile.PE(str(exe));crt={}
    for d in pe.DIRECTORY_ENTRY_IMPORT:
        for i in d.imports:
            if i.name==b'sprintf':crt['sprintf']=i.address-pe.OPTIONAL_HEADER.ImageBase
    for table in (ROOT/'source_tables').glob('*.CT'):
        for entry in ET.parse(table).getroot().iter('CheatEntry'):
            script=entry.findtext('AssemblerScript');ident=int(entry.findtext('ID'))
            if not script:continue
            if ident in (242,243,248):
                # Replaced by the bounded DLL; do not regenerate unsafe hooks.
                # Use build_death_plugins.py to build these three modes.
                continue
            name=NAMES[ident];asm,patches,thread=assembly(script)
            p=ROOT/'tools'/('generated_'+name+'.s');p.write_text(asm)
            obj=p.with_suffix('.o');res=subprocess.run(['as','--32','-o',str(obj),str(p)],capture_output=True,text=True)
            if res.returncode:print(name,res.stderr);continue
            secs,syms=elf(obj);manifest={'format':1,'id':name,'name':entry.findtext('Description').strip('"'),'sections':secs,'imports':crt,'parameters':{},'thread':thread,'symbols':syms}
            for sec in secs:
                if sec['name'].startswith('.patch_'):
                    off=int(sec['name'][7:],16);sec['rva']=off;sec['expected']=pe.get_data(off,len(bytes.fromhex(sec['data']))).hex() if off<0x5f000 else None
            (ROOT/'plugins'/(name+'.json')).write_text(json.dumps(manifest,ensure_ascii=False,indent=2))
            print('OK',name,[(s['name'],len(bytes.fromhex(s['data']))) for s in secs])
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('exe',type=Path);main(ap.parse_args().exe)
