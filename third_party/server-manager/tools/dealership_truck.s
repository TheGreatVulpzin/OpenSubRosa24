.intel_syntax noprefix
.code32
.section .inject,"ax"
handler:
    pushfd
    pushad
    cld
    cmp DWORD PTR [__server+0x90dc560],2
    jne done
    mov ecx,DWORD PTR [__server+0xe2c8bb8]
    cmp ecx,1
    jl done
    cmp ecx,1024
    jg done
    mov esi,OFFSET __server+0xe2c8bfc
shop_loop:
    cmp DWORD PTR [esi],2
    jne next_shop
    mov ebp,DWORD PTR [esi+0x8c8]
    cmp ebp,0
    jl next_shop
    cmp ebp,8
    jg next_shop
    xor ebx,ebx
    lea edi,[esi+0x8cc]
find_truck:
    cmp ebx,ebp
    jge add_truck
    cmp DWORD PTR [edi],9
    je existing_truck
    inc ebx
    add edi,12
    jmp find_truck
add_truck:
    # Use a free catalog slot; if full replace the last offer only.
    cmp ebp,8
    jl append_truck
    sub edi,12
    jmp write_truck
append_truck:
    inc DWORD PTR [esi+0x8c8]
write_truck:
    mov DWORD PTR [edi],9
    mov DWORD PTR [edi+8],0
existing_truck:
    mov eax,DWORD PTR [price]
    mov DWORD PTR [edi+4],eax
next_shop:
    add esi,0x9dc
    loop shop_loop
done:
    popad
    popfd
    jmp __server+0xa2a0
.section .inject_data,"aw"
price: .long 12000
.section .patch_5cbd2,"ax"
    call handler
.section .patch_ac9b2d8,"aw"
    .long 12000
.section .patch_ac9b2dc,"aw"
    .float 5000
