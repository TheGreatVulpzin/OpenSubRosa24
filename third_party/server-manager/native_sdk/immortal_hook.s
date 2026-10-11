.intel_syntax noprefix
.code32
.section .inject_health,"ax"
health_19036:
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov DWORD PTR [esp],eax
call __dll_health
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
mov ecx,DWORD PTR [eax+__server+0x15cbfa8]
jmp __server+0x1903c
health_4946a:
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov DWORD PTR [esp],ebx
call __dll_health
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
mov eax,DWORD PTR [ebx+__server+0x15cbfa8]
jmp __server+0x49470
health_511f3:
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov DWORD PTR [esp],esi
call __dll_health
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
mov edi,DWORD PTR [esi+__server+0x15cbfa8]
jmp __server+0x511f9
health_53bf7:
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov DWORD PTR [esp],esi
call __dll_health
# Change the saved flags only when a lock restored health. Otherwise retain
# the comparison flags from the stock game or the critical-health module.
test eax,eax
je keep_health_flags
cmp DWORD PTR [esi+__server+0x15cbfa8],0
pushfd
pop eax
mov DWORD PTR [ebp+32],eax
keep_health_flags:
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
jle __server+0x53c10
mov edx,DWORD PTR [esp+0x34]
jmp __server+0x53bfd
health_53d21:
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov DWORD PTR [esp],esi
call __dll_health
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
cmp DWORD PTR [esi+__server+0x15cbf94],-1
jmp __server+0x53d28
health_52b99:
pushfd
pushad
mov ebp,esp
sub esp,544
and esp,-16
fxsave [esp]
fninit
fldcw [esp]
sub esp,16
mov DWORD PTR [esp],esi
call __dll_health
add esp,16
fxrstor [esp]
mov esp,ebp
popad
popfd
mov eax,DWORD PTR [esi+__server+0x15cbfa8]
jmp __server+0x52b9f
.section .patch_health_19036,"ax"
jmp health_19036
.fill 1,1,0x90
.section .patch_health_4946a,"ax"
jmp health_4946a
.fill 1,1,0x90
.section .patch_health_511f3,"ax"
jmp health_511f3
.fill 1,1,0x90
.section .patch_health_53bf7,"ax"
jmp health_53bf7
.fill 1,1,0x90
.section .patch_health_53d21,"ax"
jmp health_53d21
.fill 2,1,0x90
.section .patch_health_52b99,"ax"
jmp health_52b99
.fill 1,1,0x90
