"""Emulate compiled DLL + actual 024d CreateItem/DeleteItem at two image bases.
Windows, messages, replication and detach are stubbed. Initial unit checks use
simulated hits; regression checks execute the actual human collision routine. Not a live
client/game test. Developer dependencies: pefile, unicorn; supply server exe.
"""
import hashlib,struct,sys
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from core import Engine,SERVER_SHA256,load_plugins,parameter_values
from dll_support import inspect_dll

def verify(path, combined=True):
    assert hashlib.sha256(path.read_bytes()).hexdigest()==SERVER_SHA256
    plugins,errors=load_plugins(ROOT/'plugins');assert not errors
    p=plugins['native_combined'];dllpath,meta=inspect_dll(p);checks=0
    for base,dbase in [(0x400000,0x68000000),(0x10000000,0x71000000)]:
        u=Uc(UC_ARCH_X86,UC_MODE_32);api={};messages=[];targets=[];original_calls=[];intersections=[];hit=[False];real_hit=[False];sleep_mode=[False];debug_lines=[];human_calls=[]
        ctx=0x60000000;code=0x60010000;stack=0x7f000000;stop=0x7e000000
        for at,size in [(ctx,0x1000),(code,0x1000),(stack,0x10000),(stop,0x4000),(base,0x61000),(base+0x90dc000,0x1000),(base+0x9102000,0x6c000),(base+0xeded000,0x242000),(base+0x15cb000,0x580000),(base+0x10348000,0x40000),(base+0x761e000,0x1000),(base+0xf437000,0x1000),(base+0x93dd000,0x10000),(base+0xf270000,0x2000),(base+0x17179000,0x20000)]:u.mem_map(at,size)
        s=pefile.PE(str(path));s.relocate_image(base)
        for sec in s.sections:
            if sec.VirtualAddress<0x61000:u.mem_write(base+sec.VirtualAddress,bytes(sec.get_data()))
        d=pefile.PE(str(dllpath));d.relocate_image(dbase);u.mem_map(dbase,meta.image_size);u.mem_write(dbase,d.get_memory_mapped_image())
        for desc in d.DIRECTORY_ENTRY_IMPORT:
            assert desc.dll==b'KERNEL32.dll'
            for entry in desc.imports:
                at=stop+0x1000+len(api)*0x20;api[at]=entry.name.decode();u.mem_write(entry.address,struct.pack('<I',at));u.mem_write(at,b'\x90')
        def put(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
        def get(at):return struct.unpack('<I',u.mem_read(at,4))[0]
        def floats(at,v):u.mem_write(at,struct.pack('<'+'f'*len(v),*v))
        items=base+0x91022c0;humans=base+0x15cbf80;players=base+0xededd00
        def item(i):return items+0x1ac*i
        def human(i):return humans+0x2b64*i
        def player(i):return players+0x2408*i
        def active_items():return [i for i in range(1024) if get(item(i))==1]
        addresses={'.inject':code,'.inject_data':ctx,'.patch_market':base+0x5cc0c,'.patch_attack':base+0x5cbfd,'_dll_exports':{'__dll_tick':dbase+meta.exports['SR_Tick'],'__dll_attack':dbase+meta.exports['SR_AttackTick']}}
        if combined:
            addresses['.patch_npc']=base+0x5cb7e
            addresses['_dll_exports']['__dll_npc']=dbase+meta.exports['SR_NpcTick']
            addresses['.patch_chat']=base+0x49940
            addresses['_dll_exports']['__dll_chat']=dbase+meta.exports['SR_ChatHook']
        addresses['_dll_exports']['__dll_health']=dbase+meta.exports['SR_LockHealth']
        for section in p['sections']:
            if 'rva' in section:addresses[section['name']]=base+section['rva']
        class Backend:pass
        b=Backend();b.base=base;e=Engine(b)
        def install(values={}):
            for name,data in e._image(p,addresses,parameter_values(p,values)).items():
                u.mem_write(addresses[name],data);u.ctl_remove_cache(addresses[name],addresses[name]+len(data))
        def finish(result=0,n=0):
            sp=u.reg_read(UC_X86_REG_ESP);ret=get(sp);u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+4+n*4);u.reg_write(UC_X86_REG_EIP,ret)
        def on_code(vm,at,size,unused):
            sp=vm.reg_read(UC_X86_REG_ESP)
            if at==stop:vm.emu_stop();return
            if at in api:
                if api[at]=='GetModuleHandleW':finish(base,1)
                elif api[at]=='Sleep':
                    if sleep_mode[0]:
                        # Execute one real game callback while PrepareStop waits.
                        vm.reg_write(UC_X86_REG_EIP,code+0x800)
                    else:finish(0,1)
                elif api[at]=='GetStdHandle':finish(42,1)
                elif api[at]=='CreateFileW':finish(42,7)
                elif api[at]=='CloseHandle':finish(1,1)
                elif api[at]=='WriteFile':
                    line=bytes(vm.mem_read(get(sp+8),get(sp+12))).decode();debug_lines.append(line);
                    if line.strip() and (not messages or messages[-1]!=line.strip()):messages.append(line.strip())
                    put(get(sp+16),get(sp+12));finish(1,5)
                else:raise AssertionError(api[at])
            elif at==stop+0x1800:finish(0) # deterministic native allocator RNG
            elif at==base+0x5ce18:finish() # stack cookie check
            elif at==base+0x5ce7c: # CRT memset imported via wrapper
                dest,value,length=get(sp+4),get(sp+8),get(sp+12);vm.mem_write(dest,bytes([value&255])*length);finish(dest)
            elif at==base+0x31630:
                map_rays.append((struct.unpack('<3f',vm.mem_read(get(sp+4),12)),struct.unpack('<3f',vm.mem_read(get(sp+8),12))))
                floats(base+0x103484a0,[100,25,200]);finish(1 if map_hit[0] else 0)
            elif at==base+0x49947:original_calls.append('chat');vm.reg_write(UC_X86_REG_ESP,sp+64);finish(42)
            elif at in (base+0x31800,base+0x28190,base+0x64a0,base+0x18720):
                native_commands.append((at-base,[get(sp+4+4*i) for i in range(5)]));finish(3 if at==base+0x31800 else 0)
            elif at==base+0x6380:finish() # Client replication event.
            elif at in (base+0x6260,base+0x626d):
                ptr=get(sp+8);raw=bytearray()
                while vm.mem_read(ptr+len(raw),1)!=b'\0':raw+=vm.mem_read(ptr+len(raw),1)
                assert len(raw)<=63;messages.append(raw.decode());targets.append((get(sp+4),get(sp+12),raw.decode()));finish()
            elif at==base+0x7e50: # Held item detach: simulated relationship update.
                iid=get(sp+4);hid=get(item(iid)+0x18)
                if hid<512:put(human(hid)+0x291c,0)
                put(item(iid)+0x18,-1);finish()
            elif at==base+0x51060:human_calls.append(get(human(1)+0x28));finish()
            elif at==base+0x4c8c0:original_calls.append(True);finish()
            elif at==base+0x33650:original_calls.append('npc-caches');finish()
            elif at==base+0x2f130:
                intersections.append(get(sp+4))
                if not real_hit[0]:finish(1 if hit[0] else 0)
        # Native body allocator's CRT math helpers use x87 arguments.
        put(base+0x5e070,stop+0x1800)
        u.mem_write(stop+0x1800,b'\xc3')
        for index,(offset,instructions) in enumerate([(0x5e088,b'\xd9\xff'),(0x5e08c,b'\xd9\xfe'),(0x5e090,b'\xd9\xfa'),(0x5e094,b'\xd9\xf3')]):
            at=stop+0x1820+index*0x10;put(base+offset,at);u.mem_write(at,instructions+b'\xc3')
        for bone in range(16):floats(base+0xf270280+bone*64+0x28,[1,1])
        # CRT _CIsqrt takes/returns ST(0); replace only that math import.
        u.mem_write(base+0x5cf48,b'\xd9\xfa\xc3')
        u.mem_write(code+0x800,b'\xe8'+struct.pack('<i',-0x805)+b'\xc2\x04\x00')
        u.hook_add(UC_HOOK_CODE,on_code)
        def call(at,args=(),stdcall=False,preserve=False):
            sp=stack+0xe000;u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),stop,*args));u.reg_write(UC_X86_REG_ESP,sp)
            regs=[UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
            if preserve:regs += [UC_X86_REG_ECX,UC_X86_REG_EDX]
            for i,r in enumerate(regs):u.reg_write(r,0x11111111+i)
            u.emu_start(at,stop+1,count=3000000)
            assert u.reg_read(UC_X86_REG_EIP)==stop,hex(u.reg_read(UC_X86_REG_EIP))
            assert u.reg_read(UC_X86_REG_ESP)==sp+4+(len(args)*4 if stdcall else 0)
            result=u.reg_read(UC_X86_REG_EAX)
            for i,r in enumerate(regs):assert u.reg_read(r)==0x11111111+i,(r,hex(u.reg_read(r)))
            return result
        def export(name):return dbase+meta.exports[name]
        def tick(v):
            put(base+0x90dc578,v);call(code+40,preserve=True);call(code,preserve=True)
        # Supply nonzero native item mass/inertias.
        for typ in (12,23,24):floats(base+0x10357404+typ*0x198,[1]*32)
        native_commands=[];map_hit=[True];map_rays=[]
        install();put(base+0x90dc560,2);put(base+0x761ea60,1)
        assert call(export('SR_Configure'),[ctx],True)==0;checks+=1
        tick(1);assert active_items()==[0,1] and len(original_calls)==1;checks+=1
        # Caller has two live x87 values: the wrapper must retain full FP state.
        probe=code+0x900
        raw=b'\xdb\xe3\xd9\xe8\xd9\xeb'  # fninit; fld1; fldpi
        raw+=b'\xb8'+struct.pack('<I',ctx+0x200)+b'\x0f\xae\x00'
        raw+=b'\xe8'+struct.pack('<i',code-(probe+len(raw)+5))
        raw+=b'\xb8'+struct.pack('<I',ctx+0x400)+b'\x0f\xae\x00\xdd\xd8\xdd\xd8\xc3'
        u.mem_write(probe,raw);call(probe)
        before=bytes(u.mem_read(ctx+0x200,160));after=bytes(u.mem_read(ctx+0x400,160))
        # Unicorn retains its last executed x87 instruction address in FIP;
        # compare control/status/tags and every saved x87 value instead.
        assert before[:8]+before[12:]==after[:8]+after[12:];checks+=1
        assert get(item(0)+0xc)==23 and get(item(0)+0x38)==3;checks+=1
        assert struct.unpack('<3f',u.mem_read(item(0)+0x48,12))==struct.unpack('<3f',struct.pack('<3f',692,25.725,354));checks+=1
        # First buyer, near first point.
        put(human(0),1);put(human(0)+8,0);put(human(0)+0x28,100);put(human(0)+0x14,-1)
        floats(human(0)+0x48,[692,25.725,354]);put(player(0),1);put(player(0)+0x78,0);put(player(0)+0x40,1000);put(item(0)+0x18,0)
        tick(2);assert 'Faca: $125 (24) / Bandagem: $70 (12)' in messages;checks+=1
        put(item(0)+0x108,24);tick(3)
        assert get(player(0)+0x40)==875 and get(item(2)+0xc)==24 and get(item(2)+0x38)==1 and get(item(0)+0x108)==0;checks+=1
        put(item(0)+0x108,12);tick(4);assert get(player(0)+0x40)==875 and active_items()==[0,1,2];checks+=1
        put(item(0)+0x108,12);tick(403);assert get(player(0)+0x40)==805 and get(item(3)+0xc)==12 and get(item(3)+0xe0)==4;checks+=1
        put(player(0)+0x40,1);put(item(0)+0x108,24);tick(603);assert get(player(0)+0x40)==1 and len(active_items())==4;checks+=1
        put(player(0)+0x40,1000)
        for i in range(4,1024):put(item(i),1)
        put(item(0)+0x108,24);tick(604);assert get(player(0)+0x40)==1000;checks+=1
        assert messages[-1]=='Sem espaco para criar o item. Nada foi cobrado.';checks+=1
        for i in range(4,1024):put(item(i),0)
        put(item(0)+0x108,24);put(item(0)+0x18,512);tick(605);assert get(player(0)+0x40)==1000;checks+=1
        put(item(0)+0x18,0);floats(human(0)+0x48,[0,0,0]);tick(606);assert get(player(0)+0x40)==1000;checks+=1
        # Simulate a knife swing and engine ray hit; protected target untouched.
        put(item(2)+0x18,0);put(human(0)+0x291c,1);put(human(0)+0x2920,2);put(human(0)+0xbc,1);floats(human(0)+0x2d0,[1,0,0]);floats(item(2)+0x9c,[0,0,1])
        put(human(1),1);put(human(1)+8,-1);put(human(1)+0x28,100);hit[0]=True
        tick(607);assert get(human(1)+0x28)==75;checks+=1
        assert get(base+0xf437318)==1 and get(base+0x93dd880)==1 and get(base+0x93dd8a0)==2 and get(base+0x93dd8a4)==1;checks+=1
        tick(608);assert get(human(1)+0x28)==75;checks+=1
        put(human(1)+0x34,1);tick(627);assert get(human(1)+0x28)==75;checks+=1
        put(human(1)+0x34,0);put(human(1)+8,1);put(player(1),1);put(player(1)+0x44,1);tick(647);assert get(human(1)+0x28)==75;checks+=1
        # Real 024d collision regression: camera/head basis points +Z, but
        # blade's cutting direction is local -Z, as in the original DLL.
        real_hit[0]=True;put(player(1)+0x44,0);put(human(1)+8,-1)
        floats(item(2)+0x48,[10,20,30]);floats(item(2)+0x9c,[0,0,1])
        floats(human(0)+0x2d0,[0,0,1]);put(human(1)+0x28,100)
        def victim(pos):
            for bone in range(15):floats(human(1)+0xc8+bone*0xe0,pos)
        victim([10,20,29]);tick(667)
        assert get(human(1)+0x28)==75,'real front collision did not damage target';checks+=1
        victim([10,20,31]);tick(687)
        assert get(human(1)+0x28)==75,'hit a target behind the knife';checks+=1
        victim([12,20,29]);tick(707)
        assert get(human(1)+0x28)==75,'hit outside blade ray';checks+=1
        victim([10,20,27]);tick(727)
        assert get(human(1)+0x28)==75,'hit outside configured reach';checks+=1
        victim([10,20,29]);put(human(0)+0xbc,0);tick(747)
        assert get(human(1)+0x28)==75,'damage without attack input';checks+=1
        put(human(0)+0xbc,1);floats(item(2)+0x9c,[0,0,0]);tick(767)
        assert get(human(1)+0x28)==75,'invalid blade basis caused damage';checks+=1
        # Non-unit item orientation works; head matrix is deliberately zero.
        floats(item(2)+0x9c,[0,0,0.7]);floats(human(0)+0x2d0,[0,0,0]);tick(787)
        assert get(human(1)+0x28)==50,'scaled blade basis rejected';checks+=1
        victim([9,20,30]);floats(item(2)+0x9c,[1,0,0]);tick(807)
        assert get(human(1)+0x28)==25,'rotated blade did not hit';checks+=1
        tick(808);assert get(human(1)+0x28)==25,'attack cooldown bypassed';checks+=1
        put(human(1)+0x34,10);tick(827)
        assert get(human(1)+0x28)==25,'damaged spawn-protected target';checks+=1
        put(human(1)+0x34,0);put(human(1)+8,1);put(player(1)+0x44,1);tick(847)
        assert get(human(1)+0x28)==25,'damaged invulnerable target';checks+=1
        put(player(1)+0x44,0);put(human(1)+8,-1);tick(867)
        assert get(human(1)+0x28)==0 and get(human(1)+0x2b1c)==1,'lethal hit not applied';checks+=1
        put(human(1)+0x28,100);put(ctx+60,0);tick(887)
        assert get(human(1)+0x28)==100,'damage zero did not disable knife';checks+=1
        put(ctx+60,25);put(human(0)+0x2920,1024);tick(907)
        assert get(human(1)+0x28)==100,'invalid weapon ID';checks+=1
        put(human(0)+0x2920,2);put(item(2)+0xc,12);tick(927)
        assert get(human(1)+0x28)==100,'ordinary item treated as knife';checks+=1
        put(item(2)+0xc,24);put(item(2)+0x18,1);tick(947)
        assert get(human(1)+0x28)==100,'another human owns this knife';checks+=1
        put(item(2)+0x18,0)
        # 0.5.2 regression: native pickup into hand slot 1; hand0 is empty.
        put(human(0)+0x291c,0);put(item(2)+0x18,-1)
        assert call(base+0x1b870,[2,0xffffffff,0,1])==1;checks+=1
        assert get(human(0)+0x2940)==1 and get(human(0)+0x2944)==2 and get(item(2)+0x20)==1;checks+=1
        victim([9,20,30]);put(human(0)+0xbc,0x11);put(human(1)+0x28,100)
        put(ctx+68,1);tick(967)
        assert get(human(1)+0x28)==75 and human_calls[-1]==75,'second-hand damage not visible before human update';checks+=1
        assert any('hand=1' in line and 'damage-applied' in line for line in debug_lines);checks+=1
        put(human(0)+0xbc,1);tick(987)
        assert get(human(1)+0x28)==75,'attacked with inactive hand';checks+=1
        put(human(0)+0xbc,0x12);tick(1027)
        assert get(human(1)+0x28)==75 and 'use-left-click' in debug_lines[-1];checks+=1
        # Pocket slot 2 must not act as a hand.
        put(human(0)+0x2940,0);put(item(2)+0x18,-1)
        assert call(base+0x1b870,[2,0xffffffff,0,2])==1;checks+=1
        put(human(0)+0xbc,0x11);tick(1087)
        assert get(human(1)+0x28)==75 and 'no-active-hand-item' in debug_lines[-1];checks+=1
        put(human(0)+0x2964,0);put(item(2)+0x18,-1)
        assert call(base+0x1b870,[2,0xffffffff,0,0])==1;checks+=1
        put(human(0)+0xbc,1);tick(1147)
        assert get(human(1)+0x28)==50 and 'damage-applied' in debug_lines[-1];checks+=1
        put(ctx+68,0)
        # Both callbacks share reset handling: don't erase attack cooldown twice.
        put(human(1)+0x28,100);tick(1)
        assert get(human(1)+0x28)==75,'new round did not reset attack timer';checks+=1
        tick(2);assert get(human(1)+0x28)==75,'post callback reset cooldown a second time';checks+=1
        real_hit[0]=False;hit[0]=False;put(human(0)+0xbc,0)
        if combined:
            u.mem_write(player(0)+4,b'Admin\0')
            def chat(msg,pid=0):
                u.mem_write(ctx+0x800,msg.encode()+b'\0')
                return call(base+0x49940,[pid,ctx+0x800])
            put(human(0)+0x28,25);put(player(0)+0x2c,0)
            assert chat('/heal')==1 and get(human(0)+0x28)==25 and 'requer admin' in messages[-1];checks+=1
            assert chat('ola')==42 and 'Admin: ola' in debug_lines[-1];checks+=1
            # Native /admin remains forwarded to the server's original handler.
            assert chat('/admin senha')==42;checks+=1
            put(player(0)+0x2c,1)
            assert chat('/heal')==1 and get(human(0)+0x28)==100;checks+=1
            chat('/kill');assert get(human(0)+0x28)==0xfffffffb;checks+=1
            chat('/heal');put(human(0)+0x291c,0);put(item(2)+0x18,-1)
            before=len(active_items());chat('/item 24');assert len(active_items())==before+1 and get(human(0)+0x291c)==1;checks+=1
            before=len(active_items());chat('/item 999999');assert len(active_items())==before and 'Uso:' in messages[-1];checks+=1
            chat('/item 2garbage');assert len(active_items())==before;checks+=1
            chat('/vehicle 2');assert native_commands[-1][0]==0x31800 and native_commands[-1][1][0]==2;checks+=1
            chat('/vehicle -1');assert 'Uso:' in messages[-1];checks+=1
            chat('/arm 3');assert any(a==0x28190 and args[:3]==[0,6,4] for a,args in native_commands);checks+=1
            for bone in range(15):floats(human(0)+0x154+bone*0xe0,[1])
            chat('/heavydude');assert all(struct.unpack('<f',u.mem_read(human(0)+0x154+bone*0xe0,4))[0]==100000 for bone in range(15));checks+=1
            chat('/team 2');assert get(player(0)+0x64)==2 and native_commands[-1][0]==0x64a0;checks+=1
            put(player(1),1);u.mem_write(player(1)+4,b'Other\0');chat('/deactivate 1');assert get(player(1))==0;chat('/deactivate 1');assert get(player(1))==1;checks+=1
            chat('/deactivate 256');assert 'Uso:' in messages[-1];checks+=1
            hit[0]=True;floats(base+0x103484b8,[0.5]);chat('/delh');assert native_commands[-1][0]==0x18720;checks+=1
            hit[0]=False;chat('/delh');assert 'Nenhum' in messages[-1];checks+=1
            chat('/bmhelp');assert targets[-1][1]==get(player(0)+0x78);checks+=1
            chat('/bmhelp');assert any('/turn conta' in line for line in messages[-4:]) and '/give admin ID' in messages[-1] and '/imortal' in messages[-1];checks+=1
            put(ctx+76,0);put(human(0)+0x28,33);assert chat('/heal')==42 and get(human(0)+0x28)==33;checks+=1
            put(ctx+76,1);put(ctx+72,1);chat('teste');assert '[0] Admin: teste' in debug_lines[-1];checks+=1
            # Extra commands converted from comandinhos.CT.
            put(human(0)+0x28,100);put(human(0)+0x14,-1)
            account=base+0xf437b68;put(base+0xf437b64,2)
            u.mem_write(account+0x4c+8,b'Copied name'+b'\0'*21)
            put(account+0x4c+0x34,2);put(account+0x4c+0x38,7);put(account+0x4c+0x3c,3)
            put(player(0)+0x24,123456);put(player(0)+0x28,654321)
            money=get(player(0)+0x40);chat('/turn 1')
            assert bytes(u.mem_read(player(0)+4,12))==b'Copied name\0';checks+=1
            assert [get(player(0)+off) for off in (0x23b4,0x23b8,0x23bc)]==[2,7,3];checks+=1
            assert get(player(0)+0x24)==123456 and get(player(0)+0x28)==654321 and get(player(0)+0x40)==money;checks+=1
            before=bytes(u.mem_read(player(0),0x2408));chat('/turn 2');assert bytes(u.mem_read(player(0),0x2408))==before;checks+=1
            chat('/turn -1');assert 'Uso:' in messages[-1];checks+=1
            chat('/turn 1garbage');assert 'Uso:' in messages[-1];checks+=1
            put(account+0x4c+0x38,999);chat('/turn 1');assert 'aparencia invalida' in messages[-1];checks+=1
            put(account+0x4c+0x38,7)
            u.mem_write(account+8,b'Z'*32);put(account+0x34,0);put(account+0x38,0);put(account+0x3c,0)
            chat('/turn 0');assert bytes(u.mem_read(player(0)+4,32))==b'Z'*31+b'\0' and get(player(0)+0x24)==123456;checks+=1
            chat('/herobrine');assert get(player(0)+0x23b4)==3;checks+=1
            chat('/herobrine anything');assert 'Uso:' in messages[-1];checks+=1
            # No live body: appearance still updates player, but body commands refuse.
            put(player(0)+0x78,-1);chat('/herobrine');assert get(player(0)+0x23b4)==3;checks+=1
            rays=len(map_rays);chat('/tp');assert 'personagem vivo' in messages[-1] and len(map_rays)==rays;checks+=1
            put(player(0)+0x78,0);u.mem_write(player(0)+4,b'Admin\0')
            def pose():
                floats(human(0)+0x48,[10,20,30]);floats(human(0)+0x60,[2,3,4])
                for bone in range(15):
                    floats(human(0)+0xc8+bone*0xe0,[10+bone*.1,20+bone*.2,30+bone*.3,9+bone*.1,19+bone*.2,29+bone*.3,1,2,3])
                floats(human(0)+0x2b8,[1,0,0]);floats(human(0)+0x64,[0])
            pose();before=[struct.unpack('<6f',u.mem_read(human(0)+0xc8+bone*0xe0,24)) for bone in range(15)]
            chat('/tp');assert struct.unpack('<3f',u.mem_read(human(0)+0x48,12))==(100,26,200);checks+=1
            delta=[90,6,170]
            for bone in range(15):
                now=struct.unpack('<9f',u.mem_read(human(0)+0xc8+bone*0xe0,36))
                assert all(abs(now[j]-before[bone][j]-delta[j%3])<0.0001 for j in range(6)) and now[6:]==(0,0,0)
            checks+=1
            assert map_rays[-1][1][2]<map_rays[-1][0][2] and abs(map_rays[-1][1][2]-map_rays[-1][0][2]+2000)<.001;checks+=1
            map_hit[0]=False;before=bytes(u.mem_read(human(0),0x2b64));chat('/tp');assert bytes(u.mem_read(human(0),0x2b64))==before and 'Nenhuma' in messages[-1];checks+=1
            map_hit[0]=True;put(human(0)+0x14,0);rays=len(map_rays);chat('/tp');assert len(map_rays)==rays and 'veiculo' in messages[-1];checks+=1
            put(human(0)+0x14,-1);chat('/tp 5');assert 'Uso:' in messages[-1];checks+=1
            # Malformed skeleton refuses atomically.
            pose();floats(human(0)+0xc8+14*0xe0,[float('nan')]);before=bytes(u.mem_read(human(0),0x2b64));chat('/tp');assert bytes(u.mem_read(human(0),0x2b64))==before;checks+=1
            pose()
            for index,dest in enumerate([(178,25,453),(1084,25,418),(1117,25,955),(349,25,955),(674,25,1347)]):
                chat('/gobase '+str(index));assert struct.unpack('<3f',u.mem_read(human(0)+0x48,12))==dest;checks+=1
            before=bytes(u.mem_read(human(0),0x2b64));chat('/gobase 5');assert bytes(u.mem_read(human(0),0x2b64))==before and 'Uso:' in messages[-1];checks+=1
            pose();put(base+0xf437318,0);chat('/sound 21');event=base+0x93dd880
            assert get(base+0xf437318)==1 and get(event)==9 and get(event+0x20)==21;checks+=1
            assert struct.unpack('<3f',u.mem_read(event+8,12))==(100,25,200);checks+=1
            volume,pitch=struct.unpack('<2f',u.mem_read(event+0x30,8));assert volume==1 and abs(pitch-.15)<1e-6;checks+=1
            for bad in ('-1','48','999999','2xyz'):
                count=get(base+0xf437318);chat('/sound '+bad);assert get(base+0xf437318)==count and 'Uso:' in messages[-1];checks+=1
            for good in ('0','47'):
                count=get(base+0xf437318);chat('/sound '+good);assert get(base+0xf437318)==count+1;checks+=1
            map_hit[0]=False;count=get(base+0xf437318);chat('/sound 1');assert get(base+0xf437318)==count;checks+=1
            map_hit[0]=True;pose();floats(human(0)+0x64,[float('nan')]);rays=len(map_rays);chat('/sound 1');assert len(map_rays)==rays;checks+=1
            pose();put(player(0)+0x2c,0);before=bytes(u.mem_read(human(0),0x2b64));chat('/gobase 1');assert bytes(u.mem_read(human(0),0x2b64))==before and 'requer admin' in messages[-1];checks+=1
            put(player(0)+0x2c,1);put(ctx+76,0);assert chat('/herobrine')==42;checks+=1
            put(ctx+76,1);assert chat('texto /gobase 1')==42;checks+=1
            assert chat('/nick Novo')==42;checks+=1
            # Delegated admin uses the native permission; only admins grant it.
            put(player(1),1);put(player(1)+0x1a38,0);put(player(1)+0x34,0);put(player(1)+0x2c,0)
            put(player(0)+0x2c,0);chat('/give admin 1');assert get(player(1)+0x2c)==0 and 'requer admin' in messages[-1];checks+=1
            put(player(0)+0x2c,1);chat('/give admin 1');assert get(player(1)+0x2c)==1 and 'concedida' in messages[-1] and 'recebeu' in messages[-2];checks+=1
            before=bytes(u.mem_read(player(1),0x2408));chat('/give admin 1');assert bytes(u.mem_read(player(1),0x2408))==before and 'ja e' in messages[-1];checks+=1
            put(player(1)+0x2c,0);put(player(0)+0x78,0xffffffff);chat('/give admin 1');assert get(player(1)+0x2c)==1;checks+=1
            put(player(0)+0x78,0);u.mem_write(ctx+0x800,b'/team 3\0');assert call(base+0x49940,[1,ctx+0x800])==1 and get(player(1)+0x64)==3;checks+=1
            for msg in ('/give','/give admin','/give admin -1','/give admin 256','/give admin 1junk','/give admin 1 2','/give adminx 1','/give other 1'):
                before=bytes(u.mem_read(player(1),0x2408));chat(msg);assert bytes(u.mem_read(player(1),0x2408))==before and 'Uso:' in messages[-1];checks+=1
            put(player(1)+0x2c,0);put(player(1),0);chat('/give admin 1');assert get(player(1)+0x2c)==0 and 'nao encontrado' in messages[-1];checks+=1
            put(player(1),1);put(player(1)+0x1a38,1);chat('/give admin 1');assert get(player(1)+0x2c)==0;checks+=1
            put(player(1)+0x1a38,0);put(player(1)+0x34,0xffffffff);chat('/give admin 1');assert get(player(1)+0x2c)==0;checks+=1
            put(player(1)+0x34,0);put(ctx+76,0);assert chat('/give admin 1')==42 and get(player(1)+0x2c)==0;checks+=1
            put(ctx+76,1);chat('/give  admin   1  ');assert get(player(1)+0x2c)==1;checks+=1
            assert chat('texto /give admin 1')==42;checks+=1
            # /imortal keeps damage effects/flags and native godmode untouched.
            put(player(0)+0x78,0);put(player(0)+0x34,0);put(player(0)+0x1a38,0)
            put(human(0),1);put(human(0)+8,0);put(human(0)+0x28,40);put(player(0)+0x44,0)
            put(player(0)+0x2c,0);chat('/imortal');assert get(human(0)+0x28)==40 and 'requer admin' in messages[-1];checks+=1
            put(player(0)+0x2c,1);chat('/imortal 1');assert get(human(0)+0x28)==40 and 'Uso:' in messages[-1];checks+=1
            assert chat('/imortalfoo')==42;checks+=1
            put(human(7),1);put(human(7)+8,0);put(human(7)+0x28,100);put(player(0)+0x78,7)
            chat('/bmhelp');assert targets[-1][1]==7 and targets[-1][0]==1;checks+=1
            put(human(7),0);put(player(0)+0x78,0)
            chat('/imortal');assert get(human(0)+0x28)==100 and get(player(0)+0x44)==0 and 'ativado:' in messages[-1];checks+=1
            put(human(0)+0x28,0xffffff00);put(human(0)+0x2b1c,1)
            call(export('SR_LockHealth'),[0]);assert get(human(0)+0x28)==100 and get(human(0)+0x2b1c)==1;checks+=1
            put(human(1),1);put(human(1)+8,1);put(human(1)+0x28,70)
            call(export('SR_LockHealth'),[0x2b64]);assert get(human(1)+0x28)==70;checks+=1
            for offset in (1,512*0x2b64,0xffffffff):
                before=bytes(u.mem_read(human(0),0x2b64));call(export('SR_LockHealth'),[offset]);assert bytes(u.mem_read(human(0),0x2b64))==before;checks+=1
            # Full stock damage function: lethal damage still subtracts, then
            # our guard restores before native code can delete the body.
            before=len(native_commands);call(base+0x19020,[0,300]);assert get(human(0)+0x28)==100 and len(native_commands)==before;checks+=1
            call(base+0x19020,[1,5]);assert get(human(1)+0x28)==65;checks+=1
            # All six real hooks execute with a live x87 stack, then replay their
            # original register load/comparison. Temporarily stop at continuation.
            for rva,reg,size,out in [(0x19036,0xb8,6,UC_X86_REG_ECX),(0x4946a,0xbb,6,UC_X86_REG_EAX),
                                    (0x511f3,0xbe,6,UC_X86_REG_EDI),(0x53bf7,0xbe,6,None),
                                    (0x53d21,0xbe,7,None),(0x52b99,0xbe,6,UC_X86_REG_EAX)]:
                continuation=base+rva+size;old=bytes(u.mem_read(continuation,1));u.mem_write(continuation,b'\xc3');u.ctl_remove_cache(continuation,continuation+1)
                probe=code+0xb00;raw=b'\x60\xdb\xe3\xd9\xe8\xd9\xeb'
                raw+=b'\xba'+struct.pack('<I',ctx+0x200)+b'\x0f\xae\x02'
                raw+=bytes([reg])+struct.pack('<I',0)
                raw+=b'\xe8'+struct.pack('<i',base+rva-(probe+len(raw)+5))
                # Record replay output before scratch-register changes.
                if out is not None:
                    modrm={UC_X86_REG_EAX:0x05,UC_X86_REG_ECX:0x0d,UC_X86_REG_EDI:0x3d}[out]
                    raw+=b'\x89'+bytes([modrm])+struct.pack('<I',ctx+0x600)
                raw+=b'\xba'+struct.pack('<I',ctx+0x400)+b'\x0f\xae\x02\xdd\xd8\xdd\xd8\x61\xc3'
                u.mem_write(probe,raw);u.ctl_remove_cache(probe,probe+len(raw));put(human(0)+0x28,0xfffffff6)
                call(probe,preserve=True);assert get(human(0)+0x28)==100;checks+=1
                if out is not None:assert get(ctx+0x600)==100;checks+=1
                before=bytes(u.mem_read(ctx+0x200,160));after=bytes(u.mem_read(ctx+0x400,160));assert before[:8]+before[12:]==after[:8]+after[12:];checks+=1
                u.mem_write(continuation,old);u.ctl_remove_cache(continuation,continuation+1)
            # Check both branch outcomes, including exact original behavior
            # with the lock off (critical-health modules keep their flags).
            def branch_guard(locked):
                nonlocal checks
                continuations=[base+0x53bfd,base+0x53c10]
                saved=[bytes(u.mem_read(at,6)) for at in continuations]
                for at,value in zip(continuations,(1,2)):
                    u.mem_write(at,b'\xb8'+struct.pack('<I',value)+b'\xc3');u.ctl_remove_cache(at,at+6)
                for health in (25,-10):
                    put(human(0)+0x28,health);probe=code+0xb00
                    raw=b'\x60\xbe\x00\x00\x00\x00\x83\x3d'+struct.pack('<I',human(0)+0x28)+b'\x00'
                    raw+=b'\xe8'+struct.pack('<i',base+0x53bf7-(probe+len(raw)+5))
                    raw+=b'\xa3'+struct.pack('<I',ctx+0x600)+b'\x61\xc3'
                    u.mem_write(probe,raw);u.ctl_remove_cache(probe,probe+len(raw));call(probe,preserve=True)
                    assert get(ctx+0x600)==(1 if locked or health>0 else 2);checks+=1
                    assert get(human(0)+0x28)==(100 if locked else health&0xffffffff);checks+=1
                for at,raw in zip(continuations,saved):u.mem_write(at,raw);u.ctl_remove_cache(at,at+len(raw))
            branch_guard(True)
            chat('/imortal');put(human(0)+0x28,80);call(base+0x19020,[0,5]);assert get(human(0)+0x28)==75 and 'desativado' in messages[-1];checks+=1
            branch_guard(False)
            # Body replacement / disconnect / connection reuse must clear the lock.
            for where,value in [(player(0)+0x78,1),(player(0),0),(player(0)+0x34,1),(human(0),0),(human(0)+8,1)]:
                put(human(0)+0x28,100);chat('/imortal');old=get(where);put(where,value);call(export('SR_LockHealth'),[0]);put(where,old)
                put(human(0)+0x28,75);call(export('SR_LockHealth'),[0]);assert get(human(0)+0x28)==75;checks+=1
            chat('/imortal');put(ctx+76,0);call(export('SR_AttackTick'));put(ctx+76,1);put(human(0)+0x28,75);call(export('SR_LockHealth'),[0]);assert get(human(0)+0x28)==75;checks+=1
            for where,value in [(base+0x90dc560,1),(base+0x90dc578,max(0,get(base+0x90dc578)-1))]:
                put(human(0)+0x28,100);chat('/imortal');old=get(where);put(where,value);call(export('SR_AttackTick'));put(where,old)
                put(human(0)+0x28,75);call(export('SR_LockHealth'),[0]);assert get(human(0)+0x28)==75;checks+=1
            # End-of-frame maintenance covers external scripts such as punches.
            put(human(0)+0x28,100);chat('/imortal');put(human(0)+0x28,5);call(export('SR_Tick'));assert get(human(0)+0x28)==100;checks+=1
            chat('/imortal')
            # A lethal knife hit retains its actual impact event and bleeding
            # marker even though the victim's health finishes at 100.
            put(player(1),1);put(player(1)+0x78,1);put(player(1)+0x34,0);put(player(1)+0x1a38,0);put(player(1)+0x2c,1);put(player(1)+0x44,0)
            put(human(1),1);put(human(1)+8,1);put(human(1)+0x28,100);put(human(1)+0x34,0);put(human(1)+0x2b1c,0)
            chat('/imortal',1)
            put(item(2),1);put(item(2)+0xc,24);put(item(2)+0x38,1);put(item(2)+0x18,0)
            put(human(0)+0x291c,1);put(human(0)+0x2920,2);put(human(0)+0xbc,1);put(human(0)+0x28,100)
            floats(item(2)+0x9c,[0,0,1]);floats(human(0)+0x2d0,[0,0,1]);hit[0]=True;real_hit[0]=False
            impact=get(base+0xf437318);put(ctx+60,100);put(base+0x90dc578,get(base+0x90dc578)+100);call(export('SR_AttackTick'))
            assert get(human(1)+0x28)==100 and get(human(1)+0x2b1c)==1 and get(player(1)+0x44)==0;checks+=1
            assert get(base+0xf437318)>impact;checks+=1
            chat('/imortal',1);put(ctx+60,25);hit[0]=False;put(human(0)+0xbc,0)
            # /npc executes the actual player/body allocators on the early hook.
            put(human(0)+0x28,100);put(human(0)+0x14,-1);put(player(0)+0x1a38,0)
            def npc_tick():return call(code+92,preserve=True)
            def active_npcs():return [i for i in range(256) if get(player(i))==1 and get(player(i)+0x1a38)==1]
            before=active_npcs();put(player(0)+0x2c,0)
            chat('/npc');npc_tick();assert active_npcs()==before and 'requer admin' in messages[-1];checks+=1
            put(player(0)+0x2c,1);chat('/npc 2');npc_tick();assert active_npcs()==before and 'Uso:' in messages[-1];checks+=1
            assert chat('/npcfoo')==42;checks+=1
            assert chat('texto /npc')==42;checks+=1
            put(human(0)+0x28,0);chat('/npc');npc_tick();assert active_npcs()==before and 'vivo' in messages[-1];checks+=1
            put(human(0)+0x28,100);put(human(0)+0x14,0);chat('/npc');npc_tick();assert active_npcs()==before and 'veiculo' in messages[-1];checks+=1
            put(human(0)+0x14,-1);floats(human(0)+0x48,[100,20,200]);floats(player(0)+0x8c,[0])
            chat('/npc');assert active_npcs()==before and 'recebido' in messages[-1];checks+=1
            chat('/npc');assert 'pendente' in messages[-1];checks+=1
            npc_tick();assert len(active_npcs())==len(before)+1 and 'NPC criado' in messages[-1];checks+=1
            new=next(i for i in active_npcs() if i not in before);nb=get(player(new)+0x78)
            assert nb<512 and get(human(nb))==1 and get(human(nb)+8)==new and get(human(nb)+0x14)==0xffffffff;checks+=1
            assert get(human(nb)+0x28)==100 and get(player(new)+0x64)==6;checks+=1
            assert get(player(new)+0x1a44)==0 and get(player(new)+0x1a48)==0 and get(player(new)+0x34)==0xffffffff;checks+=1
            assert struct.unpack('<3f',u.mem_read(human(nb)+0x48,12))==(100,struct.unpack('<f',struct.pack('<f',20.15))[0],197);checks+=1
            assert abs(struct.unpack('<f',u.mem_read(human(nb)+0x2878,4))[0]-197)<.001;checks+=1
            count=len(active_npcs());npc_tick();assert len(active_npcs())==count;checks+=1
            floats(player(0)+0x8c,[1.57079632679]);chat('/npc');npc_tick();assert len(active_npcs())==count+1;checks+=1
            new2=active_npcs()[-1];nb2=get(player(new2)+0x78);pos=struct.unpack('<3f',u.mem_read(human(nb2)+0x48,12))
            assert abs(pos[0]-103)<.001 and abs(pos[2]-200)<.001;checks+=1
            count=len(active_npcs());chat('/npc');put(human(0)+0x28,0);npc_tick();assert len(active_npcs())==count and 'vivo' in messages[-1];checks+=1
            put(human(0)+0x28,100);chat('/npc');put(player(0),0);npc_tick();put(player(0),1);assert len(active_npcs())==count;checks+=1
            chat('/npc');put(player(0)+0x2c,0);npc_tick();put(player(0)+0x2c,1);assert len(active_npcs())==count;checks+=1
            floats(player(0)+0x8c,[float('nan')]);chat('/npc');npc_tick();assert len(active_npcs())==count and 'Direcao invalida' in messages[-1];checks+=1
            floats(player(0)+0x8c,[0]);floats(human(0)+0x48,[float('nan'),20,200]);chat('/npc');npc_tick();assert len(active_npcs())==count and 'Posicao invalida' in messages[-1];checks+=1
            floats(human(0)+0x48,[100,20,200]);put(ctx+76,0);assert chat('/npc')==42;put(ctx+76,1);checks+=1
            # Pools full must refuse before any allocation; restore all fixtures.
            saved_h=[get(human(i)) for i in range(512)]
            for i in range(512):put(human(i),1)
            chat('/npc');npc_tick();assert len(active_npcs())==count and 'Sem espaco' in messages[-1];checks+=1
            for i,v in enumerate(saved_h):put(human(i),v)
            saved_p=[get(player(i)) for i in range(256)]
            for i in range(256):put(player(i),1)
            chat('/npc');npc_tick();assert len(active_npcs())==count and 'Sem slot' in messages[-1];checks+=1
            for i,v in enumerate(saved_p):put(player(i),v)
            put(base+0x90dc560,1);chat('/npc');npc_tick();assert len(active_npcs())==count and 'partida' in messages[-1];checks+=1
            put(base+0x90dc560,2);chat('/npc');put(ctx+76,0);npc_tick();put(ctx+76,1);npc_tick();assert len(active_npcs())==count;checks+=1
            # Remove the command-test items, retaining the earlier market fixtures.
            for iid in active_items():
                if iid>=4:put(item(iid),0)
            put(human(0)+0x291c,0)
        if combined:
            put(player(0)+0x78,0);put(player(0)+0x34,0);put(player(0)+0x2c,1);put(player(0)+0x1a38,0);put(human(0)+8,0);put(human(0)+0x28,100)
            chat('/imortal')
        # No ticks -> timeout leaves phones and state retryable.
        assert call(export('SR_PrepareStop'),[ctx],True)==6 and get(item(0))==1;checks+=1
        assert call(export('SR_Stop'),[ctx],True)==7;checks+=1
        if combined:
            put(human(0)+0x28,75);call(export('SR_LockHealth'),[0]);assert get(human(0)+0x28)==75;checks+=1
            put(human(0)+0x28,100)
        sleep_mode[0]=True
        assert call(export('SR_PrepareStop'),[ctx],True)==0;checks+=1
        assert get(item(0))==0 and get(item(1))==0 and get(item(2))==1 and get(item(3))==1;checks+=1
        assert call(export('SR_Stop'),[ctx],True)==0;checks+=1
        install({'knife_price':250,'bandage_price':30});assert call(export('SR_Configure'),[ctx],True)==0;checks+=1
        if combined:
            put(human(0)+0x28,75);call(export('SR_LockHealth'),[0]);assert get(human(0)+0x28)==75;checks+=1
        tick(1200);put(item(0)+0x18,0);floats(human(0)+0x48,[692,25.725,354]);put(item(0)+0x108,24);tick(1201)
        assert get(player(0)+0x40)==750 and get(item(4)+0xc)==24,(get(player(0)+0x40),active_items(),messages[-3:],get(human(0)+0x28),get(human(0)+0x14));checks+=1
        # Identity guard: phone slot reused by another type must not be deleted.
        put(item(1)+0xc,12);assert call(export('SR_PrepareStop'),[ctx],True)==0 and get(item(1))==1;checks+=1
        call(export('SR_Stop'),[ctx],True)
        install();put(base+0x51060,0xe9ec8b55)
        assert call(export('SR_Configure'),[ctx],True)==4;checks+=1
    print(f'{checks} DLL assertions passed at two bases; real item allocator/deleter, hooks and cleanup exercised. Real human collision tested in synthetic geometry; Windows/client behavior not tested.')
if __name__=='__main__':verify(Path(sys.argv[1]))
