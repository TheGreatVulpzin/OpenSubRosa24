.intel_syntax noprefix
.code32
.equ player0,__server+0xEDEDD00
.equ player_zoom_mode,0xAC
.section .inject,"ax"
.org 0
newmem:
player_lost:
add [esi+__server+0xEDEFE68],edi
mov DWORD PTR [esi+player0+player_zoom_mode],0x0
jmp return_player_lost
.section .patch_3d86a,"ax"
jmp player_lost
.fill 0x1,1,0x90
return_player_lost:
.section .inject,"ax"
.org 256
player_spotted:
inc DWORD PTR [esi+__server+0xEDEFE68]
mov DWORD PTR [esi+player0+player_zoom_mode],0x2
jmp return_player_spotted
.section .patch_3d70a,"ax"
jmp player_spotted
.fill 0x1,1,0x90
return_player_spotted:
.section .patch_4fd09,"ax"
.byte 0x0
.section .patch_3d5e4,"ax"
.fill 0x2,1,0x90
