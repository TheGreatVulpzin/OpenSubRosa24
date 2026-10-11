import unittest,sys,json,struct,copy
from pathlib import Path
from contextlib import contextmanager
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from core import Engine, PluginError, load_plugins
ROOT=Path(__file__).resolve().parents[1]
PLUGINS,ERRORS=load_plugins(ROOT/'plugins')
class FakeBackend:
    def __init__(self,base=0x400000):self.base=base;self.memory={};self.cursor=0x50000000;self.fail_on=None;self.paused=False;self.writes=0
    def read(self,a,n):return bytes(self.memory.get(a+i,0) for i in range(n))
    def write(self,a,data):
        self.writes+=1
        if self.fail_on==self.writes:self.fail_on=None;raise OSError('Injected write failure')
        for i,b in enumerate(data):self.memory[a+i]=b
    def pointer(self,a):return 0x70001234
    def allocate(self,n):a=self.cursor;self.cursor+=0x10000;return a
    def executable(self,a,n):pass
    @contextmanager
    def pause(self,forbidden):
        self.paused=True
        try:yield
        finally:self.paused=False
    def seed(self,p):
        for sec in p['sections']:
            if sec['name'] in ('.inject','.inject_data'):continue
            b=bytearray.fromhex(sec.get('expected') or '00'*len(bytes.fromhex(sec['data'])))
            for off in sec.get('expected_relocations',[]):struct.pack_into('<I',b,off,(struct.unpack_from('<I',b,off)[0]+self.base-0x400000)&0xffffffff)
            self.write(self.base+sec['rva'],b)
class Tests(unittest.TestCase):
    def test_readme_example_relocates_and_restores(self):
        import re
        p=json.loads(re.search(r'```json\n(.*?)\n```',(ROOT/'CRIAR-PLUGINS.md').read_text(),re.S)[1])
        b=FakeBackend(0x10000000);b.seed(p);e=Engine(b);e.enable(p)
        self.assertEqual(struct.unpack('<I',b.read(b.base+0xe02d,4))[0],5000)
        self.assertEqual(struct.unpack('<I',b.read(b.base+0xe029,4))[0],b.base+0xf437b94)
        e.disable(p['id']);self.assertEqual(struct.unpack('<I',b.read(b.base+0xe02d,4))[0],1000)
    def test_catalog(self):self.assertFalse(ERRORS);self.assertEqual(len(PLUGINS),17)
    def test_all_plugins_restore_actual_initial_memory(self):
        for base in (0x400000,0x10000000):
            for ident,p in PLUGINS.items():
                if p.get('unavailable') or p.get('kind')=='dll':continue
                with self.subTest(plugin=ident,base=base):
                    b=FakeBackend(base);b.seed(p);before=dict(b.memory);e=Engine(b);e.enable(p)
                    saved=e.active[ident]['original'];e.disable(ident)
                    for a,old in saved:self.assertEqual(b.read(a,len(old)),old)
                    self.assertFalse(e.active);self.assertFalse(b.paused)
    def test_money_config_and_restoration(self):
        b=FakeBackend();p=PLUGINS['starting_money'];b.seed(p);e=Engine(b);e.enable(p,{'money':'7000'})
        self.assertEqual(struct.unpack('<I',b.read(b.base+0xe02d,4))[0],7000)
        e.disable(p['id']);self.assertEqual(struct.unpack('<I',b.read(b.base+0xe02d,4))[0],1000)
    def test_punch_parameters(self):
        b=FakeBackend();p=PLUGINS['punch'];b.seed(p);e=Engine(b);e.enable(p,{'damage':'12.5','cooldown':'85','knockback':'4'})
        a=e.retained[-1][0]
        for key,value,fmt in [('damage',12.5,'<f'),('cooldown',85,'<i'),('knockback',-4.,'<f')]:
            off=p['parameters'][key]['sites'][0]['offset'];self.assertEqual(struct.unpack(fmt,b.read(a+off,4))[0],value)
    def test_byte_parameter(self):
        b=FakeBackend();p=PLUGINS['max_players'];b.write(b.base+0x761eabc,b'\x18\xaa');e=Engine(b);e.enable(p,{'value':12})
        self.assertEqual(b.read(b.base+0x761eabc,2),b'\x0c\xaa');e.disable(p['id']);self.assertEqual(b.read(b.base+0x761eabc,2),b'\x18\xaa')
    def test_conflicting_death_modes_rejected(self):
        from test_dll_plugins import NativeBackend
        b=NativeBackend();p=PLUGINS['instant_death'];b.seed(p);e=Engine(b);e.enable(p)
        with self.assertRaisesRegex(PluginError,'Conflito'):e.enable(PLUGINS['death_messages'])
        self.assertEqual(list(e.active),['instant_death'])
    def test_existing_mod_not_overwritten(self):
        b=FakeBackend();p=PLUGINS['starting_money'];b.seed(p);b.write(b.base+0xe027,b'\xcc');before=dict(b.memory);e=Engine(b)
        with self.assertRaisesRegex(PluginError,'Código diferente'):e.enable(p)
        self.assertEqual(b.memory,before);self.assertFalse(e.active)
    def test_failed_enable_rolls_back_every_patch(self):
        b=FakeBackend();p=PLUGINS['punch'];b.seed(p);e=Engine(b)
        # Injection write, then first hook succeeds, second hook fails.
        b.fail_on=b.writes+3
        with self.assertRaises(OSError):e.enable(p)
        self.assertFalse(e.active);self.assertFalse(b.paused)
        for s in p['sections']:
            if s.get('expected'):self.assertEqual(b.read(b.base+s['rva'],len(bytes.fromhex(s['expected']))),bytes.fromhex(s['expected']))
    def test_external_change_prevents_disable(self):
        b=FakeBackend();p=PLUGINS['starting_money'];b.seed(p);e=Engine(b);e.enable(p);b.write(b.base+0xe027,b'\xcc')
        with self.assertRaisesRegex(PluginError,'outro programa'):e.disable(p['id'])
        self.assertIn(p['id'],e.active)
    def test_bad_parameters(self):
        for value in ['nan',-1,100000001,'abc']:
            b=FakeBackend();e=Engine(b)
            with self.assertRaises(PluginError):e.enable(PLUGINS['starting_money'],{'money':value})
    def test_allocations_not_reused_after_toggle(self):
        b=FakeBackend();p=PLUGINS['punch'];b.seed(p);e=Engine(b);e.enable(p);first=e.retained[0][0];e.disable(p['id']);e.enable(p)
        self.assertNotEqual(first,e.retained[1][0])
    def test_unavailable_plugins_fail_before_accessing_memory(self):
        e=Engine(FakeBackend())
        p={'format':1,'id':'blocked','name':'Bloqueado','description':'','unavailable':'Teste'}
        with self.assertRaises(PluginError):e.enable(p)
        self.assertFalse(e.retained)
if __name__=='__main__':unittest.main()
