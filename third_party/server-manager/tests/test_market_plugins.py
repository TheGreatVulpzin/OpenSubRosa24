import copy, struct, unittest
from test_engine import PLUGINS
from test_dll_plugins import NativeBackend
from core import Engine,PluginError,validate_plugin
from dll_support import inspect_dll
P=PLUGINS['native_combined']

class MarketPlugins(unittest.TestCase):
    def setup_engine(self):
        b=NativeBackend();b.seed(P);e=Engine(b);e.enable(P);return b,e
    def test_parameters_and_hook_target(self):
        b=NativeBackend(0x10000000);b.seed(P);e=Engine(b)
        e.enable(P,{'knife_price':500,'point2_x':900,'knife_damage':30,'knife_debug':1})
        state=e.active[P['id']];ctx=state['dll_context'];hook=state['addresses']['.inject']
        self.assertEqual(struct.unpack('<III',b.read(ctx,12)),(80,1,b.base))
        self.assertEqual(struct.unpack('<i',b.read(ctx+20,4))[0],500)
        self.assertEqual(struct.unpack('<f',b.read(ctx+48,4))[0],900)
        self.assertEqual(struct.unpack('<i',b.read(ctx+60,4))[0],30)
        self.assertEqual(struct.unpack('<i',b.read(ctx+68,4))[0],1)
        attack=b.base+0x5cbfd
        self.assertEqual(attack+5+struct.unpack('<i',b.read(attack+1,4))[0],hook+40)
        at=b.base+0x5cc0c
        self.assertEqual(at+5+struct.unpack('<i',b.read(at+1,4))[0],hook)
    def test_prepare_runs_before_restore_and_stop(self):
        b,e=self.setup_engine();state=e.active[P['id']];_,pe=inspect_dll(P)
        e.disable(P['id'])
        self.assertEqual([a for a,c in b.calls[-2:]],[0x68000000+pe.exports['SR_PrepareStop'],0x68000000+pe.exports['SR_Stop']])
        self.assertEqual(b.read(b.base+0x5cc0c,5),bytes.fromhex('e8affcfeff'))
        self.assertEqual(b.read(b.base+0x5cbfd,5),bytes.fromhex('e85e44ffff'))
        self.assertFalse(e.active)
    def test_prepare_refusal_leaves_hook_active_and_retryable(self):
        b,e=self.setup_engine();state=e.active[P['id']];installed=b.read(b.base+0x5cc0c,5);b.results=[6]
        with self.assertRaisesRegex(PluginError,'código 6'):e.disable(P['id'])
        self.assertFalse(e.poisoned);self.assertIn(P['id'],e.active)
        self.assertEqual(b.read(b.base+0x5cc0c,5),installed)
        e.disable(P['id']);self.assertFalse(e.active)
    def test_altered_hook_rejects_before_cleanup(self):
        b,e=self.setup_engine();count=len(b.calls);b.write(b.base+0x5cc0c,b'\xcc')
        with self.assertRaisesRegex(PluginError,'limpeza cancelada'):e.disable(P['id'])
        self.assertEqual(len(b.calls),count);self.assertIn(P['id'],e.active)
    def test_prepare_timeout_uncertainty_poisoned(self):
        b,e=self.setup_engine()
        class Timeout(Exception):uncertain=True
        b.call_error=Timeout('call still running')
        with self.assertRaises(Timeout):e.disable(P['id'])
        self.assertTrue(e.poisoned);self.assertIn(P['id'],e.active)
    def test_bad_prepare_export_rejected(self):
        for value in (None,1,''):
            p=copy.deepcopy(P);p['dll']['prepare_stop']=value
            with self.assertRaises(PluginError):validate_plugin(p)
        p=copy.deepcopy(P);p['dll']['prepare_stop']='Missing'
        with self.assertRaisesRegex(PluginError,'não exporta'):inspect_dll(p)
    def test_combined_reactivates_with_one_dll_load(self):
        b,e=self.setup_engine();e.disable(P['id']);e.enable(P)
        self.assertEqual(len(b.loads),1)
        e.disable_all();self.assertFalse(e.active)
if __name__=='__main__':unittest.main()
