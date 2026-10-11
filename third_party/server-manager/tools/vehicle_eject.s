# Eject active occupants at the stock collision-damage call site.
# cdecl wrapper: preserve incoming registers/flags and tail-call stock damage.
.intel_syntax noprefix
.code32
.section .inject,"ax"
handler:
    pushfd
    pushad
    mov ebx,DWORD PTR [esp+0x28]
    cmp ebx,1023
    ja done
    imul eax,ebx,0x8230
    cmp DWORD PTR [eax+__server+0x5592600],0
    je done
    # Original table intended to exempt vehicle model 11, not overwrite it.
    cmp DWORD PTR [eax+__server+0x5592604],11
    je done
    mov edx,OFFSET __server+0x15cbf80
    xor edi,edi
body_loop:
    cmp DWORD PTR [edx],1
    jne next_body
    cmp DWORD PTR [edx+0x14],ebx
    jne next_body
    mov DWORD PTR [edx+0x14],-1
    mov eax,DWORD PTR [edx+8]
    cmp eax,255
    ja reset_velocity
    imul eax,eax,0x2408
    cmp DWORD PTR [eax+__server+0xededd00],1
    jne reset_velocity
    cmp DWORD PTR [eax+__server+0xededd78],edi
    jne reset_velocity
    mov DWORD PTR [eax+__server+0xedef744],1
reset_velocity:
    lea esi,[edx+0xe0]
    mov ecx,14
bone_loop:
    mov DWORD PTR [esi],0
    mov DWORD PTR [esi+4],0
    mov DWORD PTR [esi+8],0
    add esi,0xe0
    loop bone_loop
next_body:
    add edx,0x2b64
    inc edi
    cmp edi,512
    jb body_loop
done:
    popad
    popfd
    jmp __server+0x22e20
.section .patch_57060,"ax"
    call handler
