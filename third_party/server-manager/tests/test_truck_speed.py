import struct,unittest
from test_engine import FakeBackend,PLUGINS
from test_dll_plugins import NativeBackend
from core import Engine,PluginError
P=PLUGINS['truck_speed']
class TruckSpeed(unittest.TestCase):
 def test_toggle_restores_both_instructions(self):
  for base in (0x400000,0x10000000):
   b=FakeBackend(base);b.seed(P);e=Engine(b);originals={sec['rva']:b.read(base+sec['rva'],6) for sec in P['sections'] if sec.get('expected')};e.enable(P,{'speed':2.5,'power':3})
   state=e.active[P['id']];self.assertEqual(struct.unpack('<2f',b.read(state['addresses']['.inject_data'],8)),(2.5,3))
   e.disable(P['id'])
   for sec in P['sections']:
    if sec.get('expected'):self.assertEqual(b.read(base+sec['rva'],6),originals[sec['rva']])
 def test_coexists_with_stock_and_combined(self):
  b=NativeBackend();e=Engine(b)
  ps=[P,PLUGINS['dealership_truck'],PLUGINS['native_combined']]
  for p in ps:b.seed(p)
  for p in ps:e.enable(p)
  self.assertEqual(len(e.active),3)
  for ident in list(e.active):e.disable(ident)
 def test_invalid_values(self):
  for value in (0,5,'nan','inf','abc'):
   b=FakeBackend();b.seed(P)
   with self.assertRaises(PluginError):Engine(b).enable(P,{'speed':value})
