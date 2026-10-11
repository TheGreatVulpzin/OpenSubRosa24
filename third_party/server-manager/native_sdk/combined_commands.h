/* Included into black_market.c: callbacks share its context and game thread. */
static unsigned append(char *out,unsigned at,const char *s,unsigned max) {
    unsigned i;for(i=0;i<max && s[i] && at<188;i++) {unsigned char c=s[i];out[at++]=(c<32 || c==127)?' ':c;}return at;
}
static void console(const char *s) {
    DWORD n;unsigned len=0;HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);
    while(s[len] && len<192) len++;
    if(h && h!=INVALID_HANDLE_VALUE) WriteFile(h,s,len,&n,NULL);
}
static void reply(int pid,const char *s) {
    int body=-1;
    if(pid>=0 && pid<256 && I(player(pid),0)==1) {
        body=I(player(pid),0x78);
        if(body<0 || body>=512 || I(human(body),0)!=1 || I(human(body),8)!=pid || I(human(body),0x28)<=0)body=-1;
    }
    if(body>=0)((Message)(ULONG_PTR)(current->base+0x6260))(1,s,body,0);
    console(s);console("\r\n");
}
static int equal(const char *a,const char *b) {while(*a && *a==*b){a++;b++;}return *a==*b;}
static int integer(const char *s,int *value) {
    unsigned v=0,n=0;int sign=1;
    if(*s=='-'){sign=-1;s++;}else if(*s=='+')s++;
    while(*s>='0' && *s<='9'){if(++n>6)return 0;v=v*10+(unsigned)(*s++-'0');}
    while(*s==' ')s++;
    if(!n || *s)return 0;
    *value=(int)v*sign;return 1;
}
static int equip(int body,int type,int slot) {
    unsigned char *h=human(body);int id;
    id=((Create)(ULONG_PTR)(current->base+0x1b070))(type,(float *)(h+0x48),NULL,(float *)(h+0xf8));
    if(id<0 || id>=1024)return 0;
    if(type==24)I(item(id),0x38)=1;
    if(type==12)I(item(id),0xe0)=4;
    if(!((int (__cdecl *)(int,int,int,int))(ULONG_PTR)(current->base+0x1b870))(id,-1,body,slot)) {
        ((Delete)(ULONG_PTR)(current->base+0x1b3c0))(id);return 0;
    }
    dirty(id);return 1;
}
#include "extra_commands.h"
#include "npc_command.h"
static int command(int pid,const char *input) {
    char cmd[20];unsigned n=0;const char *arg;int value=0,body,i;unsigned char *p=player(pid),*h;
    while(input[n] && input[n]!=' ' && n<sizeof(cmd)-1){cmd[n]=input[n];n++;}
    cmd[n]=0;arg=input+n;while(*arg==' ')arg++;
    if(!equal(cmd,"/heal") && !equal(cmd,"/kill") && !equal(cmd,"/item") && !equal(cmd,"/vehicle") && !equal(cmd,"/arm") && !equal(cmd,"/heavydude") && !equal(cmd,"/delh") && !equal(cmd,"/team") && !equal(cmd,"/deactivate") && !equal(cmd,"/bmhelp") && !equal(cmd,"/npc") && !equal(cmd,"/give") && !equal(cmd,"/imortal") && !is_extra(cmd))return 0;
    if(!I(p,0x2c)){reply(pid,"Comando requer admin. Use /admin <senha do servidor>.");return 1;}
    if(equal(cmd,"/bmhelp")){reply(pid,"/heal /kill /item 0-26 /vehicle 0-13 /arm 1-6");reply(pid,"/heavydude /delh /team ID /deactivate ID /npc");reply(pid,"/turn conta /herobrine /tp /gobase 0-4 /sound 0-47");reply(pid,"/give admin ID (0-255) /imortal");return 1;}
    if(equal(cmd,"/give")) {
        const char *target=arg;const char *word="admin";unsigned j=0;
        unsigned char *recipient;
        while(word[j] && target[j]==word[j])j++;
        if(word[j] || target[j]!=' ') {
            reply(pid,"Uso: /give admin ID (0-255)");return 1;
        }
        target+=j;while(*target==' ')target++;
        if(!integer(target,&value) || value<0 || value>=256) {
            reply(pid,"Uso: /give admin ID (0-255)");return 1;
        }
        recipient=player(value);
        if(I(recipient,0)!=1 || I(recipient,0x1a38)!=0 || I(recipient,0x34)<0) {
            reply(pid,"Jogador conectado nao encontrado nesse ID.");return 1;
        }
        if(I(recipient,0x2c)) {
            reply(pid,"Esse jogador ja e administrador.");return 1;
        }
        I(recipient,0x2c)=1;
        ((Dirty)(ULONG_PTR)(current->base+0x64a0))(value);
        reply(value,"Voce recebeu permissao de administrador.");
        reply(pid,"Permissao de administrador concedida.");return 1;
    }
    if(equal(cmd,"/imortal")) {
        if(*arg){reply(pid,"Uso: /imortal (liga/desliga em voce)");return 1;}
        if(immortal_on[pid]) {
            immortal_on[pid]=0;reply(pid,"Imortal desativado.");return 1;
        }
        body=command_body(pid);
        if(*(int *)(ULONG_PTR)(current->base+0x90dc560u)!=2 || body<0 || I(p,0x1a38)!=0 || I(p,0x34)<0) {
            reply(pid,"Voce precisa de um personagem vivo em partida.");return 1;
        }
        immortal_body[pid]=body;immortal_connection[pid]=I(p,0x34);immortal_on[pid]=1;immortal_owner[body]=pid;
        I(human(body),0x28)=100;
        reply(pid,"Imortal ativado: vida fixa em 100; impactos continuam.");return 1;
    }
    if(equal(cmd,"/npc"))return request_npc(pid,arg);
    if(is_extra(cmd))return extra_command(pid,cmd,arg);
    if(equal(cmd,"/team")) {
        if(!integer(arg,&value) || value<0 || value>5){reply(pid,"Uso: /team 0-5");return 1;}
        I(p,0x64)=value;((Dirty)(ULONG_PTR)(current->base+0x64a0))(pid);reply(pid,"Equipe alterada.");return 1;
    }
    if(equal(cmd,"/deactivate")) {
        if(!integer(arg,&value) || value<0 || value>=256){reply(pid,"Uso: /deactivate ID (0-255)");return 1;}
        /* Only connected slots: inactive empty records must never be activated. */
        if(!I(player(value),0) && !player(value)[4]){reply(pid,"Jogador nao encontrado.");return 1;}
        I(player(value),0)=!I(player(value),0);reply(pid,"Estado do jogador alternado.");return 1;
    }
    body=I(p,0x78);
    if(body<0 || body>=512 || !I(human(body),0) || I(human(body),8)!=pid){reply(pid,"Voce precisa de um personagem ativo.");return 1;}
    h=human(body);
    if(equal(cmd,"/heal")){I(h,0x28)=100;I(h,0x2b1c)=0;I(h,0x44)=0;}
    else if(equal(cmd,"/kill")){I(h,0x28)=-5;I(h,0x2b1c)=1;}
    else if(equal(cmd,"/item")) {
        int hand=(I(h,0xbc)&0x10)?1:0;
        if(!integer(arg,&value) || value<0 || value>26){reply(pid,"Uso: /item 0-26");return 1;}
        if(I(h,0x291c+hand*0x24))hand=1-hand;
        if(I(h,0x291c+hand*0x24) || !equip(body,value,hand)){reply(pid,"Sem mao livre ou espaco para criar o item.");return 1;}
    } else if(equal(cmd,"/vehicle")) {
        float pos[3],zero[3]={0,0,0};int id;
        if(!integer(arg,&value) || value<0 || value>13){reply(pid,"Uso: /vehicle 0-13");return 1;}
        for(i=0;i<3;i++){pos[i]=F(h,0x288+i*4)-4*F(h,0x2d0+i*4);if(!finite(pos[i])){reply(pid,"Posicao invalida.");return 1;}}
        id=((int (__cdecl *)(int,const float *,const float *,const float *,int))(ULONG_PTR)(current->base+0x31800))(value,pos,zero,(float *)(h+0x2b8),0);
        if(id<0 || id>=1024){reply(pid,"Sem espaco para criar veiculo.");return 1;}
    } else if(equal(cmd,"/arm")) {
        const int colors[6]={0,2,6,8,10,4};
        if(!integer(arg,&value) || value<1 || value>6){reply(pid,"Uso: /arm 1-6");return 1;}
        ((void (__cdecl *)(int,int,int))(ULONG_PTR)(current->base+0x28190))(body,colors[value-1],4);
        equip(body,12,6);equip(body,12,6);
    } else if(equal(cmd,"/heavydude")) {
        for(i=0;i<15;i++){float v=F(h,0x154+i*0xe0);if(v==v && v>0 && v<100000.0f)F(h,0x154+i*0xe0)=v*100000.0f;}
    } else if(equal(cmd,"/delh")) {
        float from[3],to[3];int target=-1;float best=2;
        for(i=0;i<3;i++){from[i]=F(h,0x288+i*4);to[i]=from[i]-100*F(h,0x2d0+i*4);if(!finite(from[i]) || !finite(to[i])){reply(pid,"Direcao invalida.");return 1;}}
        for(i=0;i<512;i++)if(i!=body && I(human(i),0)==1 && ((Intersect)(ULONG_PTR)(current->base+0x2f130))(i,from,to,0)) {
            float fraction=*(float *)(ULONG_PTR)(current->base+0x103484b8u);if(fraction>=0 && fraction<=1 && fraction<best){best=fraction;target=i;}
        }
        if(target<0){reply(pid,"Nenhum personagem na mira.");return 1;}
        ((Delete)(ULONG_PTR)(current->base+0x18720))(target);
    }
    reply(pid,"Comando executado.");return 1;
}
int __cdecl SR_ChatHook(int pid,char *input) {
    Context *ctx=current;
    if(!ctx)return 0;
    if(ctx->enabled && !stopping && pid>=0 && pid<256 && input && I(player(pid),0)==1) {
        char safe[64],line[192];unsigned n=0,at=0;
        while(n<63 && input[n]){safe[n]=input[n];n++;}safe[n]=0;
        if(ctx->show_id){char id[64];number(id,0,(unsigned)pid);line[at++]='[';at=append(line,at,id,3);line[at++]=']';line[at++]=' ';}
        at=append(line,at,(char *)player(pid)+4,32);line[at++]=':';line[at++]=' ';at=append(line,at,safe,63);line[at++]='\r';line[at++]='\n';line[at]=0;console(line);
        if(ctx->commands_enabled && command(pid,safe))return 1;
    }
    return ((int (__cdecl *)(int,char *))(ULONG_PTR)ctx->reserved)(pid,input);
}
