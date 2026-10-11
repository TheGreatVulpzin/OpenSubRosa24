import struct,unittest
from test_engine import FakeBackend,PLUGINS
from test_dll_plugins import NativeBackend
from core import Engine,PluginError
P=PLUGINS['dealership_truck']
class DealershipTruck(unittest.TestCase):
 def test_available_and_price_values_restore_on_disable(self):
  b=FakeBackend();b.seed(P)
  for at,val in [(0xac9b2d8,1000),(0xac9b2dc,12345)]:b.write(b.base+at,struct.pack('<I',val))
  e=Engine(b);e.enable(P,{'price':17000});state=e.active[P['id']]
  self.assertEqual(b.read(state['addresses']['.inject_data'],4),struct.pack('<I',17000))
  self.assertEqual(b.read(b.base+0xac9b2d8,4),struct.pack('<I',17000))
  e.disable(P['id'])
  self.assertEqual(b.read(b.base+0xac9b2d8,4),struct.pack('<I',1000))
  self.assertEqual(b.read(b.base+0xac9b2dc,4),struct.pack('<I',12345))
 def test_stock_module_coexists_with_combined(self):
  b=NativeBackend()
  for p in [P,PLUGINS['native_combined']]:b.seed(p)
  e=Engine(b)
  for p in [P,PLUGINS['native_combined']]:e.enable(p)
  for ident in list(e.active):e.disable(ident)
  self.assertFalse(e.active)
 def test_invalid_price_rejected(self):
  for value in (-1,1000001,'abc'):
   b=FakeBackend();b.seed(P);e=Engine(b)
   with self.assertRaises(PluginError):e.enable(P,{'price':value})
