; NixDOS shell (NASM, 16-bit real mode)
; Loaded by bootloader at 0000:1000 and executed there.
; This command surface mirrors the legacy SHELL.CPP command set.

BITS 16
ORG 0x1000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    sti

    call cls
    mov si, welcome_msg
    call print_string

main_loop:
    mov si, prompt
    call print_string

    mov di, cmd_buffer
    xor cx, cx

.read_char:
    xor ah, ah
    int 0x16

    cmp al, 0x0D
    je .line_done
    cmp al, 0x08
    je .handle_backspace
    cmp cx, CMD_MAX
    jae .read_char

    stosb
    inc cx
    call putc
    jmp .read_char

.handle_backspace:
    cmp cx, 0
    je .read_char
    dec di
    dec cx
    mov al, 0x08
    call putc
    mov al, ' '
    call putc
    mov al, 0x08
    call putc
    jmp .read_char

.line_done:
    mov al, 0
    stosb
    mov al, 0x0D
    call putc
    mov al, 0x0A
    call putc

    mov si, cmd_buffer
    call uppercase

    mov si, cmd_buffer
    mov di, cmd_help
    call streq
    cmp al, 1
    je do_help

    mov si, cmd_buffer
    mov di, cmd_clr
    call streq
    cmp al, 1
    je do_cls

    mov si, cmd_buffer
    mov di, cmd_vers
    call streq
    cmp al, 1
    je do_vers

    mov si, cmd_buffer
    mov di, cmd_rboot
    call streq
    cmp al, 1
    je do_reboot

    mov si, cmd_buffer
    mov di, cmd_sdown
    call streq
    cmp al, 1
    je do_halt

    mov si, cmd_buffer
    mov di, empty_cmd
    call streq
    cmp al, 1
    je main_loop

    ; Legacy command names from SHELL.CPP that are acknowledged here.
    mov si, cmd_buffer
    mov di, cmd_time
    call streq
    cmp al, 1
    je do_legacy

    mov si, cmd_buffer
    mov di, cmd_ctime
    call streq
    cmp al, 1
    je do_legacy

    mov si, cmd_buffer
    mov di, cmd_date
    call streq
    cmp al, 1
    je do_legacy

    mov si, cmd_buffer
    mov di, cmd_cdate
    call streq
    cmp al, 1
    je do_legacy

    mov si, cmd_buffer
    mov di, cmd_clock
    call streq
    cmp al, 1
    je do_legacy

    mov si, cmd_buffer
    mov di, cmd_ccolor
    call streq
    cmp al, 1
    je do_legacy

    mov si, cmd_buffer
    mov di, cmd_ndedit
    call streq
    cmp al, 1
    je do_legacy

    mov si, cmd_buffer
    mov di, cmd_prtmsg
    call streq
    cmp al, 1
    je do_legacy

    mov si, cmd_buffer
    mov di, cmd_prtscr
    call streq
    cmp al, 1
    je do_legacy

    mov si, cmd_buffer
    mov di, cmd_equip
    call streq
    cmp al, 1
    je do_legacy

    mov si, unknown_msg
    call print_string
    jmp main_loop

do_help:
    mov si, help_msg
    call print_string
    jmp main_loop

do_cls:
    call cls
    jmp main_loop

do_vers:
    mov si, ver_msg
    call print_string
    jmp main_loop

do_legacy:
    mov si, legacy_msg
    call print_string
    jmp main_loop

do_reboot:
    int 0x19
    jmp $

do_halt:
    cli
.halt_loop:
    hlt
    jmp .halt_loop

; uppercase in-place for zero-terminated string in SI
uppercase:
.next:
    mov al, [si]
    cmp al, 0
    je .done
    cmp al, 'a'
    jb .advance
    cmp al, 'z'
    ja .advance
    sub al, 32
    mov [si], al
.advance:
    inc si
    jmp .next
.done:
    ret

; string compare: SI=input, DI=const; AL=1 if equal else 0
streq:
.compare:
    mov al, [si]
    mov bl, [di]
    cmp al, bl
    jne .not_equal
    cmp al, 0
    je .equal
    inc si
    inc di
    jmp .compare
.equal:
    mov al, 1
    ret
.not_equal:
    xor al, al
    ret

cls:
    mov ax, 0x0600
    mov bh, 0x07
    mov cx, 0x0000
    mov dx, 0x184F
    int 0x10

    mov ah, 0x02
    mov bh, 0
    mov dx, 0x0000
    int 0x10
    ret

putc:
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    ret

print_string:
    lodsb
    test al, al
    jz .done
    call putc
    jmp print_string
.done:
    ret

welcome_msg db 'NixDOS shell loaded.', 0x0D, 0x0A, 'Legacy command names are recognized.', 0x0D, 0x0A, 0
prompt      db 'nixdos> ', 0
help_msg    db 'HELP CLR VERS TIME CTIME DATE CDATE CLOCK CCOLOR NDEDIT PRTMSG PRTSCR EQUIP RBOOT SDOWN', 0x0D, 0x0A, 0
ver_msg     db 'NixDOS NASM shell (legacy-compatible command map)', 0x0D, 0x0A, 0
legacy_msg  db 'Command recognized (legacy module placeholder in this NASM build).', 0x0D, 0x0A, 0
unknown_msg db 'Unknown command.', 0x0D, 0x0A, 0

cmd_help   db 'HELP', 0
cmd_clr    db 'CLR', 0
cmd_vers   db 'VERS', 0
cmd_time   db 'TIME', 0
cmd_ctime  db 'CTIME', 0
cmd_date   db 'DATE', 0
cmd_cdate  db 'CDATE', 0
cmd_clock  db 'CLOCK', 0
cmd_ccolor db 'CCOLOR', 0
cmd_ndedit db 'NDEDIT', 0
cmd_prtmsg db 'PRTMSG', 0
cmd_prtscr db 'PRTSCR', 0
cmd_equip  db 'EQUIP', 0
cmd_rboot  db 'RBOOT', 0
cmd_sdown  db 'SDOWN', 0
empty_cmd  db 0

CMD_MAX equ 63
cmd_buffer times CMD_MAX + 1 db 0
