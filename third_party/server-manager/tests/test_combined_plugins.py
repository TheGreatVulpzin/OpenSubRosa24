import struct,unittest
from test_engine import PLUGINS
from test_dll_plugins import NativeBackend
from core import Engine,PluginError
P=PLUGINS['native_combined']
class CombinedPlugins(unittest.TestCase):
    def test_single_configuration_restores_all_hooks(self):
        for base in (0x400000,0x10000000):
            b=NativeBackend(base);b.seed(P);originals={sec['rva']:b.read(base+sec['rva'],len(bytes.fromhex(sec['expected']))) for sec in P['sections'] if sec.get('expected')};e=Engine(b);e.enable(P,{'show_id':1,'commands_enabled':0})
            state=e.active[P['id']];ctx=state['dll_context'];code=state['addresses']['.inject']
            self.assertEqual(struct.unpack('<4I',b.read(ctx,16)),(80,1,base,code+80))
            self.assertEqual(struct.unpack('<2I',b.read(ctx+72,8)),(1,0))
            e.disable(P['id'])
            for sec in P['sections']:
                if sec.get('expected'):self.assertEqual(b.read(base+sec['rva'],len(bytes.fromhex(sec['expected']))),originals[sec['rva']])
            self.assertEqual(len(b.loads),1);self.assertFalse(e.active)
    def test_health_guards_coexist_with_other_scripts(self):
        death_modes={'instant_death','death_messages','death_messages_instant'}
        for mode in death_modes:
            b=NativeBackend();e=Engine(b)
            modules=[p for ident,p in PLUGINS.items() if ident not in death_modes or ident==mode]
            for plugin in modules:b.seed(plugin)
            for plugin in modules:e.enable(plugin)
            self.assertEqual(len(e.active),len(modules))
            for ident in list(e.active):e.disable(ident)

    def test_catalog_has_only_the_combined_module(self):
        self.assertIn('native_combined',PLUGINS)
        self.assertNotIn('npc_spawner',PLUGINS)
        self.assertNotIn('native_black_market',PLUGINS)
        self.assertNotIn('native_chat_console',PLUGINS)
if __name__=='__main__':unittest.main()
