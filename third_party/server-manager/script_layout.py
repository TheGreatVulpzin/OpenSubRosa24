"""Persist virtual folders without moving plugin or DLL files on disk."""
def normalize_layout(settings, plugins):
    seen=set();folders=set()
    def walk(nodes,depth=0):
        result=[]
        if not isinstance(nodes,list) or depth>64:return result
        for node in nodes:
            if not isinstance(node,dict):continue
            ident=node.get('id')
            if not isinstance(ident,str):continue
            if 'children' in node:
                if not ident.startswith('folder:') or ident in folders or ident in plugins:continue
                folders.add(ident)
                name=node.get('name')
                result.append({'id':ident,'name':name.strip() if isinstance(name,str) and name.strip() else 'Pasta',
                               'open':node.get('open',True) is not False,'children':walk(node['children'],depth+1)})
            elif ident in plugins and ident not in seen:
                seen.add(ident);result.append({'id':ident})
        return result
    layout=settings.get('script_layout')
    if not isinstance(layout,list):
        order=settings.get('script_order',[])
        layout=[{'id':ident} for ident in order] if isinstance(order,list) else []
    result=walk(layout)
    result.extend({'id':ident} for ident in plugins if ident not in seen)
    return result
