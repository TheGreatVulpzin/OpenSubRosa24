.intel_syntax noprefix
.code32
.section .inject,"ax"
rpm_hook:
    # Original load in native engine torque function, ECX=vehicle*8230.
    fld DWORD PTR [ecx+__server+0x5598450]
    pushfd
    cmp DWORD PTR [ecx+__server+0x5592604],9
    jne rpm_done
    fdiv DWORD PTR [speed_multiplier]
rpm_done:
    popfd
    jmp __server+0x23fb0
.balign 16
force_hook:
    # Native torque result in ST(0), EDX=vehicle*8230.
    pushfd
    cmp DWORD PTR [edx+__server+0x5592604],9
    jne force_done
    fmul DWORD PTR [force_multiplier]
force_done:
    popfd
    fmul DWORD PTR [edx+__server+0x5598454]
    jmp __server+0x325c1
.section .inject_data,"aw"
speed_multiplier: .float 2
force_multiplier: .float 2
.section .patch_23faa,"ax"
    jmp rpm_hook
    nop
.section .patch_325bb,"ax"
    jmp force_hook
    nop
