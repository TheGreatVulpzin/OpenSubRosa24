/* Dedicated 024d only. All object calls run on the game thread via SR_Tick. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef SR_BUILD_ID
#error Build with tools/build_market_dll.py
#endif
const char SR_BuildId[65] = SR_BUILD_ID;
typedef struct {
    DWORD size, abi, base, reserved;
    volatile LONG enabled;
    int knife_price, bandage_price, knife_wait, bandage_wait;
    float points[2][3];
    int knife_damage;
    float knife_range;
    int knife_debug;
#ifdef SR_COMBINED
    int show_id, commands_enabled;
#endif
} Context;
static Context *volatile current;
static volatile LONG stopping, cleaned;
#ifdef SR_COMBINED
static int npc_pending=-1, npc_body=-1;
#endif
static int phones[2] = {-1,-1}, owner_seen[2] = {-1,-1};
static DWORD wait[256], blade_wait[512], menu_wait[2], retries[2];
static int waited[256], blade_used[512], notified[2];
static DWORD last_tick;
static int have_tick;
static DWORD debug_at[512];
static int debug_seen[512];
#define I(p,o) (*(int *)((p)+(o)))
#define F(p,o) (*(float *)((p)+(o)))
static unsigned char *object(DWORD rva, unsigned stride, int id) {
    return (unsigned char *)(ULONG_PTR)(current->base + rva + stride*(unsigned)id);
}
static unsigned char *item(int id) {return object(0x91022c0u,0x1ac,id);}
static unsigned char *human(int id) {return object(0x15cbf80u,0x2b64,id);}
static unsigned char *player(int id) {return object(0xededd00u,0x2408,id);}
static DWORD ticks(void) {return *(DWORD *)(ULONG_PTR)(current->base+0x90dc578u);}
static int finite(float v) {return v==v && v>-100000.0f && v<100000.0f;}
typedef int (__cdecl *Create)(int,const float *,const float *,const float *);
typedef void (__cdecl *Delete)(int);
typedef void (__cdecl *Dirty)(int);
typedef void (__cdecl *Message)(int,const char *,int,int);
typedef int (__cdecl *Intersect)(int,const float *,const float *,float);
static int create(int type,const float *pos) {
    const float zero[3]={0,0,0}, rotation[9]={1,0,0,0,1,0,0,0,1};
    return ((Create)(ULONG_PTR)(current->base+0x1b070))(type,pos,zero,rotation);
}
static void dirty(int id) {((Dirty)(ULONG_PTR)(current->base+0x6380))(id);}
static void message(int id,const char *text) {((Message)(ULONG_PTR)(current->base+0x626d))(2,text,id,0);}
static int ours(int n) {
    unsigned char *p; int j;
    if(phones[n]<0 || phones[n]>=1024) return 0;
    p=item(phones[n]);
    if(I(p,0)!=1 || I(p,0xc)!=23 || I(p,0x38)!=3) return 0;
    for(j=0;j<3;j++) if(F(p,0x170+4*j)!=current->points[n][j]) return 0;
    return 1;
}
static void cleanup(void) {
    int n;
#ifdef SR_COMBINED
    npc_pending=-1;
#endif
    for(n=0;n<2;n++) {
        if(ours(n)) ((Delete)(ULONG_PTR)(current->base+0x1b3c0))(phones[n]);
        phones[n]=-1;owner_seen[n]=-1;
    }
    InterlockedExchange(&current->enabled,0);
    InterlockedExchange(&cleaned,1);
}
static unsigned text(char *out,unsigned at,const char *s) {
    while(*s && at<62) out[at++]=*s++;
    out[at]=0;return at;
}
static unsigned number(char *out,unsigned at,unsigned v) {
    char digits[10];unsigned n=0;
    do {digits[n++]=(char)('0'+v%10);v/=10;} while(v);
    while(n && at<62) out[at++]=digits[--n];
    out[at]=0;return at;
}
static void menu(int id) {
    char s[64];unsigned at=text(s,0,"Faca: $");
    at=number(s,at,(unsigned)current->knife_price);at=text(s,at," (24) / Bandagem: $");
    at=number(s,at,(unsigned)current->bandage_price);text(s,at," (12)");message(id,s);
}
static void market(int n,DWORD tick) {
    unsigned char *p,*h,*buyer; int j,body,pid,dial,type,cost,cooldown,id;
    if(!ours(n)) {
        phones[n]=-1;owner_seen[n]=-1;notified[n]=0;
        if((int)(tick-retries[n])<0) return;
        retries[n]=tick+60;
        id=create(23,current->points[n]);
        if(id<0 || id>=1024) return;
        phones[n]=id;p=item(id);I(p,0x38)=3;
        for(j=0;j<3;j++) F(p,0x170+j*4)=current->points[n][j];
        dirty(id);
    }
    p=item(phones[n]);
    for(j=0;j<3;j++) {F(p,0x48+j*4)=current->points[n][j];F(p,0x60+j*4)=0;F(p,0x6c+j*4)=0;}
    I(p,4)=0;I(p,8)=1;
    body=I(p,0x18);
    if(body<0 || body>=512) {owner_seen[n]=-1;return;}
    h=human(body);pid=I(h,8);
    if(I(h,0)!=1 || I(h,0x28)<=0 || pid<0 || pid>=256) return;
    buyer=player(pid);
    if(I(buyer,0)!=1 || I(buyer,0x78)!=body || I(buyer,0x1a38)!=0) return;
    if(owner_seen[n]!=body || !notified[n] || tick-menu_wait[n]>=180) {
        menu(phones[n]);owner_seen[n]=body;notified[n]=1;menu_wait[n]=tick;
    }
    dial=I(p,0x108);
    if(dial!=24 && dial!=12) return;
    I(p,0x108)=0; /* Consume this order even if it fails. */
    {float distance=0;
        for(j=0;j<3;j++) {float d=F(h,0x48+j*4)-current->points[n][j];if(!finite(d)) return;distance+=d*d;}
        if(distance>36) {message(phones[n],"Aproxime-se do Black Market.");return;}
    }
    if(waited[pid] && (int)(tick-wait[pid])<0) {message(phones[n],"Aguarde para comprar novamente.");return;}
    type=dial;cost=dial==24?current->knife_price:current->bandage_price;
    cooldown=dial==24?current->knife_wait:current->bandage_wait;
    if(I(buyer,0x40)<cost) {message(phones[n],"Dinheiro insuficiente.");return;}
    {float pos[3]={current->points[n][0],current->points[n][1]+0.25f,current->points[n][2]};
        id=create(type,pos);
    }
    if(id<0 || id>=1024) {message(phones[n],"Sem espaco para criar o item. Nada foi cobrado.");return;}
    if(type==24) I(item(id),0x38)=1;else I(item(id),0xe0)=4;
    dirty(id);I(buyer,0x40)-=cost;wait[pid]=tick+(DWORD)cooldown;waited[pid]=1;
    message(phones[n],dial==24?"Faca comprada. Pegue o item no ponto de venda.":"Bandagem comprada. Pegue o item no ponto de venda.");
}
/* Debug is opt-in, bounded to one record/second per body. Path is relative
 * to the server's working directory. Does not depend on console visibility. */
