; NixDOS 2 boot sector (NASM, 16-bit real mode -> 32-bit protected mode)
;
; 1. Normalise segments and set up a stack below 0x7C00.
; 2. Enable the A20 address line and VERIFY it with a memory wrap-around
;    test (BIOS INT 15h, fast gate 0x92, keyboard controller - in order).
; 3. Load the kernel from LBA 1 to 0x10000 using INT 13h extensions,
;    retrying failed reads.
; 4. Switch to 32-bit protected mode with a flat GDT and jump to the kernel.

BITS 16
ORG 0x7C00

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 128
%endif

KERNEL_SEG      equ 0x1000          ; kernel is loaded at 0x1000:0000 = 0x10000
CHUNK           equ 32              ; sectors per INT 13h call (16 KiB)

start:
    jmp 0x0000:init                 ; force CS = 0

init:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    cld

    mov [boot_drive], dl

    mov si, msg_boot
    call print

    call enable_a20

    ; --- make sure the BIOS supports LBA disk access -----------------------
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc no_lba
    cmp bx, 0xAA55
    jne no_lba

    ; --- load the kernel -----------------------------------------------------
.read_loop:
    mov ax, [remaining]
    test ax, ax
    jz .loaded
    cmp ax, CHUNK
    jbe .count_ok
    mov ax, CHUNK
.count_ok:
    mov [chunk], ax
    mov byte [retries], 3
.retry:
    mov ax, [chunk]
    mov [dap_count], ax
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jnc .read_ok
    dec byte [retries]
    jz disk_error
    xor ah, ah                      ; reset disk system and try again
    mov dl, [boot_drive]
    int 0x13
    jmp .retry
.read_ok:
    mov ax, [chunk]
    sub [remaining], ax
    add [dap_lba], ax
    adc word [dap_lba + 2], 0
    shl ax, 5                       ; sectors * 512 / 16 = paragraphs
    add [dap_seg], ax
    mov al, '.'
    call putc
    jmp .read_loop

.loaded:
    ; --- enter protected mode -----------------------------------------------
    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pmode

no_lba:
    mov si, msg_nolba
    jmp fatal

disk_error:
    mov si, msg_disk
fatal:
    call print
.halt:
    cli
    hlt
    jmp .halt

; -----------------------------------------------------------------------------
; A20 handling
; -----------------------------------------------------------------------------

; check_a20: ZF=1 if memory wraps at 1 MiB (A20 disabled), ZF=0 if enabled.
; Compares 0000:0500 with FFFF:0510 (= physical 0x100500).
check_a20:
    push ds
    push es
    push si
    push di
    xor ax, ax
    mov es, ax
    not ax
    mov ds, ax
    mov di, 0x0500
    mov si, 0x0510
    mov bl, [es:di]
    mov bh, [ds:si]
    mov byte [es:di], 0x00
    mov byte [ds:si], 0xFF
    cmp byte [es:di], 0xFF          ; equal -> the write wrapped around
    mov [ds:si], bh                 ; (mov/pop keep the flags intact)
    mov [es:di], bl
    pop di
    pop si
    pop es
    pop ds
    ret

enable_a20:
    call check_a20
    jnz .ok

    mov ax, 0x2401                  ; method 1: BIOS
    int 0x15
    call check_a20
    jnz .ok

    in al, 0x92                     ; method 2: fast A20 gate
    or al, 0x02
    and al, 0xFE                    ; never set bit 0 (it resets the CPU)
    out 0x92, al
    call check_a20
    jnz .ok

    call kbc_wait                   ; method 3: keyboard controller
    mov al, 0xD1                    ; write output port
    out 0x64, al
    call kbc_wait
    mov al, 0xDF                    ; A20 on
    out 0x60, al
    call kbc_wait

    mov cx, 0x4000                  ; give the controller time to react
.poll:
    call check_a20
    jnz .ok
    loop .poll

    mov si, msg_a20_fail
    jmp fatal
.ok:
    mov si, msg_a20_ok
    jmp print                       ; tail call

kbc_wait:
    mov cx, 0xFFFF
.wait:
    in al, 0x64
    test al, 0x02
    jz .done
    loop .wait
.done:
    ret

; -----------------------------------------------------------------------------
; BIOS teletype output
; -----------------------------------------------------------------------------
print:
    lodsb
    test al, al
    jz .done
    call putc
    jmp print
.done:
    ret

putc:
    mov ah, 0x0E
    xor bx, bx
    int 0x10
    ret

; -----------------------------------------------------------------------------
; 32-bit entry
; -----------------------------------------------------------------------------
BITS 32
pmode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    movzx edx, byte [boot_drive]
    jmp 0x08:0x10000

; -----------------------------------------------------------------------------
; Data
; -----------------------------------------------------------------------------
align 8
gdt:
    dq 0                            ; null descriptor
    dq 0x00CF9A000000FFFF           ; 0x08: code, base 0, limit 4 GiB, 32-bit
    dq 0x00CF92000000FFFF           ; 0x10: data, base 0, limit 4 GiB, 32-bit
gdt_desc:
    dw gdt_desc - gdt - 1
    dd gdt

dap:                                ; INT 13h extended read packet
    db 0x10, 0
dap_count:
    dw 0
    dw 0                            ; buffer offset
dap_seg:
    dw KERNEL_SEG                   ; buffer segment
dap_lba:
    dd 1                            ; kernel starts right after the boot sector
    dd 0

remaining   dw KERNEL_SECTORS
chunk       dw 0
retries     db 0
boot_drive  db 0

msg_boot     db 'NixDOS boot: ', 0
msg_a20_ok   db 'A20 ok, loading kernel', 0
msg_a20_fail db 'A20 failed!', 0
msg_nolba    db 'No LBA BIOS!', 0
msg_disk     db 'Disk error!', 0

times 510 - ($ - $$) db 0
dw 0xAA55
