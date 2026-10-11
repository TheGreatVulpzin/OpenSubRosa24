.intel_syntax noprefix
.code32
.equ max_z,0x2048
.equ min_z,0x1428
.equ max_x,0x784
.equ min_x,0x608
.equ player0,__server+0xEDEDD00
.equ player_team,0x64
.equ player_body,0x78
.equ body0,__server+0x15CBF80
.equ body_vehicle,0x14
.equ body_health,0x28
.equ body_position,0x48
.equ bodybytes,0x2B64
.equ spawnProtectionPenalty,__server+0x16A20
.section .inject,"ax"
.org 0
newmem:
player_shotted:
cmp dword ptr [eax+__server+0x15CBFB4],0x0
jg originalcode_shotted
cmp dword ptr [ecx+player0+player_team],0x6
je originalcode_shotted
pushad 
cmp DWORD PTR [eax+body0+body_position+0x0],0x45000000
jg attacker_shotted
cmp DWORD PTR [eax+body0+body_position+0x0],0x44b28000
jl attacker_shotted
cmp DWORD PTR [eax+body0+body_position+0x8],0x44440000
jg attacker_shotted
cmp DWORD PTR [eax+body0+body_position+0x8],0x44180000
jl attacker_shotted
push ebp
push edx
call spawnProtectionPenalty
add esp,0x8
jmp cleanup_shotted
attacker_shotted:
mov eax,[edx+player0+player_body]
imul eax,bodybytes
cmp DWORD PTR [eax+body0+body_position+0x0],0x45000000
jg cleanup_shotted
cmp DWORD PTR [eax+body0+body_position+0x0],0x44b28000
jl cleanup_shotted
cmp DWORD PTR [eax+body0+body_position+0x8],0x44440000
jg cleanup_shotted
cmp DWORD PTR [eax+body0+body_position+0x8],0x44180000
jl cleanup_shotted
push ebp
push edx
call spawnProtectionPenalty
add esp,0x8
cleanup_shotted:
popad 
originalcode_shotted:
cmp dword ptr [ecx+__server+0xEDEDD64],0x07
jmp return_shotted
.section .patch_49394,"ax"
jmp player_shotted
.fill 0x2,1,0x90
return_shotted:
.section .inject,"ax"
.org 512
player_runover:
cmp dword ptr [eax+__server+0x15CBFB4],0x0
jg originalcode_runover
cmp dword ptr [ecx+player0+player_team],0x6
je originalcode_runover
pushad 
mov eax,[esp+0x0C]
mov eax,[eax+__server+0x5592614]
mov eax,[eax+player0+player_body]
imul eax,bodybytes
cmp DWORD PTR [eax+body0+body_position+0x0],0x45000000
jg cleanup_runover
cmp DWORD PTR [eax+body0+body_position+0x0],0x44b28000
jl cleanup_runover
cmp DWORD PTR [eax+body0+body_position+0x8],0x44440000
jg cleanup_runover
cmp DWORD PTR [eax+body0+body_position+0x8],0x44180000
jl cleanup_runover
popad 
jmp __server+0x52BF2
cleanup_runover:
popad 
originalcode_runover:
cmp dword ptr [eax+__server+0xEDEDD64],0x07
jmp return_runover
.section .patch_52be0,"ax"
jmp player_runover
.fill 0x2,1,0x90
return_runover:
