import json,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
from script_layout import normalize_layout
from manager import Manager

class Tree:
    def __init__(self):self.nodes={};self.roots=[];self.sel=();self.cursor=''
    def get_children(self,parent=''):return tuple(self.nodes[parent]['children'] if parent else self.roots)
    def insert(self,parent,index,iid,**kw):
        self.nodes[iid]={'parent':None,'children':[],'text':'','open':False,'tags':(),**kw};self.move(iid,parent,index)
    def parent(self,i):return self.nodes[i]['parent'] or ''
    def index(self,i):return list(self.get_children(self.parent(i))).index(i)
    def detach(self,i):
        if self.nodes[i]['parent'] is not None:
            dest=self.nodes[self.parent(i)]['children'] if self.parent(i) else self.roots;dest.remove(i)
        self.nodes[i]['parent']=None
    def move(self,i,parent,index):
        ancestor=parent
        while ancestor:
            if ancestor==i:raise RuntimeError('cycle')
            ancestor=self.parent(ancestor)
        self.detach(i);dest=self.nodes[parent]['children'] if parent else self.roots
        dest.insert(len(dest) if index=='end' else index,i);self.nodes[i]['parent']=parent
    def item(self,i,option=None,**kw):
        self.nodes[i].update(kw)
        return self.nodes[i][option] if option else dict(self.nodes[i])
    def delete(self,*ids):
        for i in ids:
            self.delete(*self.get_children(i));self.detach(i);del self.nodes[i]
            if i in self.sel:self.sel=()
    def selection(self):return self.sel
    def selection_set(self,i):self.sel=(i,)
    def exists(self,i):return i in self.nodes
    def see(self,i):pass
    def configure(self,**kw):self.cursor=kw.get('cursor',self.cursor)
    def visible(self,parent=''):
        result=[]
        for i in self.get_children(parent):
            result.append(i)
            if self.item(i,'open'):result+=self.visible(i)
        return result
    def identify_row(self,y):
        row=(y-24)//20;v=self.visible();return v[row] if 0<=row<len(v) else ''
    def bbox(self,i):return (0,24+self.visible().index(i)*20,340,20)
    def winfo_width(self):return 340
    def winfo_height(self):return 300
    def set(self,i,column,value):self.nodes[i][column]=value

class Value:
    def __init__(self):self.value=''
    def set(self,value):self.value=value
    def get(self):return self.value

class LayoutTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        m=self.m=Manager.__new__(Manager);m.tree=Tree();m.plugins={i:{'name':i} for i in ('a','b','c')};m.settings={'parameters':{'a':{'power':'2'}}};m.configpath=Path(self.tmp.name)/'settings.json';m.selected=None;m.entries={};m.engine=None;m.drag=None;m.drag_hint=Value();m.sync=lambda:None;m.run=lambda f:f()
        for i in ('a','b','c'):m.tree.insert('','end',iid=i,text=i)
        m.tree.insert('','end',iid='folder:f',text='Veículos',open=False)
    def test_migrate_old_order_and_add_new(self):
        self.assertEqual(normalize_layout({'script_order':['b','b','missing','a']},self.m.plugins),[{'id':'b'},{'id':'a'},{'id':'c'}])
    def test_invalid_and_duplicate_layout(self):
        raw=[{'id':'folder:f','name':' X ','open':False,'children':[{'id':'a'},{'id':'missing'}]},{'id':'a'},{'id':'folder:f','children':[]},None]
        result=normalize_layout({'script_layout':raw},self.m.plugins)
        self.assertEqual(result[0],{'id':'folder:f','name':'X','open':False,'children':[{'id':'a'}]})
        self.assertEqual(result[1:],[{'id':'b'},{'id':'c'}])
    def test_reorder_and_move_into_closed_folder_persist(self):
        m=self.m;m.relocate('c','',0);self.assertEqual(m.tree.get_children(),('c','a','b','folder:f'))
        m.relocate('a','folder:f',0)
        saved=json.loads(m.configpath.read_text());self.assertEqual(saved['script_order'],['c','b','a'])
        folder=saved['script_layout'][-1];self.assertFalse(folder['open']);self.assertEqual(folder['children'],[{'id':'a'}])
        self.assertEqual(normalize_layout(saved,m.plugins),saved['script_layout']);self.assertEqual(saved['parameters']['a'],{'power':'2'})
    def test_move_out_and_sibling_buttons(self):
        m=self.m;m.relocate('a','folder:f',0);m.relocate('b','folder:f',1);m.tree.selection_set('b');m.move_script(-1)
        self.assertEqual(m.tree.get_children('folder:f'),('b','a'));m.relocate('b','',0);self.assertEqual(m.tree.get_children()[0],'b')
    def test_cycles_blocked(self):
        m=self.m;m.tree.insert('folder:f','end',iid='folder:g',text='Filha');before=m.layout_snapshot();m.relocate('folder:f','folder:g',0);self.assertEqual(before,m.layout_snapshot())
        m.tree.item('folder:f',open=True);self.assertIsNone(m.drop_destination(100,114,'folder:f'))
    def test_failed_save_rolls_back(self):
        m=self.m;before=m.layout_snapshot()
        with patch.object(m,'save',side_effect=OSError('disk')):
            with self.assertRaises(OSError):m.relocate('a','folder:f',0)
        self.assertEqual(before,m.layout_snapshot())
    def test_drop_positions(self):
        m=self.m
        self.assertEqual(m.drop_destination(100,65,'a')[:2],('',1)) # before c, after removal
        self.assertEqual(m.drop_destination(100,79,'a')[:2],('',2)) # after c
        self.assertEqual(m.drop_destination(100,94,'a')[:2],('folder:f',0))
        self.assertIsNone(m.drop_destination(-5,94,'a'))
        self.assertEqual(m.drop_destination(100,180,'a')[:2],('',4))
    def test_drag_release_moves_and_click_does_not(self):
        m=self.m;m.drag={'id':'a','moving':True};m.drag_end(SimpleNamespace(x=100,y=94));self.assertEqual(m.tree.parent('a'),'folder:f');self.assertIsNone(m.drag)
        before=m.layout_snapshot();m.drag={'id':'b','moving':False};m.drag_end(SimpleNamespace(x=100,y=180));self.assertEqual(before,m.layout_snapshot())
    def test_folder_delete_preserves_children_and_active_engine(self):
        m=self.m;m.relocate('a','folder:f',0);m.relocate('b','folder:f',1);m.engine=SimpleNamespace(active={'a':object()});engine=m.engine
        m.tree.selection_set('folder:f');m.select=lambda:None;m.remove_folder()
        self.assertEqual(m.tree.get_children(),('c','a','b'));self.assertIs(m.engine,engine);self.assertIn('a',m.engine.active)
    def test_delete_failure_restores_folder(self):
        m=self.m;m.relocate('a','folder:f',0);m.tree.selection_set('folder:f');before=m.layout_snapshot()
        with patch.object(m,'save',side_effect=OSError('disk')):
            with self.assertRaises(OSError):m.remove_folder()
        self.assertEqual(before,m.layout_snapshot())
    def test_new_and_rename(self):
        m=self.m;m.window=None;m.tree.selection_set('folder:f')
        with patch('manager.simpledialog.askstring',return_value=' Motor '):m.new_folder()
        new=m.tree.selection()[0];self.assertEqual(m.tree.parent(new),'folder:f');self.assertEqual(m.tree.item(new,'text'),'Motor')
        with patch('manager.simpledialog.askstring',return_value=' Ajustes '):m.rename_folder()
        self.assertEqual(m.tree.item(new,'text'),'Ajustes')
    def test_folder_toggle_saves_after_tk_state_change(self):
        m=self.m;callbacks=[];m.window=SimpleNamespace(after_idle=callbacks.append);m.folder_toggle();m.tree.item('folder:f',open=True);callbacks[0]()
        self.assertTrue(json.loads(m.configpath.read_text())['script_layout'][-1]['open'])

if __name__=='__main__':unittest.main()
