.intel_syntax noprefix
.code32
.section .inject,"ax"
.org 0
newmem:
cmp eax,0x09
jnl truck
xor edi,edi
jmp __server+0x57F1
truck:
cmp eax,0x0A
jnl return
mov edi,0x9
jmp __server+0x57F1
.section .patch_57b1,"ax"
jmp newmem
.fill 0x4,1,0x90
return:
.section .inject,"ax"
.org 256
update:
call __server+0x5700
ret 
.section .patch_ac9b2d8,"ax"
.long 0x2ee0
.section .patch_ac9b2dc,"ax"
.long 0x459c4000
