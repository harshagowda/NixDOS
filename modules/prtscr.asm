BITS 16
ORG 0x2000

start:
    mov si, msg
.next:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10
    jmp .next
.done:
    retf

msg db 'PRTSCR module loaded from disk.', 0x0D, 0x0A, 0
