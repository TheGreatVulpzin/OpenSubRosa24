.intel_syntax noprefix
.code32
.section .inject,"ax"
hook_53d37:
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov eax,DWORD PTR [ebp+4]
mov DWORD PTR [esp],eax
call __dll_death
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
cmp DWORD PTR [esi+__server+0x15cbfa8],-5
jmp __server+0x53d3e
hook_49409:
sub DWORD PTR [ebx+__server+0x15cbfa8],ebp
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov eax,DWORD PTR [ebp+16]
mov DWORD PTR [esp],eax
mov DWORD PTR [esp+4],1
mov DWORD PTR [esp+8],-1
mov eax,DWORD PTR [ebp+4]
mov DWORD PTR [esp+8],eax
call __dll_cause
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
jmp __server+0x4940f
hook_16a6f:
sub DWORD PTR [eax+__server+0x15cbfa8],ecx
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov eax,DWORD PTR [ebp+28]
mov DWORD PTR [esp],eax
mov DWORD PTR [esp+4],2
mov DWORD PTR [esp+8],-1
call __dll_cause
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
jmp __server+0x16a75
hook_52b92:
add DWORD PTR [esi+__server+0x15cbfa8],-120
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov eax,DWORD PTR [ebp+4]
mov DWORD PTR [esp],eax
mov DWORD PTR [esp+4],3
mov DWORD PTR [esp+8],-1
mov eax,DWORD PTR [ebp+48]
mov DWORD PTR [esp+8],eax
call __dll_cause
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
jmp __server+0x52b99
hook_52307:
add DWORD PTR [esi+__server+0x15cbfa8],-120
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov eax,DWORD PTR [ebp+4]
mov DWORD PTR [esp],eax
mov DWORD PTR [esp+4],4
mov DWORD PTR [esp+8],-1
call __dll_cause
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
jmp __server+0x5230e
hook_53be6:
mov DWORD PTR [esi+__server+0x15cbfa8],-100
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov eax,DWORD PTR [ebp+4]
mov DWORD PTR [esp],eax
mov DWORD PTR [esp+4],5
mov DWORD PTR [esp+8],-1
call __dll_cause
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
jmp __server+0x53bf0
.section .patch_53d37,"ax"
jmp hook_53d37
.fill 2,1,0x90
.section .patch_49409,"ax"
jmp hook_49409
.fill 1,1,0x90
.section .patch_16a6f,"ax"
jmp hook_16a6f
.fill 1,1,0x90
.section .patch_52b92,"ax"
jmp hook_52b92
.fill 2,1,0x90
.section .patch_52307,"ax"
jmp hook_52307
.fill 2,1,0x90
.section .patch_53be6,"ax"
jmp hook_53be6
.fill 5,1,0x90