static void debug(int body,int hand,int count,int id,int flags,const char *status) {
    char out[256];unsigned at=0;DWORD written;HANDLE file;unsigned char *w=NULL;
    if(!current->knife_debug) return;
    if(debug_seen[body] && ticks()-debug_at[body]<60) return;
    debug_seen[body]=1;debug_at[body]=ticks();
    at=text(out,at,"body=");at=number(out,at,(unsigned)body);
    at=text(out,at," hand=");at=number(out,at,(unsigned)hand);
    at=text(out,at," count=");at=number(out,at,(unsigned)count);
    at=text(out,at," item=");at=number(out,at,(unsigned)id);
    at=text(out,at," flags=");at=number(out,at,(unsigned)flags);
    out[at++]=' ';out[at++]='|';out[at++]=' ';
    /* Menu builders intentionally cap at 62. Reset offsets for extra fields. */
    if(id>=0 && id<1024) w=item(id);
    if(w) {
        char extra[64];unsigned n=0;
        n=text(extra,n,"type=");n=number(extra,n,(unsigned)I(w,0xc));
        n=text(extra,n," state=");n=number(extra,n,(unsigned)I(w,0x38));
        n=text(extra,n," owner=");n=number(extra,n,(unsigned)I(w,0x18));
        n=text(extra,n," slot=");n=number(extra,n,(unsigned)I(w,0x20));
        {unsigned j;for(j=0;j<n && at<190;j++) out[at++]=extra[j];}
        out[at++]=' ';out[at++]='|';out[at++]=' ';
    }
    while(*status && at<248) out[at++]=*status++;
    out[at++]='\r';out[at++]='\n';
    file=CreateFileW(L"sr_black_market_debug.log",FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file!=INVALID_HANDLE_VALUE) {WriteFile(file,out,at,&written,NULL);CloseHandle(file);}
}
#ifdef SR_COMBINED
#include "immortal.h"
#endif
static void blades(DWORD tick) {
    int a,b,j,id;
    for(a=0;a<512;a++) {
        unsigned char *h=human(a),*weapon;float from[3],to[3],axis[3],norm=0,length;
        int flags,hand,count,protected_hit=0;
        if(I(h,0)!=1 || I(h,0x28)<=0) continue;
        flags=I(h,0xbc);hand=(flags&0x10)?1:0;
        /* Native 44BDxx: bit 0x10 selects which 0x24-byte hand slot is active.
         * Count is a stack length, not a boolean. Only the front item is used.
         */
        count=I(h,0x291c+0x24*hand);id=count>0 && count<=8?I(h,0x2920+0x24*hand):-1;
        if(!(flags&1)) {if(flags&2) debug(a,hand,count,id,flags,"use-left-click");continue;}
        if(count<=0 || count>8 || id<0 || id>=1024) {debug(a,hand,count,id,flags,"no-active-hand-item");continue;}
        weapon=item(id);
        if(I(weapon,0)!=1 || I(weapon,0xc)!=24 || I(weapon,0x38)!=1 || I(weapon,0x18)!=a || I(weapon,0x20)!=hand) {debug(a,hand,count,id,flags,"not-active-knife");continue;}
        if(I(h,0x14)!=-1) {debug(a,hand,count,id,flags,"inside-vehicle");continue;}
        if(blade_used[a] && (int)(tick-blade_wait[a])<0) continue;
        for(j=0;j<3;j++) {axis[j]=F(weapon,0x9c+j*4);if(!finite(axis[j])) break;norm+=axis[j]*axis[j];}
        if(j!=3 || !(norm>0.0001f && norm<100.0f)) {debug(a,hand,count,id,flags,"invalid-blade-axis");continue;}
        __asm__("fsqrt" : "=t"(length) : "0"(norm));
        for(j=0;j<3;j++) {
            float pos=F(weapon,0x48+j*4),d=axis[j]/length;
            from[j]=pos+d*0.11f;to[j]=pos-d*current->knife_range;
            if(!finite(from[j]) || !finite(to[j])) break;
        }
        if(j!=3) {debug(a,hand,count,id,flags,"invalid-blade-position");continue;}
        blade_wait[a]=tick+20;blade_used[a]=1;
        /* Swing impulse copied in form from legacy: opposite impulses on
         * forearm/upper arm along head local -Z. Mirrored for the second hand.
         * Applied before native human simulation, so physics advances it.
         */
        for(j=0;j<3;j++) {
            float d=F(h,0x2d0+j*4)*-0.33f;
            unsigned upper=hand?0x380:0x620, lower=hand?0x460:0x700;
            if(finite(d) && finite(F(h,upper+j*4)) && finite(F(h,lower+j*4))) {
                F(h,lower+j*4)+=d;F(h,upper+j*4)-=d*0.5f;
            }
        }
        for(b=0;b<512;b++) {
            unsigned char *target=human(b);int pid=I(target,8),health;
            if(b==a || I(target,0)!=1 || I(target,0x28)<=0) continue;
            if(!((Intersect)(ULONG_PTR)(current->base+0x2f130))(b,from,to,0.05f)) continue;
            if(I(target,0x34)>0 || (pid>=0 && pid<256 && I(player(pid),0)==1 && I(player(pid),0x44)!=0)) {protected_hit=1;continue;}
            health=I(target,0x28)-current->knife_damage;I(target,0x28)=health<0?0:health;I(target,0x2b1c)=1;
#ifdef SR_COMBINED
            SR_LockHealth((unsigned)b*0x2b64u);
#endif
            /* Legacy body impact: replicated effect (2,1), interpreted by client. */
            ((void (__cdecl *)(int,int,const float *,const float *))(ULONG_PTR)(current->base+0x16c40))(2,1,(const float *)(ULONG_PTR)(current->base+0x103484a0u),(const float *)(ULONG_PTR)(current->base+0x103484acu));
            debug(a,hand,count,id,flags,"damage-applied");break;
        }
        if(b==512) debug(a,hand,count,id,flags,protected_hit?"target-protected":"no-body-intersection");
    }
}
static void update_timers(DWORD tick) {
    int n;
    if(have_tick && tick<last_tick && last_tick-tick<0x80000000u) {
#ifdef SR_COMBINED
        immortal_reset();
#endif
        for(n=0;n<256;n++) waited[n]=0;
        for(n=0;n<512;n++) {blade_used[n]=0;debug_seen[n]=0;}
        for(n=0;n<2;n++) {retries[n]=tick;notified[n]=0;}
    }
    last_tick=tick;have_tick=1;
}
/* Separate pre-human callback. The post-human callback remains for purchases
 * so phone digits are processed first. Both run on the same game thread. */
