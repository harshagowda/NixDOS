; NixDOS bootloader (NASM, 16-bit real mode)
; Loads the NixDOS shell binary from disk and jumps to it.

BITS 16
ORG 0x7C00

%ifndef SHELL_SECTORS
%define SHELL_SECTORS 16
%endif

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    call enable_a20

    mov si, boot_msg
    call print_string

    ; Load stage-2 shell to 0000:1000
    mov ax, 0x0000
    mov es, ax
    mov bx, 0x1000

    mov ah, 0x02            ; BIOS disk read
    mov al, SHELL_SECTORS   ; sector count
    mov ch, 0x00            ; cylinder 0
    mov cl, 0x02            ; start at sector 2 (sector 1 is bootloader)
    mov dh, 0x00            ; head 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    jmp 0x0000:0x1000

disk_error:
    mov si, error_msg
    call print_string
    jmp $

; Enable address line 20 so memory above 1 MiB is reachable.
; Tries the fast A20 gate first, then falls back to the keyboard controller.
enable_a20:
    in al, 0x92
    test al, 0x02
    jnz .done
    or al, 0x02
    and al, 0xFE
    out 0x92, al
    in al, 0x92
    test al, 0x02
    jnz .done

    call .wait_input_clear
    mov al, 0xD1
    out 0x64, al

    call .wait_input_clear
    mov al, 0xDF
    out 0x60, al

    call .wait_input_clear

.done:
    ret

.wait_input_clear:
    in al, 0x64
    test al, 0x02
    jnz .wait_input_clear
    ret

print_string:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    jmp print_string
.done:
    ret

boot_msg db 'NixDOS bootloader: loading shell...', 0x0D, 0x0A, 0
error_msg db 'Disk read failed. System halted.', 0x0D, 0x0A, 0
boot_drive db 0

times 510 - ($ - $$) db 0
dw 0xAA55
