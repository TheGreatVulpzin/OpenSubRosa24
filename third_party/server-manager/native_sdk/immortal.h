/* Bound to the issuing player's current body, not a permanent slot privilege.
 * Damage code, hit effects and bleeding flags still execute. Never set godmode. */
static int immortal_owner[512];
static int immortal_on[256],immortal_body[256],immortal_connection[256];
static void immortal_reset(void) {
    int i;for(i=0;i<256;i++){immortal_on[i]=0;immortal_body[i]=-1;}
    for(i=0;i<512;i++)immortal_owner[i]=-1;
}
static int immortal_valid(int pid,int body) {
    unsigned char *p,*h;Context *ctx=current;
    if(!ctx || stopping || !ctx->enabled || !ctx->commands_enabled ||
       *(int *)(ULONG_PTR)(ctx->base+0x90dc560u)!=2 || pid<0 || pid>=256 ||
       body<0 || body>=512 || !immortal_on[pid])return 0;
    if(body!=immortal_body[pid])return 0;
    p=player(pid);h=human(body);
    if(I(p,0)!=1 || I(p,0x1a38)!=0 || I(p,0x34)!=immortal_connection[pid] ||
       I(p,0x78)!=immortal_body[pid] || I(h,0)!=1 || I(h,8)!=pid) {
        immortal_on[pid]=0;return 0;
    }
    return body==immortal_body[pid];
}
int __cdecl SR_LockHealth(unsigned offset) {
    int body,pid;
    if(offset>=512u*0x2b64u || offset%0x2b64u)return 0;
    body=(int)(offset/0x2b64u);pid=immortal_owner[body];
    if(immortal_valid(pid,body)){I(human(body),0x28)=100;return 1;}
    return 0;
}
static void immortal_tick(void) {
    int pid;Context *ctx=current;
    if(!ctx || stopping || !ctx->enabled || !ctx->commands_enabled ||
       *(int *)(ULONG_PTR)(ctx->base+0x90dc560u)!=2) {immortal_reset();return;}
    for(pid=0;pid<256;pid++)if(immortal_on[pid]) {
        int body=immortal_body[pid];
        if(immortal_valid(pid,body))I(human(body),0x28)=100;
    }
}
