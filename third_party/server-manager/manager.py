"""Sub Rosa Server Manager 0.7.1 — Windows GUI, standard-library-only runtime."""
import os, sys, json, logging, subprocess, time
from pathlib import Path
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, simpledialog
from script_layout import normalize_layout
from core import Engine, PluginError, load_plugins, SERVER_SHA256
ROOT=Path(__file__).resolve().parent

class Manager:
    def __init__(self,window):
        from win32_backend import WindowsBackend, servers
        self.Backend=WindowsBackend;self.servers=servers;self.window=window
        self.engine=None;self.plugins={};self.entries={};self.processes=[];self.selected=None;self.command_seen={}
        self.configpath=ROOT/'settings.json';self.settings={}
        if self.configpath.exists():
            try:self.settings=json.loads(self.configpath.read_text(encoding='utf-8'))
            except Exception as e:messagebox.showwarning('Configuração',str(e))
        window.title('Sub Rosa 24 — Gerenciador de scripts');window.geometry('1000x650');window.minsize(880,570)
        style=ttk.Style();style.theme_use('clam')
        top=ttk.Frame(window,padding=12);top.pack(fill='x')
        ttk.Label(top,text='SUB ROSA 24',font=('Segoe UI',17,'bold')).pack(side='left')
        self.status=tk.StringVar(value='Desconectado');ttk.Label(top,textvariable=self.status).pack(side='right')
        bar=ttk.Frame(window,padding=(12,0,12,10));bar.pack(fill='x')
        self.proc=tk.StringVar();self.combo=ttk.Combobox(bar,textvariable=self.proc,state='readonly',width=30);self.combo.pack(side='left')
        for label,action in [('Atualizar servidores',self.refresh),('Conectar',self.connect),('Abrir servidor',self.launch),('Desconectar',self.disconnect)]:ttk.Button(bar,text=label,command=lambda f=action:self.run(f)).pack(side='left',padx=4)
        body=ttk.Panedwindow(window,orient='horizontal');body.pack(fill='both',expand=True,padx=12)
        left=ttk.Frame(body);right=ttk.Frame(body,padding=12);body.add(left,weight=1);body.add(right,weight=2)
        treebox=ttk.Frame(left);treebox.pack(fill='both',expand=True)
        self.tree=ttk.Treeview(treebox,columns=('state',),show='tree headings',selectmode='browse');self.tree.heading('#0',text='Script');self.tree.heading('state',text='Estado');self.tree.column('#0',width=270);self.tree.column('state',width=95)
        treescroll=ttk.Scrollbar(treebox,orient='vertical',command=self.tree.yview);self.tree.configure(yscrollcommand=treescroll.set)
        treescroll.pack(side='right',fill='y');self.tree.pack(side='left',fill='both',expand=True)
        self.tree.bind('<<TreeviewSelect>>',self.select)
        self.drag=None
        self.tree.tag_configure('drop',background='#c9e6fb')
        self.tree.bind('<ButtonPress-1>',self.drag_start,add='+')
        self.tree.bind('<B1-Motion>',self.drag_motion,add='+')
        self.tree.bind('<ButtonRelease-1>',self.drag_end,add='+')
        self.tree.bind('<<TreeviewOpen>>',self.folder_toggle,add='+')
        self.tree.bind('<<TreeviewClose>>',self.folder_toggle,add='+')
        self.tree.bind('<Escape>',lambda event:self.cancel_drag())
        folderbar=ttk.Frame(left,padding=(0,6,0,0));folderbar.pack(fill='x')
        for label,action in [('Nova pasta',self.new_folder),('Renomear',self.rename_folder),('Excluir pasta',self.remove_folder)]:
            ttk.Button(folderbar,text=label,command=lambda f=action:self.run(f)).pack(side='left',padx=2)
        self.drag_hint=tk.StringVar(value='Arraste para ordenar ou colocar em uma pasta.')
        ttk.Label(left,textvariable=self.drag_hint,wraplength=340).pack(fill='x',pady=4)
        orderbar=ttk.Frame(left,padding=(0,6,0,0));orderbar.pack(fill='x')
        self.upbtn=ttk.Button(orderbar,text='↑ Subir',command=lambda:self.run(lambda:self.move_script(-1)));self.upbtn.pack(side='left')
        self.downbtn=ttk.Button(orderbar,text='↓ Descer',command=lambda:self.run(lambda:self.move_script(1)));self.downbtn.pack(side='left',padx=6)
        ttk.Label(orderbar,text='Organização salva').pack(side='left',padx=4)
        self.title=tk.StringVar(value='Selecione um script');ttk.Label(right,textvariable=self.title,font=('Segoe UI',15,'bold')).pack(anchor='w')
        self.desc=tk.StringVar();ttk.Label(right,textvariable=self.desc,wraplength=480,justify='left').pack(anchor='w',pady=12)
        fieldbox=ttk.Frame(right);fieldbox.pack(fill='both',expand=True)
        fieldcanvas=tk.Canvas(fieldbox,height=220,highlightthickness=0)
        fieldscroll=ttk.Scrollbar(fieldbox,orient='vertical',command=fieldcanvas.yview)
        fieldcanvas.configure(yscrollcommand=fieldscroll.set)
        fieldscroll.pack(side='right',fill='y');fieldcanvas.pack(side='left',fill='both',expand=True)
        self.fields=ttk.Frame(fieldcanvas)
        fieldwindow=fieldcanvas.create_window((0,0),window=self.fields,anchor='nw')
        self.fields.bind('<Configure>',lambda event:fieldcanvas.configure(scrollregion=fieldcanvas.bbox('all')))
        fieldcanvas.bind('<Configure>',lambda event:fieldcanvas.itemconfigure(fieldwindow,width=event.width))
        self.fieldcanvas=fieldcanvas
        actions=ttk.Frame(right);actions.pack(fill='x',pady=18)
        self.enablebtn=ttk.Button(actions,text='Ativar',command=lambda:self.run(self.enable));self.enablebtn.pack(side='left')
        self.disablebtn=ttk.Button(actions,text='Desativar',command=lambda:self.run(self.disable));self.disablebtn.pack(side='left',padx=8)
        self.commandbtn=ttk.Button(actions,text='Executar comando',command=lambda:self.run(self.command));self.commandbtn.pack(side='left')
        self.commandtext=tk.StringVar();ttk.Label(right,textvariable=self.commandtext,wraplength=470).pack(anchor='w',pady=8)
        ttk.Label(right,text='Scripts comuns: desative, edite e ative novamente. No spawn, pode mudar a quantidade antes de cada pedido.\nAs opções são salvas, mas scripts não são ativados automaticamente.',wraplength=470).pack(anchor='w')
        foot=ttk.Frame(window,padding=12);foot.pack(fill='x')
        ttk.Button(foot,text='Recarregar pasta plugins',command=lambda:self.run(self.reload)).pack(side='left')
        ttk.Button(foot,text='Adicionar script JSON',command=lambda:self.run(self.import_plugin)).pack(side='left',padx=8)
        ttk.Button(foot,text='Salvar configurações',command=lambda:self.run(self.save)).pack(side='left',padx=8)
        ttk.Button(foot,text='Desativar todos',command=lambda:self.run(self.disable_all)).pack(side='right')
        self.console=tk.Text(window,height=6,state='disabled',font=('Consolas',9));self.console.pack(fill='x',padx=12,pady=(0,12))
        window.protocol('WM_DELETE_WINDOW',self.close);self.reload();self.refresh();self.tick()
    def log(self,text):
        logging.info(text);self.console.configure(state='normal');self.console.insert('end',time.strftime('%H:%M:%S')+' '+text+'\n');self.console.see('end');self.console.configure(state='disabled')
    def run(self,f):
        try:f()
        except Exception as e:logging.exception('Operação falhou');self.log(str(e));messagebox.showerror('Operação não concluída',str(e))
        self.sync()
    def refresh(self):
        self.processes=self.servers();vals=[f'PID {pid} — {name}' for pid,name in self.processes];self.combo['values']=vals
        if vals:self.combo.current(0)
        else:self.proc.set('Nenhum servidor aberto')
    def connect(self):
        if self.engine:raise PluginError('Desconecte da sessão atual primeiro.')
        i=self.combo.current()
        if i<0:raise PluginError('Abra o servidor e atualize a lista.')
        backend=self.Backend(self.processes[i][0]);self.engine=Engine(backend)
        self.status.set(f'Conectado — PID {backend.pid}');self.log('Conectado ao servidor 024d; versão verificada.')
    def launch(self):
        path=filedialog.askopenfilename(title='Selecione subrosadedicated024d.exe',filetypes=[('Servidor Sub Rosa','subrosadedicated024d.exe')])
        if not path:return
        import hashlib
        path=Path(path)
        if hashlib.sha256(path.read_bytes()).hexdigest()!=SERVER_SHA256:raise PluginError('Executável incompatível com os plugins incluídos.')
        self.settings['server_path']=str(path);self.save()
        subprocess.Popen([str(path)],cwd=str(path.parent));self.log('Servidor iniciado. Aguarde a inicialização, atualize a lista e conecte.')
        self.window.after(1000,lambda:self.run(self.refresh))
    def reload(self):
        self.capture()
        plugins,errors=load_plugins(ROOT/'plugins')
        if errors:raise PluginError('\n'.join(errors))
        if self.engine:
            for ident,state in self.engine.active.items():
                if plugins.get(ident)!=state['plugin']:raise PluginError('Desative '+state['plugin']['name']+' antes de editar/remover seu arquivo.')
        self.cancel_drag()
        self.plugins=plugins;self.tree.delete(*self.tree.get_children())
        layout=normalize_layout(self.settings,plugins)
        def insert(nodes,parent=''):
            for node in nodes:
                ident=node['id']
                if 'children' in node:
                    self.tree.insert(parent,'end',iid=ident,text=node['name'],open=node['open'],values=('Pasta',))
                    insert(node['children'],ident)
                else:
                    p=plugins[ident];self.tree.insert(parent,'end',iid=ident,text=p['name'],values=('Indisponível' if p.get('unavailable') else 'Desativado',))
        insert(layout);self.remember_layout()
        if self.selected in plugins:self.tree.selection_set(self.selected);self.select()
        else:self.selected=None;self.select()
        self.sync()
        self.log(f'{len(plugins)} módulos carregados. Plugins DLL usam um JSON acompanhado de sua DLL.')
    def layout_snapshot(self,parent=''):
        nodes=[]
        for ident in self.tree.get_children(parent):
            node={'id':ident}
            if ident not in self.plugins:
                node.update(name=self.tree.item(ident,'text'),open=bool(self.tree.item(ident,'open')),children=self.layout_snapshot(ident))
            nodes.append(node)
        return nodes
    def remember_layout(self):
        self.settings['script_layout']=self.layout_snapshot()
        def scripts(nodes):
            for node in nodes:
                if 'children' in node:yield from scripts(node['children'])
                else:yield node['id']
        self.settings['script_order']=list(scripts(self.settings['script_layout']))
    def restore_layout(self,nodes,parent=''):
        for index,node in enumerate(nodes):
            self.tree.move(node['id'],parent,index)
            if 'children' in node:
                self.tree.item(node['id'],text=node['name'],open=node['open'])
                self.restore_layout(node['children'],node['id'])
    def relocate(self,ident,parent,index):
        ancestor=parent
        while ancestor:
            if ancestor==ident:return
            ancestor=self.tree.parent(ancestor)
        before=self.layout_snapshot()
        self.tree.move(ident,parent,index);self.remember_layout()
        try:self.save()
        except Exception:
            self.restore_layout(before);self.remember_layout();raise
        self.tree.selection_set(ident);self.sync()
    def move_script(self,direction):
        selection=self.tree.selection()
        if not selection:return
        ident=selection[0];parent=self.tree.parent(ident);order=list(self.tree.get_children(parent));target=order.index(ident)+direction
        if 0<=target<len(order):self.relocate(ident,parent,target);self.tree.see(ident)
    def new_folder(self):
        import uuid
        name=simpledialog.askstring('Nova pasta','Nome da pasta:',parent=self.window)
        if not name or not name.strip():return
        sel=self.tree.selection();parent=sel[0] if sel and sel[0] not in self.plugins else (self.tree.parent(sel[0]) if sel else '')
        ident='folder:'+uuid.uuid4().hex
        self.tree.insert(parent,'end',iid=ident,text=name.strip(),open=True,values=('Pasta',))
        self.remember_layout()
        try:self.save()
        except Exception:self.tree.delete(ident);self.remember_layout();raise
        self.tree.selection_set(ident);self.tree.see(ident)
    def rename_folder(self):
        sel=self.tree.selection()
        if not sel or sel[0] in self.plugins:return
        ident=sel[0];old=self.tree.item(ident,'text')
        name=simpledialog.askstring('Renomear pasta','Nome da pasta:',initialvalue=old,parent=self.window)
        if not name or not name.strip():return
        self.tree.item(ident,text=name.strip());self.remember_layout()
        try:self.save()
        except Exception:self.tree.item(ident,text=old);self.remember_layout();raise
    def remove_folder(self):
        sel=self.tree.selection()
        if not sel or sel[0] in self.plugins:return
        ident=sel[0];parent=self.tree.parent(ident);index=self.tree.index(ident);before=self.layout_snapshot()
        for offset,child in enumerate(self.tree.get_children(ident)):self.tree.move(child,parent,index+offset)
        # Keep the empty node until saving succeeds, so a failed save can be undone.
        self.tree.detach(ident);self.remember_layout()
        try:self.save()
        except Exception:self.restore_layout(before);self.remember_layout();raise
        self.tree.delete(ident);self.select();self.sync()
    def folder_toggle(self,event=None):
        # Tk emits the event before changing the open flag.
        self.window.after_idle(lambda:self.run(self.save_layout))
    def save_layout(self):
        self.remember_layout();self.save()
    def cancel_drag(self):
        if getattr(self,'drag',None):
            target=self.drag.get('highlight')
            if target and self.tree.exists(target):self.tree.item(target,tags=())
        self.drag=None;self.tree.configure(cursor='')
        if hasattr(self,'drag_hint'):self.drag_hint.set('Arraste para ordenar ou colocar em uma pasta.')
    def drag_start(self,event):
        self.cancel_drag()
        ident=self.tree.identify_row(event.y)
        if not ident or self.tree.identify_region(event.x,event.y) not in ('tree','cell'):return
        if 'indicator' in self.tree.identify_element(event.x,event.y):return
        self.drag={'id':ident,'x':event.x,'y':event.y,'moving':False}
    def drop_destination(self,x,y,source):
        if x<0 or x>self.tree.winfo_width() or y<0 or y>self.tree.winfo_height():return None
        target=self.tree.identify_row(y)
        if not target:
            if y<24:return None
            return ('',len(self.tree.get_children('')),'Fim da lista','')
        if target==source:return None
        box=self.tree.bbox(target);fraction=(y-box[1])/box[3]
        if target not in self.plugins and .25<=fraction<=.75:
            parent=target;index=len(self.tree.get_children(target));label='Dentro de '+self.tree.item(target,'text')
        else:
            parent=self.tree.parent(target);index=self.tree.index(target)+(fraction>=.5)
            label=('Depois de ' if fraction>=.5 else 'Antes de ')+self.tree.item(target,'text')
        ancestor=parent
        while ancestor:
            if ancestor==source:return None
            ancestor=self.tree.parent(ancestor)
        if self.tree.parent(source)==parent and self.tree.index(source)<index:index-=1
        return parent,index,label,target
    def drag_motion(self,event):
        if not self.drag:return
        if not self.drag['moving'] and abs(event.x-self.drag['x'])+abs(event.y-self.drag['y'])<6:return
        self.drag['moving']=True;self.tree.configure(cursor='fleur')
        if event.y<30:self.tree.yview_scroll(-1,'units')
        elif event.y>self.tree.winfo_height()-24:self.tree.yview_scroll(1,'units')
        old=self.drag.get('highlight')
        if old:self.tree.item(old,tags=())
        dest=self.drop_destination(event.x,event.y,self.drag['id'])
        self.drag['highlight']=dest[3] if dest else None
        if dest and dest[3]:self.tree.item(dest[3],tags=('drop',))
        self.drag_hint.set(dest[2] if dest else 'Solte sobre uma linha ou pasta válida.')
        return 'break'
    def drag_end(self,event):
        if not self.drag:return
        source=self.drag['id'];moving=self.drag['moving']
        dest=self.drop_destination(event.x,event.y,source) if moving else None
        self.cancel_drag()
        if dest:self.run(lambda:self.relocate(source,dest[0],dest[1]))
        if moving:return 'break'
    def import_plugin(self):
        from core import validate_plugin
        import re
        path=filedialog.askopenfilename(title='Adicionar um plugin JSON',filetypes=[('Plugin','*.json')])
        if not path:return
        path=Path(path)
        data=path.read_text(encoding='utf-8');plugin=json.loads(data);validate_plugin(plugin)
        ident=plugin['id']
        if not re.fullmatch(r'[a-zA-Z0-9_-]+',ident):raise PluginError('Use um ID com letras, números, _ ou -.')
        if ident in self.plugins:raise PluginError('Este ID já existe. Use outro ID ou edite o arquivo existente com o script desativado.')
        copied=None
        if plugin.get('kind')=='dll':
            from dll_support import inspect_dll
            import shutil,hashlib
            source,pe=inspect_dll(plugin,path.parent)
            destination=(ROOT/'plugins'/plugin['dll']['file']).resolve()
            if not destination.is_relative_to((ROOT/'plugins').resolve()):raise PluginError('Caminho da DLL fora da pasta plugins.')
            if destination.exists():
                if hashlib.sha256(destination.read_bytes()).hexdigest()!=plugin['dll']['sha256']:raise PluginError('Já existe outra DLL neste caminho. Use outro nome no plugin.')
            else:
                destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,destination);copied=destination
        target=ROOT/'plugins'/(ident+'.json')
        try:target.write_text(data,encoding='utf-8')
        except Exception:
            if copied:copied.unlink(missing_ok=True)
            raise
        self.reload();self.tree.selection_set(ident);self.select();self.log('Adicionado: '+plugin['name'])
    def capture(self):
        if self.selected and self.entries:self.settings.setdefault('parameters',{})[self.selected]={key:v.get() for key,v in self.entries.items()}
    def select(self,event=None):
        self.capture();sel=self.tree.selection()
        if not sel:
            self.selected=None;self.entries={}
            for widget in self.fields.winfo_children():widget.destroy()
            self.title.set('Selecione um script');self.desc.set('');self.sync();return
        self.selected=sel[0] if sel[0] in self.plugins else None
        if self.selected is None:
            self.title.set(self.tree.item(sel[0],'text'));self.desc.set('Arraste scripts para esta pasta. Use a seta ao lado do nome para abrir ou fechar.')
            for widget in self.fields.winfo_children():widget.destroy()
            self.entries={};self.commandtext.set('');self.sync();return
        p=self.plugins[self.selected];self.title.set(p['name']);self.desc.set(p.get('description',''))
        for widget in self.fields.winfo_children():widget.destroy()
        self.fieldcanvas.yview_moveto(0)
        self.entries={};saved=self.settings.get('parameters',{}).get(self.selected,{})
        for row,(key,spec) in enumerate(p.get('parameters',{}).items()):
            ttk.Label(self.fields,text=spec['label']).grid(row=row,column=0,sticky='w',pady=5)
            v=tk.StringVar(value=str(saved.get(key,spec['default'])));self.entries[key]=v
            ttk.Entry(self.fields,textvariable=v,width=18).grid(row=row,column=1,sticky='e',padx=12)
        self.sync()
    def need(self):
        if not self.engine or not self.engine.backend.alive():raise PluginError('Conecte a um servidor aberto primeiro.')
        return self.engine
    def enable(self):
        if not self.selected:return
        engine=self.need();self.capture();engine.enable(self.plugins[self.selected],self.settings.get('parameters',{}).get(self.selected,{}));self.save();self.log('Ativado: '+self.plugins[self.selected]['name'])
    def command(self):
        if not self.selected:return
        engine=self.need();self.capture();engine.command(self.selected,self.settings.get('parameters',{}).get(self.selected,{}));self.save();self.log('Pedido enviado: '+self.plugins[self.selected]['name']);self.poll_commands()
    def disable(self):
        if not self.selected:return
        engine=self.need()
        if self.selected not in engine.active:return
        engine.disable(self.selected);self.log('Desativado: '+self.plugins[self.selected]['name'])
    def disable_all(self):
        if self.engine and self.engine.backend.alive():self.engine.disable_all();self.log('Todos os scripts foram desativados.')
    def save(self):
        self.capture();self.remember_layout();tmp=self.configpath.with_suffix('.tmp');tmp.write_text(json.dumps(self.settings,ensure_ascii=False,indent=2),encoding='utf-8');os.replace(tmp,self.configpath)
    def disconnect(self):
        if not self.engine:return
        if self.engine.backend.alive():self.engine.disable_all()
        self.engine.backend.close();self.engine=None;self.status.set('Desconectado');self.log('Desconectado; alterações reversíveis restauradas.')
    def sync(self):
        for ident,p in self.plugins.items():
            state='Indisponível' if p.get('unavailable') else ('Erro: reiniciar' if self.engine and self.engine.poisoned and ident in self.engine.active else ('Ativado' if self.engine and ident in self.engine.active else 'Desativado'))
            self.tree.set(ident,'state',state)
        selection=self.tree.selection();order=self.tree.get_children(self.tree.parent(selection[0])) if selection else ()
        index=order.index(selection[0]) if selection and selection[0] in order else -1
        self.upbtn['state']='normal' if index>0 else 'disabled'
        self.downbtn['state']='normal' if 0<=index<len(order)-1 else 'disabled'
        p=self.plugins.get(self.selected,{})
        active=bool(self.engine and self.selected in self.engine.active)
        self.enablebtn['state']='normal' if self.engine and not self.engine.poisoned and p and not active and not p.get('unavailable') else 'disabled'
        self.disablebtn['state']='normal' if active else 'disabled'
        self.commandbtn['text']=p.get('command',{}).get('label','Executar comando')
        self.commandbtn['state']='normal' if active and p.get('command') else 'disabled'
        if not active or not p.get('command'):self.commandtext.set('')
    def poll_commands(self):
        messages={-1:'A partida ainda não está na fase ativa.',-2:'Mapa sem rotas de trânsito inicializadas.',-3:'Não há slot livre de jogador.',-4:'Não há slot livre de corpo.',-5:'Não há slot livre de veículo.',-6:'Quantidade inválida.',-7:'Falha ao criar o corpo do NPC.',-8:'O primeiro jogador identificado não está conectado. Desative e ative o módulo para escolher novamente.',-9:'O primeiro jogador ainda não tem corpo vivo. Aguarde o respawn.',-10:'O primeiro jogador precisa sair do veículo.',-11:'Posição ou direção inválida do primeiro jogador.'}
        if not self.engine:return
        for ident,state in self.engine.active.items():
            if not state['plugin'].get('command'):continue
            st=self.engine.command_status(ident)
            if st['pending']:text=f'Pedido em processamento: {st["created"]}/{st["quantity"]} NPCs.'
            elif st['status']==2:text=f'Rotina concluiu: {st["created"]}/{st["quantity"]} NPCs criados.'
            elif st['status']<0:text=messages.get(st['status'],'Erro interno.')+f' Criados antes da interrupção: {st["created"]}.'
            else:text='Pronto para receber um pedido.'
            if self.selected==ident:self.commandtext.set(text)
            marker=(st['status'],st['pending'],st['quantity'],st['created'])
            if self.command_seen.get(ident)!=marker:
                self.command_seen[ident]=marker
                if not st['pending'] and st['status']!=0:self.log(text)
    def tick(self):
        if self.engine and not self.engine.backend.alive():self.engine.backend.close();self.engine=None;self.status.set('Servidor encerrado');self.log('O servidor foi encerrado. A próxima sessão começa sem scripts ativados.');self.sync()
        if self.engine:
            try:self.poll_commands()
            except Exception as e:logging.exception('Falha ao consultar comando');self.commandtext.set(str(e))
        self.window.after(1000,self.tick)
    def close(self):
        try:self.save();self.disconnect()
        except Exception as e:messagebox.showerror('Não foi possível fechar com restauração',str(e)+'\nTente desativar novamente ou encerre o servidor antes de fechar o gerenciador.');return
        self.window.destroy()

def main():
    if os.name!='nt':print('Execute este gerenciador no Windows.');return 1
    (ROOT/'logs').mkdir(exist_ok=True)
    logging.basicConfig(filename=ROOT/'logs'/'manager.log',encoding='utf-8',level=logging.INFO,format='%(asctime)s %(levelname)s %(message)s')
    root=tk.Tk();Manager(root);root.mainloop();return 0
if __name__=='__main__':raise SystemExit(main())
