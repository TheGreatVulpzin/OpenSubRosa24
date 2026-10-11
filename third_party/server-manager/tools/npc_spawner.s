# Pedestrian NPC command. Runs before per-frame caches/physics are built.
.intel_syntax noprefix
.code32
.section .inject,"ax"
handler:
    pushfd
    pushad
    mov eax,esp
    sub esp,528
    and esp,-16
    mov DWORD PTR [esp+512],eax
    fxsave [esp]
    cld
    # Latch the first connected human independently of player pool slot order.
    cmp DWORD PTR [latched],0
    jne check_request
    cmp DWORD PTR [__server+0x1c4afc0],1
    jl check_request
    mov eax,DWORD PTR [__server+0x15e39030]
    cmp eax,255
    ja check_request
    mov DWORD PTR [reference_id],eax
    mov eax,DWORD PTR [__server+0x15e39020]
    mov DWORD PTR [reference_ip],eax
    movzx eax,WORD PTR [__server+0x15e39024]
    mov DWORD PTR [reference_port],eax
    mov DWORD PTR [latched],1
check_request:
    cmp DWORD PTR [request],0
    je restore
    cmp DWORD PTR [__server+0x90dc560],2
    jne bad_state
    cmp DWORD PTR [quantity],1
    jl bad_quantity
    cmp DWORD PTR [quantity],8
    jg bad_quantity
    cmp DWORD PTR [latched],1
    jne no_reference
    mov ecx,DWORD PTR [__server+0x1c4afc0]
    cmp ecx,1
    jl no_reference
    cmp ecx,32
    jg no_reference
    mov edi,OFFSET __server+0x15e39020
find_connection:
    mov eax,DWORD PTR [reference_ip]
    cmp DWORD PTR [edi],eax
    jne next_connection
    movzx eax,WORD PTR [edi+4]
    cmp eax,DWORD PTR [reference_port]
    jne next_connection
    mov eax,DWORD PTR [edi+0x10]
    cmp eax,DWORD PTR [reference_id]
    je reference_connected
next_connection:
    add edi,0xf170
    loop find_connection
    jmp no_reference
reference_connected:
    cmp eax,255
    ja no_reference
    imul esi,eax,0x2408
    add esi,OFFSET __server+0xEDEDD00
    cmp DWORD PTR [esi],1
    jne no_reference
    cmp DWORD PTR [esi+0x1a38],0
    jne no_reference
    mov edx,DWORD PTR [esi+0x78]
    cmp edx,511
    ja no_reference_body
    imul edi,edx,0x2b64
    add edi,OFFSET __server+0x15CBF80
    cmp DWORD PTR [edi],1
    jne no_reference_body
    cmp DWORD PTR [edi+8],eax
    jne no_reference_body
    cmp DWORD PTR [edi+0x28],0
    jle no_reference_body
    cmp DWORD PTR [edi+0x14],-1
    jne reference_in_car
    # Reject NaN/Inf or implausible coordinates before any allocation.
    lea edx,[edi+0x48]
    mov ecx,3
check_position:
    mov eax,DWORD PTR [edx]
    and eax,0x7fffffff
    cmp eax,0x46a00000
    ja bad_position
    add edx,4
    loop check_position
    mov eax,DWORD PTR [esi+0x8c]
    mov DWORD PTR [yaw],eax
    and eax,0x7fffffff
    cmp eax,0x42c80000
    ja bad_position
    # FXSAVE does not clear a live x87 stack. Use a clean temporary stack.
    fninit
    fldcw WORD PTR [esp]
    fld DWORD PTR [yaw]
    fsincos
    fstp DWORD PTR [cosine]
    fstp DWORD PTR [sine]
    fild DWORD PTR [created]
    fmul DWORD PTR [spacing]
    fadd DWORD PTR [distance]
    fstp DWORD PTR [offset_distance]
    fld DWORD PTR [sine]
    fmul DWORD PTR [offset_distance]
    fadd DWORD PTR [edi+0x48]
    fstp DWORD PTR [position]
    fld DWORD PTR [edi+0x4c]
    fadd DWORD PTR [height_offset]
    fstp DWORD PTR [position+4]
    fld DWORD PTR [cosine]
    fmul DWORD PTR [offset_distance]
    fchs
    fadd DWORD PTR [edi+0x50]
    fstp DWORD PTR [position+8]
    # Upright rotation, matching the heading of the reference player.
    mov eax,DWORD PTR [cosine]
    mov DWORD PTR [matrix],eax
    mov DWORD PTR [matrix+32],eax
    mov eax,DWORD PTR [sine]
    mov DWORD PTR [matrix+8],eax
    xor eax,0x80000000
    mov DWORD PTR [matrix+24],eax
    mov eax,OFFSET __server+0x15CBF80
    mov ecx,512
