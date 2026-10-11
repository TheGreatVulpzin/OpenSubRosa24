/* Converted from comandinhos.CT. Uses the combined chat callback on game thread.
 * No extra patch at 4A0E2 and no CLR/CRT dependency.
 */
static int is_extra(const char *cmd) {
    return equal(cmd,"/turn") || equal(cmd,"/herobrine") || equal(cmd,"/tp") || equal(cmd,"/gobase") || equal(cmd,"/sound");
}
static int command_body(int pid) {
    int body=I(player(pid),0x78);
    if(body<0 || body>=512 || I(human(body),0)!=1 || I(human(body),8)!=pid || I(human(body),0x28)<=0)return -1;
    return body;
}
static void appearance(int pid,int body) {
    ((Dirty)(ULONG_PTR)(current->base+0x64a0))(pid);
    if(body>=0)((Dirty)(ULONG_PTR)(current->base+0x6580))(body);
}
static int aim_map(unsigned char *h,float *hit) {
    float from[3],to[3],pitch=F(h,0x64),sy;int j;
    if(!finite(pitch))return 0;
    __asm__("fsin" : "=t"(sy) : "0"(pitch));
    from[0]=F(h,0x288);from[1]=F(h,0x28c);from[2]=F(h,0x290);
    to[0]=from[0]+2000*F(h,0x2c0);
    to[1]=from[1]-2000*sy;
    to[2]=from[2]-2000*F(h,0x2b8);
    for(j=0;j<3;j++)if(!finite(from[j]) || !finite(to[j]))return 0;
    if(to[0]==from[0] && to[1]==from[1] && to[2]==from[2])return 0;
    if(!((int (__cdecl *)(const float *,const float *,float))(ULONG_PTR)(current->base+0x31630))(from,to,0))return 0;
    for(j=0;j<3;j++){hit[j]=*(float *)(ULONG_PTR)(current->base+0x103484a0u+j*4);if(!finite(hit[j]))return 0;}
    return 1;
}
static int teleport(int body,const float *dest) {
    unsigned char *h=human(body);float delta[3];int j,bone,id;
    if(I(h,0x14)!=-1)return 0;
    /* Preflight every position before writing; retain the whole skeleton shape. */
    for(j=0;j<3;j++){delta[j]=dest[j]-F(h,0x48+j*4);if(!finite(dest[j]) || !finite(delta[j]))return 0;}
    for(bone=0;bone<15;bone++)for(j=0;j<6;j++) {
        float v=F(h,0xc8+bone*0xe0+j*4)+delta[j%3];if(!finite(v))return 0;
    }
    for(bone=0;bone<15;bone++)for(j=0;j<3;j++) {
        unsigned off=0xc8+bone*0xe0+j*4;
        F(h,off)+=delta[j];F(h,off+0xc)+=delta[j];F(h,off+0x18)=0;
    }
    for(j=0;j<3;j++){F(h,0x48+j*4)=dest[j];F(h,0x60+j*4)=0;}
    /* Keep attached equipment near the body until native attachment simulation. */
    for(id=0;id<1024;id++) {
        unsigned char *w=item(id);
        if(I(w,0)!=1 || I(w,0x18)!=body)continue;
        for(j=0;j<3;j++)if(finite(F(w,0x48+j*4)+delta[j]) && finite(F(w,0x54+j*4)+delta[j])) {
            F(w,0x48+j*4)+=delta[j];F(w,0x54+j*4)+=delta[j];F(w,0x60+j*4)=0;
        }
    }
    return 1;
}
static int extra_command(int pid,const char *cmd,const char *arg) {
    unsigned char *p=player(pid),*h;int body=command_body(pid),value,j;
    if(equal(cmd,"/turn")) {
        int count=*(int *)(ULONG_PTR)(current->base+0xf437b64u);unsigned char *a;char name[32];int eyes,skin,hair;
        if(count<0 || count>65535 || !integer(arg,&value) || value<0 || value>=count){reply(pid,"Uso: /turn indice da conta salva (comeca em 0)");return 1;}
        a=object(0xf437b68u,0x4c,value);eyes=I(a,0x34);skin=I(a,0x38);hair=I(a,0x3c);
        if(eyes<0 || eyes>3 || skin<0 || skin>7 || hair<0 || hair>3){reply(pid,"A conta salva tem aparencia invalida.");return 1;}
        for(j=0;j<31;j++){name[j]=(char)a[8+j];if(!name[j])break;}
        for(;j<32;j++)name[j]=0;
        for(j=0;j<32;j++)p[4+j]=(unsigned char)name[j];
        I(p,0x23b4)=eyes;I(p,0x23b8)=skin;I(p,0x23bc)=hair;
        appearance(pid,body);reply(pid,"Nome e aparencia copiados. Conta e dinheiro nao mudaram.");return 1;
    }
    if(equal(cmd,"/herobrine")) {
        if(*arg){reply(pid,"Uso: /herobrine");return 1;}
        I(p,0x23b4)=3;appearance(pid,body);reply(pid,"Olhos alterados para o tipo Herobrine.");return 1;
    }
    if(body<0){reply(pid,"Voce precisa de um personagem vivo.");return 1;}
    h=human(body);
    if(equal(cmd,"/gobase")) {
        const float bases[5][3]={{178,25,453},{1084,25,418},{1117,25,955},{349,25,955},{674,25,1347}};
        if(!integer(arg,&value) || value<0 || value>4){reply(pid,"Uso: /gobase 0-4");return 1;}
        if(!teleport(body,bases[value])){reply(pid,"Saia do veiculo e confira a posicao antes de teleportar.");return 1;}
        reply(pid,"Teleportado para a base.");return 1;
    }
    if(equal(cmd,"/sound") && (!integer(arg,&value) || value<0 || value>=48)) {
        reply(pid,"Uso: /sound 0-47");return 1;
    }
    if(equal(cmd,"/tp") && *arg){reply(pid,"Uso: /tp (olhe para uma superficie)");return 1;}
    if(equal(cmd,"/tp") && I(h,0x14)!=-1){reply(pid,"Saia do veiculo antes de teleportar.");return 1;}
    {
        float point[3];
        if(!aim_map(h,point)){reply(pid,"Nenhuma superficie valida na mira.");return 1;}
        if(equal(cmd,"/tp")) {
            point[1]+=1;
            if(!teleport(body,point)){reply(pid,"Posicao invalida para teleporte.");return 1;}
            reply(pid,"Teleportado para o ponto da mira.");
        } else {
            ((void (__cdecl *)(int,const float *,float,float))(ULONG_PTR)(current->base+0x16db0))(value,point,1.0f,0.15f);
            reply(pid,"Som enviado ao ponto da mira.");
        }
    }
    return 1;
}
