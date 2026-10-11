"""Exercise the actual backend methods with Win32 API stand-ins on Linux.
Only the platform binding/import preamble is omitted; production method bodies
are compiled verbatim from the AST. This is not a live Windows integration test.
"""
import ast
import ctypes
import os
import struct
import types
import unittest
from pathlib import Path
from test_engine import ROOT, PLUGINS
from core import PluginError
from dll_support import inspect_dll, LOAD_LIBRARY_EX_STUB


def production_methods():
    source=ast.parse((ROOT/'win32_backend.py').read_text())
    nodes=[n for n in source.body if isinstance(n,ast.ClassDef) and n.name in ('NativeCallError','WindowsBackend')]
    ns={'PluginError':PluginError,'C':ctypes,'DWORD':ctypes.c_uint32,'struct':struct,'Path':Path,'os':os,'contextmanager':__import__('contextlib').contextmanager}
    exec(compile(ast.Module(body=nodes,type_ignores=[]),'win32_backend.py','exec'),ns)
    return ns


def module_image(exports,base,size=0x4000):
    image=bytearray(size);image[:2]=b'MZ';struct.pack_into('<I',image,0x3c,0x80)
    image[0x80:0x84]=b'PE\0\0';struct.pack_into('<H',image,0x98,0x10b)
    struct.pack_into('<II',image,0x98+96,0x1000,0x500)
    count=len(exports);struct.pack_into('<IIHHIIIIIII',image,0x1000,0,0,0,0,0,1,count,count,0x1040,0x1080,0x10c0)
    cursor=0x1100
    for i,(name,target) in enumerate(exports.items()):
        struct.pack_into('<I',image,0x1080+4*i,cursor);payload=name.encode()+b'\0';image[cursor:cursor+len(payload)]=payload;cursor+=len(payload)
        struct.pack_into('<H',image,0x10c0+2*i,i)
        if isinstance(target,str):
            at=cursor;payload=target.encode()+b'\0';image[cursor:cursor+len(payload)]=payload;cursor+=len(payload)
        else:at=target
        struct.pack_into('<I',image,0x1040+4*i,at)
    return bytes(image)