void __cdecl SR_AttackTick(void) {
    Context *ctx=current;DWORD tick;
    if(!ctx || stopping || !ctx->enabled) return;
    tick=ticks();
    update_timers(tick);
#ifdef SR_COMBINED
    immortal_tick();
#endif
    if(*(int *)(ULONG_PTR)(ctx->base+0x90dc560u)==2 && ctx->knife_damage>0) blades(tick);
}
void __cdecl SR_Tick(void) {
    DWORD tick;int n;Context *ctx=current;
    if(!ctx) return;
    if(stopping) {
#ifdef SR_COMBINED
        immortal_reset();
#endif
        if(!cleaned) cleanup();
        return;
    }
    if(!ctx->enabled) return;
    tick=ticks();
    update_timers(tick);
    if(*(int *)(ULONG_PTR)(ctx->base+0x90dc560u)!=2) return;
#ifdef SR_COMBINED
    immortal_tick();
#endif
    for(n=0;n<2;n++) market(n,tick);

}
DWORD WINAPI SR_Configure(LPVOID argument) {
    Context *ctx=(Context *)argument;int i,j;
    const unsigned char a[6]={0x55,0x8b,0xec,0x83,0xe4,0xf8}, b[6]={0x83,0xec,0x40,0x53,0x55,0x56};
    if(!ctx || ctx->size!=sizeof(*ctx) || ctx->abi!=1) return 1;
    if(ctx->base!=(DWORD)(ULONG_PTR)GetModuleHandleW(NULL)) return 2;
    if(ctx->knife_price<0 || ctx->knife_price>1000000 || ctx->bandage_price<0 || ctx->bandage_price>1000000 || ctx->knife_wait<0 || ctx->knife_wait>36000 || ctx->bandage_wait<0 || ctx->bandage_wait>36000 || ctx->knife_damage<0 || ctx->knife_damage>100 || ctx->knife_debug<0 || ctx->knife_debug>1 || !(ctx->knife_range>=0.1f && ctx->knife_range<=3.0f)) return 3;
    for(i=0;i<2;i++) for(j=0;j<3;j++) if(!finite(ctx->points[i][j])) return 3;
    for(i=0;i<6;i++) if(*(unsigned char *)(ULONG_PTR)(ctx->base+0x51060+i)!=a[i] || *(unsigned char *)(ULONG_PTR)(ctx->base+0x5b9b0+i)!=b[i]) return 4;
    if(current && current->enabled && !cleaned) return 5;
#ifdef SR_COMBINED
    if(!ctx->reserved || ctx->show_id<0 || ctx->show_id>1 || ctx->commands_enabled<0 || ctx->commands_enabled>1) return 3;
#endif
    current=ctx;stopping=0;cleaned=0;have_tick=0;
#ifdef SR_COMBINED
    npc_pending=-1;npc_body=-1;immortal_reset();
#endif
    for(i=0;i<2;i++) {phones[i]=-1;owner_seen[i]=-1;notified[i]=0;retries[i]=ticks();}
    for(i=0;i<256;i++) waited[i]=0;
    for(i=0;i<512;i++) {blade_used[i]=0;debug_seen[i]=0;}
    InterlockedExchange(&ctx->enabled,1);return 0;
}
DWORD WINAPI SR_PrepareStop(LPVOID argument) {
    unsigned n;Context *ctx=(Context *)argument;
    if(!ctx || ctx!=current) return 1;
    InterlockedExchange(&stopping,1);
    for(n=0;n<3000;n++) {if(cleaned) return 0;Sleep(1);}
    return 6; /* Leave the hook installed; resume server ticks and retry. */
}
DWORD WINAPI SR_Stop(LPVOID argument) {
    Context *ctx=(Context *)argument;
    if(!ctx || ctx->size!=sizeof(*ctx)) return 1;
    if(ctx==current && !cleaned && (phones[0]>=0 || phones[1]>=0)) return 7;
    InterlockedExchange(&ctx->enabled,0);return 0;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {
    (void)instance;(void)reason;(void)reserved;return TRUE;
}

#ifdef SR_COMBINED
#include "combined_commands.h"
#endif
