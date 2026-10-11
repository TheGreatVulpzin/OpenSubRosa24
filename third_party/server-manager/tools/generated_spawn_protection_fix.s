.intel_syntax noprefix
.code32
.equ vehicle0,__server+0x5592600
.equ vehicle_velocity,0x68
.equ vehiclebytes,0x8230
.section .inject,"ax"
.org 0
newmem:
lenght:
fld DWORD PTR [esp+0x4]
fmul DWORD PTR [esp+0x4]
fstp DWORD PTR [esp+0x4]
fld DWORD PTR [esp+0x8]
fmul DWORD PTR [esp+0x8]
fstp DWORD PTR [esp+0x8]
fld DWORD PTR [esp+0xC]
fmul DWORD PTR [esp+0xC]
fstp DWORD PTR [esp+0xC]
fld DWORD PTR [esp+0x4]
fadd DWORD PTR [esp+0x8]
fadd DWORD PTR [esp+0xC]
fstp DWORD PTR [esp+0x4]
mov eax,[esp+0x4]
ret 
code:
mov ecx,eax
mov eax,[esp+0xC]
push ecx
push ebx
push [eax+vehicle0+vehicle_velocity+0x0]
push [eax+vehicle0+vehicle_velocity+0x4]
push [eax+vehicle0+vehicle_velocity+0x8]
call lenght
add esp,0xC
pop ebx
pop ecx
cmp eax,0x3ca3d70a
jl return
originalcode:
push 0x69
push ecx
call __server+0x16A20
add esp,0x08
jmp return
.section .patch_52c01,"ax"
jmp code
.fill 0x6,1,0x90
return:
