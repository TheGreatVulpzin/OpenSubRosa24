typedef unsigned int U;
extern unsigned char pe_blob[], pe_end[], npc_blob[], npc_end[], data_blob[], data_end[];
#define W(a) (*(volatile U *)(a))
#define F(a) (*(volatile float *)(a))
static int sc1(int n,U b){int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(b):"memory");return r;}
static int sc3(int n,U b,U c,U d){int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(b),"c"(c),"d"(d):"memory");return r;}
static void copy(void *d,const void *s,U n){unsigned char *x=d;const unsigned char*y=s;while(n--)*x++=*y++;}
static void *fill(void *d,int v,U n){unsigned char*x=d;while(n--)*x++=v;return d;}
static void patch(U addr,U fn){*(unsigned char*)addr=0xe9;W(addr+1)=fn-addr-5;}
static int rnd(void){return 0;}
static void noop(void){}
__attribute__((naked)) static void ci_cos(void){__asm__("fcos; ret");}
__attribute__((naked)) static void ci_sin(void){__asm__("fsin; ret");}
__attribute__((naked)) static void ci_sqrt(void){__asm__("fsqrt; ret");}
__attribute__((naked)) static void ci_atan(void){__asm__("fpatan; ret");}
static void map(U a,U n){U arg[]={a,n,7,0x4032,-1,0};if((U)sc1(90,(U)arg)!=a)sc1(1,99);}
static void check(int ok,U code){if(!ok)sc1(1,code);}
static void tick(void){((void(*)(void))0x50000000)();}
static U P(U i){return 0xf1edd00+i*0x2408;}
static U H(U i){return 0x19cbf80+i*0x2b64;}
static void request(U n){W(0x50010000)=1;W(0x50010004)=n;W(0x50010008)=n;W(0x5001000c)=0;W(0x50010010)=0;}
void _start(void){
 map(0x400000,0x25f3b000);map(0x50000000,0x20000);
 copy((void*)0x400000,pe_blob,pe_end-pe_blob);copy((void*)0x50000000,npc_blob,npc_end-npc_blob);copy((void*)0x50010000,data_blob,data_end-data_blob);
 patch(0x45ce7c,(U)fill);patch(0x45ce18,(U)noop);patch(0x433570,(U)noop);W(0x45e070)=(U)rnd;W(0x45e07c)=(U)copy;W(0x45e080)=(U)fill;W(0x45e088)=(U)ci_cos;W(0x45e08c)=(U)ci_sin;W(0x45e090)=(U)ci_sqrt;W(0x45e094)=(U)ci_atan;
 __asm__ volatile("fninit");
 W(0x94dc560)=2;W(0x7a1ea60)=1;
 // Geometry is synthetic. Initialize bone masses/inertias, no roads/vehicles.
 for(U i=0;i<16;i++){U s=0xf670280+64*i;F(s+0x28)=1;F(s+0x2c)=1;}
 W(P(7))=1;W(P(7)+0x78)=12;W(P(7)+0x1a38)=0;W(H(12))=1;W(H(12)+8)=7;W(H(12)+0x28)=100;W(H(12)+0x14)=-1;
 F(H(12)+0x48)=100;F(H(12)+0x4c)=20;F(H(12)+0x50)=200;F(P(7)+0x8c)=0;
 W(0x204afc0)=2;W(0x16239020)=123;*(unsigned short*)0x16239024=77;W(0x16239030)=7;
 W(0x162481a0)=124;*(unsigned short*)0x162481a4=78;W(0x162481b0)=3;
 tick();check(W(0x50010000)==0,1);check(W(0x50010014)==7,2);
 request(1);tick();check(W(0x50010010)==2,3);check(W(0x5001000c)==1,4);
 check(W(P(0))==1,5);check(W(P(0)+0x1a38)==1,6);check(W(P(0)+0x78)==0,7);check(W(H(0))==1,8);check(W(H(0)+8)==0,9);check(W(H(0)+0x14)==-1,10);check(W(H(0)+0x28)==100,11);
 check(F(H(0)+0x2870)==100,12);check(F(H(0)+0x2878)>196.99f && F(H(0)+0x2878)<197.01f,13);check(F(H(0)+0x2874)>20.14f && F(H(0)+0x2874)<20.16f,14);
 // Player event advertises valid body and AI, body event generated natively.
 check(W(0xf837318)==2,15);check(W(0x97dd880)==17,16);check(W(0x97dd900)==7,17);check(W(0x97dd924)==0,18);
 // Native pedestrian AI can run with an empty path list in a synthetic world.
 ((void(*)(int))0x44f720)(0);
 request(2);tick();tick();check(W(0x5001000c)==2,19);check(W(0x50010000)==0,20);check(W(P(1)+0x78)==1,21);check(W(P(2)+0x78)==2,22);check(F(H(2)+0x2878)<195.51f && F(H(2)+0x2878)>195.49f,23);
 W(H(12)+0x28)=0;request(1);tick();check((int)W(0x50010010)==-9,24);W(H(12)+0x28)=100;
 W(H(12)+0x14)=0;request(1);tick();check((int)W(0x50010010)==-10,25);W(H(12)+0x14)=-1;
 // Heading +pi/2 places the next NPC on +X; preserve six live x87 values.
 F(P(7)+0x8c)=1.57079632679f;request(1);
 __asm__ volatile("fld1; fld1; fld1; fld1; fld1; fld1");tick();
 float saved;__asm__ volatile("fstps %0; fstp %%st(0); fstp %%st(0); fstp %%st(0); fstp %%st(0); fstp %%st(0)":"=m"(saved));
 check(saved==1,27);check(W(0x50010010)==2,28);check(F(H(3)+0x2870)>102.99f && F(H(3)+0x2870)<103.01f,29);check(F(H(3)+0x2878)>199.99f && F(H(3)+0x2878)<200.01f,30);
 for(U i=0;i<512;i++)W(H(i))=1;
 request(1);tick();check((int)W(0x50010010)==-4,31);
 for(U i=0;i<512;i++)if(i!=12)W(H(i))=0;
 for(U i=0;i<256;i++)W(P(i))=1;
 request(1);tick();check((int)W(0x50010010)==-3,32);
 W(0x204afc0)=0;request(1);tick();check((int)W(0x50010010)==-8,26);
 const char msg[]="Native pedestrian spawn: creation, position, AI, events, repeat and rejection checks OK\n";sc3(4,1,(U)msg,sizeof(msg)-1);sc1(1,0);
}
