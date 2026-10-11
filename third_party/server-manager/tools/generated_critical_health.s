.intel_syntax noprefix
.code32
.equ body0,__server+0x15CBF80
.equ body_vehicle,0x14
.equ body_health,0x28
.equ body_bleeding,0x2B1C
.section .inject,"ax"
.org 0
newmem:
code:
cmp dword ptr [esi+body0+body_health],0xA
jnl originalcode
mov DWORD PTR [esi+body0+body_bleeding],0x1
mov DWORD PTR [esi+body0+body_vehicle],-0x1
originalcode:
cmp dword ptr [esi+body0+body_health],0x9
jmp return
.section .patch_53bf0,"ax"
jmp code
.fill 0x2,1,0x90
return:
.section .patch_4eed4,"ax"
cmp dword ptr [eax+body0+body_health],0x9
.section .patch_206ad,"ax"
cmp dword ptr [eax+body0+body_health],0x9
.section .patch_51321,"ax"
cmp dword ptr [esi+body0+body_health],0x9
.section .patch_514d7,"ax"
cmp dword ptr [esi+body0+body_health],0x9
.section .patch_1907e,"ax"
cmp dword ptr [edi+body0+body_health],0x9
.section .patch_4a4a6,"ax"
cmp dword ptr [esi+body0+body_health],0x9
.section .patch_53a23,"ax"
cmp dword ptr [esi+body0+body_health],0x9
.section .patch_42ae5,"ax"
cmp dword ptr [esi+body0+body_health],0x9
