import unittest,struct,json
from pathlib import Path
from test_engine import FakeBackend,PLUGINS
from core import Engine,PluginError
PLUGINS=dict(PLUGINS,npc_spawner=json.loads((Path(__file__).resolve().parents[1]/'tools/npc_spawner-archived.json').read_text()))
class Commands(unittest.TestCase):
    def setup_engine(self):
        b=FakeBackend();p=PLUGINS['npc_spawner'];b.seed(p);e=Engine(b);e.enable(p);return b,e,e.active[p['id']]['addresses']['.inject_data']
    def test_native_call_and_original_simulation_target(self):
        b,e,a=self.setup_engine();state=e.active['npc_spawner'];p=state['plugin']
        sec=next(s for s in p['sections'] if s['name']=='.inject')
        code=state['addresses']['.inject'];targets={0xe8:[],0xe9:[]}
        for r in sec['relocations']:
            if r['type']==2 and r['symbol']['name']=='__server':
                at=r['offset'];opcode=b.read(code+at-1,1)[0]
                targets[opcode].append(code+at+4+struct.unpack('<i',b.read(code+at,4))[0])
        hook=next(s for s in p['sections'] if s['name'].startswith('.patch_'))
        original=b.base+hook['rva']+5+struct.unpack_from('<i',bytes.fromhex(hook['expected']),1)[0]
        self.assertEqual(set(targets[0xe8]),{b.base+0x28020,b.base+0x50ae0,b.base+0x64a0})
        self.assertNotIn(b.base+0x585b0,targets[0xe8])
        self.assertEqual(targets[0xe9],[original])
        self.assertEqual(original,b.base+0x33570)
    def test_enabling_does_not_automatically_spawn(self):
        b,e,a=self.setup_engine();self.assertEqual(e.command_status('npc_spawner')['pending'],False)
        self.assertNotEqual(e.active['npc_spawner']['addresses']['.inject'],a)
    def test_request_is_queued_with_current_quantity(self):
        b,e,a=self.setup_engine();e.command('npc_spawner',{'quantity':3})
        self.assertEqual(struct.unpack('<iiiii',b.read(a,20)),(1,3,3,0,0))
        self.assertFalse(b.paused)
    def test_pending_request_cannot_be_replaced(self):
        b,e,a=self.setup_engine();e.command('npc_spawner',{'quantity':3});snapshot=b.read(a,20)
        with self.assertRaisesRegex(PluginError,'pedido pendente'):e.command('npc_spawner',{'quantity':5})
        self.assertEqual(b.read(a,20),snapshot)
    def test_completed_request_can_be_repeated(self):
        b,e,a=self.setup_engine();e.command('npc_spawner',{'quantity':3})
        b.write(a,struct.pack('<iiiii',0,3,0,3,2));e.command('npc_spawner',{'quantity':2})
        self.assertEqual(struct.unpack('<iiiii',b.read(a,20)),(1,2,2,0,0))
    def test_bad_quantity_does_not_change_state(self):
        b,e,a=self.setup_engine();before=b.read(a,20)
        for value in (0,9,-1,'abc'):
            with self.assertRaises(PluginError):e.command('npc_spawner',{'quantity':value})
            self.assertEqual(b.read(a,20),before)
    def test_hook_changed_blocks_commands(self):
        b,e,a=self.setup_engine();b.write(b.base+0x5cb79,b'\xcc')
        with self.assertRaisesRegex(PluginError,'hook'):e.command('npc_spawner',{'quantity':1})
        self.assertEqual(e.command_status('npc_spawner')['pending'],False)
    def test_failed_request_write_rolls_back(self):
        b,e,a=self.setup_engine();before=b.read(a,20);b.fail_on=b.writes+1
        with self.assertRaises(OSError):e.command('npc_spawner',{'quantity':1})
        self.assertEqual(b.read(a,20),before);self.assertFalse(b.paused)
    def test_disable_with_pending_request_restores_hook(self):
        b,e,a=self.setup_engine();e.command('npc_spawner',{'quantity':2});e.disable('npc_spawner')
        p=next(s for s in PLUGINS['npc_spawner']['sections'] if s['name'].startswith('.patch_'))
        self.assertEqual(b.read(b.base+p['rva'],5),bytes.fromhex(p['expected']))
        self.assertEqual(len(e.retained),2)
if __name__=='__main__':unittest.main()
