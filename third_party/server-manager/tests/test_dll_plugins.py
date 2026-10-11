import copy
import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path
from test_engine import FakeBackend, ROOT, PLUGINS
from core import Engine, PluginError, validate_plugin
from dll_support import PE32, inspect_dll

P = PLUGINS['native_combined']


class NativeBackend(FakeBackend):
    def __init__(self, base=0x400000):
        super().__init__(base)
        self.calls = []; self.loads = []; self.results = []; self.call_error = None
    def load_dll(self, path, pe, spec):
        self.loads.append(path)
        return 0x68000000
    def remote_call(self, address, argument):
        assert not self.paused, 'DLL/RPC calls must not run while game threads are suspended'
        self.calls.append((address, argument))
        if self.call_error:
            exc = self.call_error; self.call_error = None; raise exc
        return self.results.pop(0) if self.results else 0


class DLLPlugins(unittest.TestCase):
    def test_compiled_dll_is_pe32_sdk_and_identity_matches(self):
        path, pe = inspect_dll(P)
        self.assertEqual(set(pe.exports), {'SR_BuildId','SR_Configure','SR_PrepareStop','SR_Stop','SR_ChatHook','SR_Tick','SR_AttackTick','SR_NpcTick','SR_LockHealth'})
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), P['dll']['sha256'])
    def test_death_modes_configure_restore_and_reuse_dll(self):
        for base in (0x400000,0x10000000):
            b=NativeBackend(base);e=Engine(b);contexts=[]
            for mode,ident in enumerate(('death_messages','instant_death','death_messages_instant'),1):
                p=PLUGINS[ident];b.seed(p);e.enable(p);state=e.active[ident]
                context=state['dll_context'];contexts.append(context)
                self.assertEqual(struct.unpack('<6I',b.read(context,24)),(24,1,base,0,1,mode))
                saved=state['original'];e.disable(ident)
                for address,data in saved:self.assertEqual(b.read(address,len(data)),data)
            self.assertEqual(len(b.loads),1)
            self.assertEqual(len(set(contexts)),3)
            self.assertFalse(e.active)

    def test_non_dll_or_malformed_pe_is_rejected(self):
        for data in (b'', b'MZ', b'MZ' + b'\0' * 1024):
            with self.assertRaises(PluginError):PE32(data)
    def test_invalid_paths_are_rejected(self):
        for path in ('../x.dll', '/tmp/x.dll', 'C:/x.dll', 'native\\x.dll', 'x.exe'):
            p=copy.deepcopy(P);p['dll']['file']=path
            with self.assertRaises(PluginError):validate_plugin(p)
    def test_missing_export_or_wrong_architecture_rejected_before_loading(self):
        path, pe=inspect_dll(P)
        with tempfile.TemporaryDirectory() as folder:
            p=copy.deepcopy(P);p['dll']['file']='test.dll'
            target=Path(folder)/'test.dll';data=bytearray(path.read_bytes())
            nt=struct.unpack_from('<I',data,0x3c)[0];struct.pack_into('<H',data,nt+4,0x8664)
            target.write_bytes(data);p['dll']['sha256']=hashlib.sha256(data).hexdigest()
            with self.assertRaises(PluginError):inspect_dll(p,folder)
            target.write_bytes(path.read_bytes());p['dll']['sha256']=P['dll']['sha256'];p['dll']['stop']='Missing'
            with self.assertRaisesRegex(PluginError,'não exporta'):inspect_dll(p,folder)
    def test_wrong_hash_or_build_id_rejected(self):
        for key in ('sha256','build_id'):
            p=copy.deepcopy(P);p['dll'][key]='0'*64
            with self.assertRaises(PluginError):inspect_dll(p)
    def setup_engine(self,base=0x400000):
        b=NativeBackend(base);b.seed(P);return b,Engine(b)
    def test_aslr_hook_context_and_original_trampoline(self):
        _,pe=inspect_dll(P)
        for base in (0x400000,0x10000000):
            b,e=self.setup_engine(base);e.enable(P,{'show_id':1});state=e.active[P['id']]
            addresses=state['addresses'];ctx=addresses['.inject_data'];code=addresses['.inject']
            self.assertEqual(struct.unpack('<IIIII',b.read(ctx,20)),(80,1,base,code+80,1))
            at=base+0x49940
            self.assertEqual(at+5+struct.unpack('<i',b.read(at+1,4))[0],0x68000000+pe.exports['SR_ChatHook'])
            self.assertEqual(code+92+struct.unpack('<i',b.read(code+88,4))[0],base+0x49947)
            self.assertEqual(b.calls,[(0x68000000+pe.exports['SR_Configure'],ctx)])
            e.disable(P['id']);self.assertEqual(b.read(at,7),bytes.fromhex('8b44240483ec40'))
            self.assertEqual(b.calls[-1],(0x68000000+pe.exports['SR_Stop'],ctx));self.assertFalse(e.active)
    def test_hot_toggle_reuses_resident_module_and_retains_old_context(self):
        b,e=self.setup_engine();e.enable(P);old=e.active[P['id']]['dll_context'];e.disable(P['id']);e.enable(P,{'show_id':1})
        self.assertEqual(len(b.loads),1);self.assertNotEqual(e.active[P['id']]['dll_context'],old)
        self.assertEqual(len(e.retained),4)
    def test_configuration_refusal_leaves_original_hook_and_stops_dll(self):
        b,e=self.setup_engine();b.results=[3,0]
        with self.assertRaisesRegex(PluginError,'código 3'):e.enable(P)
        self.assertEqual(len(b.calls),2);self.assertFalse(e.active);self.assertFalse(e.poisoned)
        self.assertEqual(b.read(b.base+0x49940,7),bytes.fromhex('8b44240483ec40'))
    def test_existing_other_hook_does_not_get_overwritten(self):
        b,e=self.setup_engine();b.write(b.base+0x49940,b'\xcc')
        with self.assertRaisesRegex(PluginError,'Código diferente'):e.enable(P)
        self.assertEqual(b.read(b.base+0x49940,1),b'\xcc');self.assertFalse(e.active)
        self.assertEqual(len(b.calls),2)
    def test_failed_patch_rolls_back_then_stops_dll(self):
        b,e=self.setup_engine();b.fail_on=b.writes+3
        with self.assertRaises(OSError):e.enable(P)
        self.assertEqual(b.read(b.base+0x49940,7),bytes.fromhex('8b44240483ec40'))
        self.assertEqual(len(b.calls),2);self.assertFalse(e.active)
    def test_uncertain_native_call_blocks_further_modifications(self):
        b,e=self.setup_engine();exc=PluginError('timeout');exc.uncertain=True;b.call_error=exc
        with self.assertRaises(PluginError):e.enable(P)
        self.assertTrue(e.poisoned)
        with self.assertRaisesRegex(PluginError,'bloqueada'):e.enable(PLUGINS['starting_money'])
    def test_stop_refusal_is_not_reported_as_success(self):
        b,e=self.setup_engine();e.enable(P);b.results=[0,4]
        with self.assertRaisesRegex(PluginError,'parada'):e.disable(P['id'])
        self.assertTrue(e.poisoned);self.assertIn(P['id'],e.active)
        self.assertEqual(b.read(b.base+0x49940,7),bytes.fromhex('8b44240483ec40'))
    def test_same_dll_cannot_own_multiple_active_hooks(self):
        b,e=self.setup_engine();e.enable(P)
        p=copy.deepcopy(P);p['id']='other';
        for sec in p['sections']:
            if 'rva' in sec:sec['rva']+=0x100000
        with self.assertRaisesRegex(PluginError,'outro módulo'):e.enable(p)


if __name__=='__main__':unittest.main()
