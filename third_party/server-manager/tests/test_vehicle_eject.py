import unittest
from test_engine import FakeBackend,PLUGINS
from test_dll_plugins import NativeBackend
from core import Engine
class VehicleEject(unittest.TestCase):
 def test_enabled_and_restored(self):
  p=PLUGINS['vehicle_eject'];self.assertFalse(p.get('unavailable'))
  for base in (0x400000,0x10000000):
   b=FakeBackend(base);b.seed(p);before=b.read(base+0x57060,5);e=Engine(b);e.enable(p)
   self.assertNotEqual(b.read(base+0x57060,5),before);e.disable(p['id']);self.assertEqual(b.read(base+0x57060,5),before)
 def test_coexists(self):
  b=NativeBackend();e=Engine(b)
  ps=[PLUGINS[i] for i in ('vehicle_eject','truck_speed','dealership_truck','native_combined','punch')]
  for p in ps:b.seed(p)
  for p in ps:e.enable(p)
  self.assertEqual(len(e.active),5)
  for ident in list(e.active):e.disable(ident)
if __name__=='__main__':unittest.main()