find_body:
    cmp DWORD PTR [eax],0
    je body_available
    add eax,0x2b64
    loop find_body
    jmp no_body
body_available:
    call __server+0x28020
    cmp eax,255
    ja no_player
    mov ebp,eax
    imul ebx,eax,0x2408
    add ebx,OFFSET __server+0xEDEDD00
    mov DWORD PTR [ebx+4],0x0043504e
    mov DWORD PTR [ebx+0x64],6
    mov eax,DWORD PTR [yaw]
    mov DWORD PTR [ebx+0x8c],eax
    # Native allocator initialized appearance, connection=-1, body=-1.
    push ebp
    push OFFSET matrix
    push OFFSET position
    call __server+0x50ae0
    add esp,12
    cmp eax,511
    ja rollback_player
    mov DWORD PTR [ebx+0x78],eax
    imul edi,eax,0x2b64
    add edi,OFFSET __server+0x15CBF80
    cmp DWORD PTR [edi],1
    jne rollback_player
    cmp DWORD PTR [edi+8],ebp
    jne rollback_player
    cmp DWORD PTR [edi+0x14],-1
    jne rollback_player
    # Initialize input angles and world center for the first frame.
    mov eax,DWORD PTR [yaw]
    mov DWORD PTR [edi+0xac],eax
    mov DWORD PTR [edi+0x60],eax
    mov DWORD PTR [edi+0xb0],0
    mov eax,DWORD PTR [position]
    mov DWORD PTR [edi+0x48],eax
    mov eax,DWORD PTR [position+4]
    mov DWORD PTR [edi+0x4c],eax
    mov eax,DWORD PTR [position+8]
    mov DWORD PTR [edi+0x50],eax
    # Pedestrian native AI, with empty route list (idle, no driving route).
    mov DWORD PTR [ebx+0x1a44],0
    mov DWORD PTR [ebx+0x1a48],0
    mov DWORD PTR [ebx+0x1a38],1
    push ebp
    call __server+0x64a0
    add esp,4
    inc DWORD PTR [created]
    dec DWORD PTR [remaining]
    cmp DWORD PTR [remaining],0
    jg restore
    mov DWORD PTR [status],2
    mov DWORD PTR [request],0
    jmp restore
rollback_player:
    # No broad pool writes: roll back only the slot just allocated by us.
    mov DWORD PTR [ebx],0
    mov DWORD PTR [status],-7
    jmp cancel
bad_state:
    mov DWORD PTR [status],-1
    jmp cancel
no_reference:
    mov DWORD PTR [status],-8
    jmp cancel
no_reference_body:
    mov DWORD PTR [status],-9
    jmp cancel
reference_in_car:
    mov DWORD PTR [status],-10
    jmp cancel
bad_position:
    mov DWORD PTR [status],-11
    jmp cancel
no_player:
    mov DWORD PTR [status],-3
    jmp cancel
no_body:
    mov DWORD PTR [status],-4
    jmp cancel
bad_quantity:
    mov DWORD PTR [status],-6
cancel:
    mov DWORD PTR [request],0
restore:
    fxrstor [esp]
    mov esp,DWORD PTR [esp+512]
    popad
    popfd
    jmp __server+0x33570

.section .inject_data,"aw"
.balign 4
request: .long 0
quantity: .long 1
remaining: .long 0
created: .long 0
status: .long 0
reference_id: .long -1
reference_ip: .long 0
reference_port: .long 0
latched: .long 0
position: .float 0,0,0
matrix: .float 1,0,0,0,1,0,0,0,1
yaw: .float 0
distance: .float 3
spacing: .float 1.5
height_offset: .float 0.15
sine: .float 0
cosine: .float 1
offset_distance: .float 3
.section .patch_5cb79,"ax"
    call handler
