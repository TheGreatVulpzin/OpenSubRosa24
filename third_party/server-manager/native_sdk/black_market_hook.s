.intel_syntax noprefix
.code32
.section .inject,"ax"
market_hook:
    pushfd
    pushad
    mov ebp, esp
    sub esp, 544
    and esp, -16
    fxsave [esp]
    fninit
    fldcw [esp]
    call __dll_tick
    fxrstor [esp]
    mov esp, ebp
    popad
    popfd
    jmp __server+0x4c8c0
.section .patch_market,"ax"
    call market_hook

.section .inject,"ax"
attack_hook:
    pushfd
    pushad
    mov ebp, esp
    sub esp, 544
    and esp, -16
    fxsave [esp]
    fninit
    fldcw [esp]
    call __dll_attack
    fxrstor [esp]
    mov esp, ebp
    popad
    popfd
    jmp __server+0x51060
.section .patch_attack,"ax"
    call attack_hook

.section .inject_npc,"ax"
npc_hook:
    pushfd
    pushad
    mov ebp, esp
    sub esp, 544
    and esp, -16
    fxsave [esp]
    fninit
    fldcw [esp]
    call __dll_npc
    fxrstor [esp]
    mov esp, ebp
    popad
    popfd
    jmp __server+0x33650
.section .patch_npc,"ax"
    call npc_hook
