.intel_syntax noprefix
.code32
.equ player0,__server+0xEDEDD00
.equ player_ai_walk_completed,0x1A44
.equ playerbytes,0x2408
.equ vehicle0,__server+0x5592600
.equ vehicle_type,0x4
.equ vehiclebytes,0x8230
.equ body0,__server+0x15CBF80
.equ body_playerid,0x8
.equ body_vehicle,0x14
.equ body_part,0xC8
.equ body_part_position,0x0
.equ body_part_velocity,0x18
.equ body_part_bytes,0xE0
.equ bodybytes,0x2B64
.section .inject,"ax"
.org 0
newmem:
pushad 
mov ebx,[esp+0x20]
imul edx,ebx,vehiclebytes
mov DWORD PTR [edx+vehicle0+vehicle_type],0xB
je cleanup
mov ecx,0x80
xor edx,edx
player_loop:
cmp [edx+body0+body_vehicle],ebx
jne player_next
mov DWORD PTR [edx+body0+body_vehicle],-0x1
mov eax,[edx+body0+body_playerid]
imul eax,playerbytes
mov DWORD PTR [eax+player0+player_ai_walk_completed],0x1
xor eax,eax
reset_vol_loop:
mov DWORD PTR [edx+eax+body0+body_part+body_part_velocity+0x0],0x0
mov DWORD PTR [edx+eax+body0+body_part+body_part_velocity+0x4],0x0
mov DWORD PTR [edx+eax+body0+body_part+body_part_velocity+0x8],0x0
add eax,body_part_bytes
cmp eax,0xC40
jl reset_vol_loop
player_next:
add edx,bodybytes
dec ecx
cmp ecx,0x0
jge player_loop
cleanup:
popad 
call __server+0x22E20
jmp return
.section .patch_57060,"ax"
jmp newmem
return:
