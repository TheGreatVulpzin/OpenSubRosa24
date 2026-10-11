#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef SR_BUILD_ID
#error Build with build_death_plugins.py
#endif
const char SR_BuildId[65]=SR_BUILD_ID;
typedef struct {DWORD size,abi,base,reserved;volatile LONG enabled;DWORD mode;} Context;
static Context *volatile current;
static volatile LONG in_flight;
#define I(p,o) (*(int *)((p)+(o)))
static unsigned char *human(int id){return (unsigned char *)(ULONG_PTR)(current->base+0x15cbf80u+0x2b64u*(unsigned)id);}
static unsigned char *player(int id){return (unsigned char *)(ULONG_PTR)(current->base+0xededd00u+0x2408u*(unsigned)id);}
static int sent[512],owner[512],timer[512],reason[512],victim[512],killer[512];
static DWORD expiry[512],last_tick;static int have_tick;
static void reset(void){int i;for(i=0;i<512;i++){sent[i]=0;owner[i]=-2;timer[i]=0;reason[i]=0;victim[i]=-2;killer[i]=-1;}have_tick=0;}
static int active(void){
    DWORD tick;Context *c=current;
    if(!c || !c->enabled)return 0;
    if(*(int *)(ULONG_PTR)(c->base+0x90dc560u)!=2){reset();return 0;}
    tick=*(DWORD *)(ULONG_PTR)(c->base+0x90dc578u);
    if(have_tick && tick<last_tick && last_tick-tick<0x80000000u)reset();
    have_tick=1;last_tick=tick;return 1;
}
static int body_index(unsigned offset){if(offset>=512u*0x2b64u || offset%0x2b64u)return -1;return (int)(offset/0x2b64u);}
static void cause(unsigned offset,int kind,unsigned source){
    int body=body_index(offset),k=-1,count;
    if(!active() || body<0 || kind<1 || kind>5 || I(human(body),0)!=1)return;
    if(kind==1){
        count=*(int *)(ULONG_PTR)(current->base+0x17529a40u);
        if(count>0 && count<=65536 && source%0x5cu==0 && source/0x5cu<(unsigned)count)
            k=*(int *)(ULONG_PTR)(current->base+0x1409ba8u+source);
    }else if(kind==3 && source<1024u*0x8230u && source%0x8230u==0){
        unsigned char *v=(unsigned char *)(ULONG_PTR)(current->base+0x5592600u+source);
        if(I(v,0)!=0)k=I(v,0x14);
    }
    reason[body]=kind;victim[body]=I(human(body),8);killer[body]=(k>=0 && k<256)?k:-1;
    expiry[body]=last_tick+1200;
}
/* Plain bounded copies: percent signs are characters, never a format string. */
static unsigned append(char *out,unsigned n,const char *s,unsigned limit){
    unsigned i;for(i=0;i<limit && s[i] && n<63;i++){
        unsigned char c=(unsigned char)s[i];out[n++]=(c<32 || c==127)?' ':c;
    }out[n]=0;return n;
}
static unsigned name(char *out,unsigned n,int pid){
    if(pid<0 || pid>=256 || I(player(pid),0)!=1 || I(player(pid),0x1a38)!=0)
        return append(out,n,"um NPC",24);
    if(!player(pid)[4])return append(out,n,"um jogador",24);
    return append(out,n,(const char *)player(pid)+4,24);
}
static void death(unsigned offset){
    int body=body_index(offset),pid,kind=0,k=-1,t;unsigned char *h;char text[64];unsigned n=0;
    if(!active() || body<0)return;
    h=human(body);pid=I(h,8);t=I(h,0x24);
    if(I(h,0)!=1){sent[body]=0;reason[body]=0;owner[body]=-2;timer[body]=0;return;}
    if(I(h,0x28)>-5){if(sent[body] || victim[body]!=pid)reason[body]=0;sent[body]=0;owner[body]=pid;timer[body]=t;return;}
    if((pid!=-1 && owner[body]!=pid) || t>timer[body])sent[body]=0;
    owner[body]=pid;timer[body]=t;
    if(!sent[body]){
        sent[body]=1;
        if(victim[body]==pid && (LONG)(expiry[body]-last_tick)>=0){kind=reason[body];k=killer[body];}
        if(current->mode&1u){
            if((kind==1 || kind==3) && k>=0 && k<256 && k!=pid){
                n=name(text,n,k);n=append(text,n,kind==3?" atropelou ":" matou ",12);n=name(text,n,pid);
            }else{
                n=name(text,n,pid);
                n=append(text,n,(kind==1 || kind==3) && k==pid?" se matou":kind==2?" foi punido":kind==3?" morreu por um veiculo":kind==4?" caiu de um lugar alto":kind==5?" caiu para fora do mapa":I(h,0x2b1c)==1?" sangrou ate a morte":" morreu por motivos desconhecidos",40);
            }
            ((void (__cdecl *)(int,const char *,int,int))(ULONG_PTR)(current->base+0x6260u))(0,text,-1,0);
        }
    }
    /* The native death code handles penalties and detachment once. Fast mode
       only advances its timer instead of calling penalties a second time. */
    if(current->mode&2u){if(t>=0xa8c)I(h,0x24)=0xa8b;}
    /* A corpse must not detach a new respawned body or use an invalid owner. */
    if(pid!=-1 && (pid<0 || pid>=256 || I(player(pid),0)!=1 || I(player(pid),0x78)!=body)){I(h,8)=-1;owner[body]=-1;}
}
static int enter(void){
    InterlockedIncrement(&in_flight);
    if(!current || !current->enabled){InterlockedDecrement(&in_flight);return 0;}
    return 1;
}
void __cdecl SR_Cause(unsigned offset,int kind,unsigned source){if(enter()){cause(offset,kind,source);InterlockedDecrement(&in_flight);}}
void __cdecl SR_Death(unsigned offset){if(enter()){death(offset);InterlockedDecrement(&in_flight);}}
DWORD WINAPI SR_Configure(LPVOID arg){Context *c=(Context *)arg;
    if(!c || c->size!=sizeof(*c) || c->abi!=1 || c->mode<1 || c->mode>3)return 1;
    if(c->base!=(DWORD)(ULONG_PTR)GetModuleHandleW(NULL))return 2;
    if(current && current->enabled)return 5;
    if(in_flight)return 6;
    current=c;reset();InterlockedExchange(&c->enabled,1);return 0;
}
DWORD WINAPI SR_PrepareStop(LPVOID arg){unsigned n;if(!arg || arg!=current)return 1;InterlockedExchange(&current->enabled,0);for(n=0;n<3000;n++){if(!in_flight)return 0;Sleep(1);}return 6;}
DWORD WINAPI SR_Stop(LPVOID arg){if(!arg || arg!=current)return 1;InterlockedExchange(&current->enabled,0);return 0;}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID arg){(void)h;(void)reason;(void)arg;return TRUE;}