class WindowsDLLProtocol(unittest.TestCase):
    def setup_backend(self):
        ns=production_methods();calls=[];closed=[];state={'wait':0,'result':0,'thread':123}
        ns['OpenProcess']=lambda rights,inherit,pid:calls.append(('open',rights,pid)) or 456
        ns['CreateRemoteThread']=lambda handle,attr,stack,address,arg,flags,tid:calls.append(('thread',address,arg)) or state['thread']
        ns['WaitForSingleObject']=lambda handle,timeout:state['wait']
        def exit_code(handle,out):ctypes.cast(out,ctypes.POINTER(ctypes.c_uint32))[0]=state['result'];return True
        ns['GetExitCodeThread']=exit_code;ns['CloseHandle']=lambda handle:closed.append(handle) or True
        ns['error']=lambda text:(_ for _ in ()).throw(PluginError(text))
        class Backend(ns['WindowsBackend']):
            def __init__(self):self.pid=7;self.dll_handle=None;self.native_calls=0;self.images={};self.mods=[]
            def alive(self):return True
            def _modules(self):return self.mods
            def read(self,address,n):
                for base,image in self.images.items():
                    if base<=address and address+n<=base+len(image):return image[address-base:address-base+n]
                raise PluginError('unmapped test memory')
        return Backend(),state,calls,closed
    def test_remote_thread_rights_and_result(self):
        b,state,calls,closed=self.setup_backend();state['result']=3
        self.assertEqual(b.remote_call(0x68001000,0x60000000),3)
        self.assertEqual(calls,[('open',0x043a,7),('thread',0x68001000,0x60000000)])
        self.assertEqual(closed,[123])
    def test_timeout_preserves_thread_and_reports_uncertainty(self):
        b,state,calls,closed=self.setup_backend();state['wait']=258
        with self.assertRaises(PluginError) as error:b.remote_call(0x68001000,0x60000000)
        self.assertTrue(error.exception.uncertain);self.assertEqual(closed,[123])
        self.assertNotIn('terminate',str(calls).lower())
    def test_native_exception_and_pointer_results_are_distinguished(self):
        b,state,calls,closed=self.setup_backend();state['result']=0xc0000005
        with self.assertRaises(PluginError) as error:b.remote_call(0x68001000,0x60000000)
        self.assertTrue(error.exception.uncertain)
        state['result']=0xd1000000
        self.assertEqual(b.remote_call(0x68001000,0x60000000,True),0xd1000000)
    def test_forwarded_loadlibrary_resolves_in_remote_32bit_modules(self):
        b,state,calls,closed=self.setup_backend()
        for name,base,exports in [('kernel32.dll',0x75000000,{'LoadLibraryW':'KERNELBASE.LoadLibraryW'}),('kernelbase.dll',0x76000000,{'LoadLibraryW':0x2000})]:
            b.images[base]=module_image(exports,base);b.mods.append(types.SimpleNamespace(szModule=name,modBaseAddr=base,modBaseSize=0x4000))
        self.assertEqual(b._export('kernel32.dll','LoadLibraryW'),0x76002000)
        self.assertEqual(b._export('kernelbase',1),0x76002000)
    def test_missing_or_circular_export_is_rejected(self):
        b,state,calls,closed=self.setup_backend();base=0x75000000
        b.images[base]=module_image({'LoadLibraryW':'kernel32.LoadLibraryW'},base)
        b.mods=[types.SimpleNamespace(szModule='kernel32.dll',modBaseAddr=base,modBaseSize=0x4000)]
        with self.assertRaisesRegex(PluginError,'circular'):b._export('kernel32','LoadLibraryW')
        with self.assertRaisesRegex(PluginError,'não encontrada'):b._export('kernel32','Missing')
    def native_image(self):
        plugin=PLUGINS['native_combined'];path,pe=inspect_dll(plugin)
        image=bytearray(pe.image_size);image[:pe.header_size]=pe.data[:pe.header_size]
        for rva,virtual,size,raw,flags in pe.sections:image[rva:rva+size]=pe.data[raw:raw+size]
        return path,pe,plugin['dll'],bytes(image)
    def test_resident_dll_is_reused_only_when_identity_matches(self):
        b,state,calls,closed=self.setup_backend();path,pe,spec,image=self.native_image();base=0x68000000
        b.images[base]=image;b.mods=[types.SimpleNamespace(szModule=path.name,szExePath=str(path),modBaseAddr=base,modBaseSize=pe.image_size)]
        self.assertEqual(b.load_dll(path,pe,spec),base);self.assertEqual(calls,[])
        damaged=bytearray(image);damaged[pe.exports[spec['identity']]]=ord('x');b.images[base]=bytes(damaged)
        with self.assertRaisesRegex(PluginError,'compilação'):b.load_dll(path,pe,spec)
    def test_same_name_from_another_directory_is_rejected(self):
        b,state,calls,closed=self.setup_backend();path,pe,spec,image=self.native_image()
        b.mods=[types.SimpleNamespace(szModule=path.name,szExePath=str(path.parent.parent/'another'/path.name))]
        with self.assertRaisesRegex(PluginError,'outro caminho'):b.load_dll(path,pe,spec)
        self.assertEqual(calls,[])
    def test_loadlibrary_receives_utf16_absolute_path_and_pointer_result(self):
        b,state,calls,closed=self.setup_backend();path,pe,spec,image=self.native_image();base=0x68000000
        kernel=0x75000000;b.images[kernel]=module_image({'LoadLibraryExW':0x2000},kernel)
        b.mods=[types.SimpleNamespace(szModule='kernel32.dll',modBaseAddr=kernel,modBaseSize=0x4000)]
        memory=[];addresses=iter([0x60000000,0x60010000,0x60020000]);b.allocate=lambda n:next(addresses);b.write=lambda at,data:memory.append((at,data));b.executable=lambda at,n:None
        def call(address,argument,pointer_result):
            self.assertEqual((address,argument,pointer_result),(0x60020000,0x60010000,True))
            b.images[base]=image;b.mods.append(types.SimpleNamespace(szModule=path.name,szExePath=str(path),modBaseAddr=base,modBaseSize=pe.image_size));return base
        b.remote_call=call
        self.assertEqual(b.load_dll(path,pe,spec),base)
        self.assertEqual(memory,[(0x60000000,(str(path.resolve())+'\0').encode('utf-16-le')),(0x60010000,struct.pack('<III',kernel+0x2000,0x60000000,0x1100)),(0x60020000,LOAD_LIBRARY_EX_STUB)])
        self.assertEqual(b.native_calls,1)


if __name__=='__main__':unittest.main()
