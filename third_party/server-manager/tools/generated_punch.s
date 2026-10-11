.intel_syntax noprefix
.code32
.equ enable_descombulator,0x1
.equ head_damage_multiplier,0x5
.equ punch_cooldown,0x46
.equ human0,__server+0x15CBF80
.equ human_playerid,0x8
.equ human_health,0x28
.equ human_spawnprotectiontime,0x34
.equ human_drunkness,0x44
.equ human_globalpos,0x48
.equ human_inputflags,0xBC
.equ human_punchdelay,0xC4
.equ bone0,0xC8
.equ bone_vel,0x18
.equ bonesize,0xE0
.equ humansize,0x2B64
.equ player0,__server+0xEDEDD00
.equ player_godmode,0x44
.equ playersize,0x2408
.equ lineintersectresult,__server+0x103484A0
.equ lineintersectresult_pos,0x0
.equ lineintersectresult_normal,0x4
.section .inject,"ax"
.org 0
newmem:
handanims:
fld dword ptr [__server+0x5E848]
fstp dword ptr [esp+0x24]
cmp DWORD PTR [esi+human0+human_punchdelay],0x0
jle handanims_skip1
dec DWORD PTR [esi+human0+human_punchdelay]
handanims_skip1:
test DWORD PTR [esi+human0+human_inputflags],0x800
je handanims_return
cmp edi,0x3
jne handanims_otherhand
mov DWORD PTR [esp+0x1C],0x3e19999a
mov DWORD PTR [esp+0x20],0x3dcccccd
mov DWORD PTR [esp+0x24],0xbe99999a
jmp handanims_left_hand
handanims_otherhand:
mov DWORD PTR [esp+0x1C],0xbe19999a
mov DWORD PTR [esp+0x20],0x3dcccccd
mov DWORD PTR [esp+0x24],0xbe99999a
handanims_left_hand:
test DWORD PTR [esi+human0+human_inputflags],0x1
je handanims_right_hand
cmp edi,0x3
jne handanims_right_hand
mov DWORD PTR [esp+0x1C],0x3e4ccccd
mov DWORD PTR [esp+0x20],0x3e4ccccd
mov DWORD PTR [esp+0x24],0xbf800000
handanims_right_hand:
test DWORD PTR [esi+human0+human_inputflags],0x2
je handanims_return
cmp edi,0x6
jne handanims_return
mov DWORD PTR [esp+0x1C],0xbe4ccccd
mov DWORD PTR [esp+0x20],0x3e4ccccd
mov DWORD PTR [esp+0x24],0xbf800000
jmp handanims_return
.section .patch_2fac7,"ax"
jmp handanims
.fill 0x5,1,0x90
handanims_return:
.section .inject,"ax"
.org 512
collision_func:
push ebp
mov ebp,esp
push esi
push ecx
push edi
mov eax,[ebp+0x8]
imul eax,humansize
mov esi,[ebp+0xC]
imul esi,bonesize
mov ecx,[ebp+0x10]
imul ecx,humansize
mov edi,[ebp+0x14]
imul edi,bonesize
test DWORD PTR [ecx+human0+human_inputflags],0x800
je collision_func_return
cmp DWORD PTR [ecx+human0+human_punchdelay],0x0
jg collision_func_return
mov DWORD PTR [ecx+human0+human_spawnprotectiontime],0x0
cmp DWORD PTR [eax+human0+human_spawnprotectiontime],0x0
jg collision_func_return
push eax
mov eax,[eax+human0+human_playerid]
cmp eax,-0x1
je skip_godcheck
imul eax,playersize
cmp DWORD PTR [eax+player0+player_godmode],0x0
je skip_godcheck
pop eax
jmp collision_func_return
skip_godcheck:
pop eax
fld DWORD PTR [ecx+human0+edi+bone0+bone_vel+0x0]
fsub DWORD PTR [eax+human0+esi+bone0+bone_vel+0x0]
fmul st,st(0)
fld DWORD PTR [ecx+human0+edi+bone0+bone_vel+0x4]
fsub DWORD PTR [eax+human0+esi+bone0+bone_vel+0x4]
fmul st,st(0)
fld DWORD PTR [ecx+human0+edi+bone0+bone_vel+0x8]
fsub DWORD PTR [eax+human0+esi+bone0+bone_vel+0x8]
fmul st,st(0)
faddp 
faddp 
fsqrt 
push 0x42c80000
fmul DWORD PTR [esp]
fchs 
add esp,0x4
push 0xc0400000
fld DWORD PTR [esp]
add esp,0x4
fcomip st(1)
jb collision_func_cleanup
cmp DWORD PTR [ebp+0xC],0x2
je collision_func_dmg_head
fiadd DWORD PTR [eax+human0+human_health]
fistp DWORD PTR [eax+human0+human_health]
jmp collision_func_effect
collision_func_dmg_head:
fld st(0)
push 0x40a00000
fmul DWORD PTR [esp]
add esp,0x4
fiadd DWORD PTR [eax+human0+human_health]
fistp DWORD PTR [eax+human0+human_health]
fchs 
fadd st,st(0)
fiadd DWORD PTR [eax+human0+human_drunkness]
fistp DWORD PTR [eax+human0+human_drunkness]
collision_func_effect:
mov DWORD PTR [ecx+human0+human_punchdelay],punch_cooldown
push 0xc1080000
fld DWORD PTR [eax+human0+esi+bone0+bone_vel+0x0]
fmul DWORD PTR [esp]
fsubr DWORD PTR [ecx+human0+edi+bone0+bone_vel+0x0]
fstp DWORD PTR [eax+human0+esi+bone0+bone_vel+0x0]
fld DWORD PTR [eax+human0+esi+bone0+bone_vel+0x4]
fmul DWORD PTR [esp]
fsubr DWORD PTR [ecx+human0+edi+bone0+bone_vel+0x4]
fstp DWORD PTR [eax+human0+esi+bone0+bone_vel+0x4]
fld DWORD PTR [eax+human0+esi+bone0+bone_vel+0x8]
fmul DWORD PTR [esp]
fsubr DWORD PTR [ecx+human0+edi+bone0+bone_vel+0x8]
fstp DWORD PTR [eax+human0+esi+bone0+bone_vel+0x8]
add esp,0x4
push 0x0
lea eax,[ecx+human0+edi+bone0]
push eax
push 0x1
push 0x0
call __server+0x16C40
add esp,0x10
jmp collision_func_return
collision_func_cleanup:
fstp st(0)
collision_func_return:
pop edi
pop ecx
pop esi
pop ebp
ret 
collision:
cmp ebx,0x5
je collision_hand
cmp ebx,0x8
jne collision2
collision_hand:
push ebx
push [esp+0x34]
push edi
push [esp+0x20]
call collision_func
add esp,0x10
jmp collision_originalcode
collision2:
cmp edi,0x5
je collision_hand2
cmp edi,0x8
jne collision_originalcode
collision_hand2:
push edi
push [esp+0x18]
push ebx
push [esp+0x3C]
call collision_func
add esp,0x10
jmp collision_originalcode
collision_originalcode:
cmp ebx,0x0B
je cleanup1
cmp ebx,0x0E
je cleanup1
cmp ebx,0x0A
je cleanup1
cmp ebx,0x0D
je cleanup1
cmp edi,0x0B
je cleanup2
cmp edi,0x0E
je cleanup2
cmp edi,0x0A
je cleanup2
cmp edi,0x0D
je cleanup2
fld st(0)
mov eax,[esp+0x14]
jmp collision_return
cleanup1:
fstp st(0)
jmp __server+0x195D6
cleanup2:
fstp st(0)
jmp __server+0x195BC
.section .patch_1950e,"ax"
jmp collision
.fill 0x1,1,0x90
collision_return:
.section .patch_194b0,"ax"
.fill 0x24,1,0x90
.section .patch_1946e,"ax"
.fill 0x24,1,0x90
