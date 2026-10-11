/* Chat queues one pedestrian; allocation occurs before native connection/AI
 * caches at 5CB7E. The GUI spawner retains its separate 5CB79 hook. */
static const char *npc_reference(int pid,int body) {
    unsigned char *p=player(pid),*h;
    if(*(int *)(ULONG_PTR)(current->base+0x90dc560u)!=2)
        return "O servidor precisa estar em partida.";
    if(I(p,0)!=1 || I(p,0x1a38)!=0 || command_body(pid)!=body || body<0)
        return "Voce precisa de um personagem vivo.";
    h=human(body);
    if(I(h,0x14)!=-1)return "Saia do veiculo antes de usar /npc.";
    if(!finite(F(p,0x8c)) || F(p,0x8c)<-100 || F(p,0x8c)>100)
        return "Direcao invalida para criar NPC.";
    for(int j=0;j<3;j++)if(!finite(F(h,0x48+j*4)) ||
        F(h,0x48+j*4)<-20480 || F(h,0x48+j*4)>20480)
        return "Posicao invalida para criar NPC.";
    return NULL;
}
static int request_npc(int pid,const char *arg) {
    int body=command_body(pid);const char *error;
    if(*arg){reply(pid,"Uso: /npc");return 1;}
    error=npc_reference(pid,body);
    if(error){reply(pid,error);return 1;}
    if(npc_pending>=0){reply(pid,"Aguarde o pedido de NPC pendente.");return 1;}
    npc_body=body;npc_pending=pid;
    reply(pid,"Pedido de NPC recebido.");return 1;
}
void __cdecl SR_NpcTick(void) {
    Context *ctx=current;int pid,body,id,b,j;unsigned char *p,*h,*np,*nh;
    float pos[3],matrix[9]={1,0,0,0,1,0,0,0,1},yaw,s,c;
    const char *error;
    if(!ctx || stopping || !ctx->enabled || !ctx->commands_enabled) {
        npc_pending=-1;return;
    }
    pid=npc_pending;body=npc_body;npc_pending=-1;
    if(pid<0 || pid>=256)return;
    p=player(pid);
    /* Revalidate after chat processing: no allocation for a stale character. */
    if(I(p,0)!=1)return;
    if(!I(p,0x2c)){reply(pid,"Comando requer admin.");return;}
    error=npc_reference(pid,body);
    if(error){reply(pid,error);return;}
    h=human(body);yaw=F(p,0x8c);
    __asm__("fsincos" : "=t"(c),"=u"(s) : "0"(yaw));
    pos[0]=F(h,0x48)+s*3;pos[1]=F(h,0x4c)+0.15f;
    pos[2]=F(h,0x50)-c*3;
    for(j=0;j<3;j++)if(!finite(pos[j])){reply(pid,"Posicao invalida.");return;}
    matrix[0]=c;matrix[8]=c;matrix[2]=s;matrix[6]=-s;
    for(b=0;b<512;b++)if(I(human(b),0)==0)break;
    if(b==512){reply(pid,"Sem espaco para criar personagem NPC.");return;}
    for(id=0;id<256;id++)if(I(player(id),0)==0)break;
    if(id==256){reply(pid,"Sem slot livre para NPC.");return;}
    id=((int (__cdecl *)(void))(ULONG_PTR)(ctx->base+0x28020))();
    if(id<0 || id>=256){reply(pid,"Sem slot livre para NPC.");return;}
    np=player(id);I(np,4)=0x0043504e;I(np,0x64)=6;F(np,0x8c)=yaw;
    b=((int (__cdecl *)(const float *,const float *,int))(ULONG_PTR)(ctx->base+0x50ae0))(pos,matrix,id);
    if(b<0 || b>=512){I(np,0)=0;reply(pid,"Falha ao criar o corpo do NPC.");return;}
    nh=human(b);
    if(I(nh,0)!=1 || I(nh,8)!=id || I(nh,0x14)!=-1) {
        /* Only remove a body demonstrably owned by this new slot. */
        if(I(nh,0)==1 && I(nh,8)==id)
            ((Delete)(ULONG_PTR)(ctx->base+0x18720))(b);
        I(np,0)=0;reply(pid,"Falha ao inicializar NPC.");return;
    }
    I(np,0x78)=b;F(nh,0xac)=yaw;F(nh,0x60)=yaw;F(nh,0xb0)=0;
    for(j=0;j<3;j++)F(nh,0x48+j*4)=pos[j];
    I(np,0x1a44)=0;I(np,0x1a48)=0;I(np,0x1a38)=1;
    ((Dirty)(ULONG_PTR)(ctx->base+0x64a0))(id);
    reply(pid,"NPC criado a 3 metros a sua frente.");
}
