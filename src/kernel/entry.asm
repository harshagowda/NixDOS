; NixDOS 2 kernel entry, interrupt stubs and low-level helpers (NASM, 32-bit)

BITS 32

extern kmain
extern isr_handler
extern __bss_start
extern __bss_end

section .text.entry
global _start
_start:
    ; the boot sector passes the BIOS boot drive in EDX
    mov esp, 0x90000
    mov ebx, edx

    ; clear .bss (it is not part of the flat binary)
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    push ebx
    call kmain
.hang:
    cli
    hlt
    jmp .hang

section .text

; -----------------------------------------------------------------------------
; Interrupt service routine stubs: vectors 0-47
; -----------------------------------------------------------------------------
%macro ISR_NOERR 1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
%assign i 20
%rep 28
ISR_NOERR i
%assign i i+1
%endrep

isr_common:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    push esp                        ; struct regs *
    call isr_handler
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8                      ; int number + error code
    iret

section .rodata
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 48
    dd isr %+ i
%assign i i+1
%endrep

section .text

; -----------------------------------------------------------------------------
; int k_setjmp(k_jmp_buf b)  /  void k_longjmp(k_jmp_buf b, int val)
; buffer layout: ebx, esi, edi, ebp, esp, eip
; -----------------------------------------------------------------------------
global k_setjmp
k_setjmp:
    mov eax, [esp + 4]
    mov [eax], ebx
    mov [eax + 4], esi
    mov [eax + 8], edi
    mov [eax + 12], ebp
    lea ecx, [esp + 4]
    mov [eax + 16], ecx
    mov ecx, [esp]
    mov [eax + 20], ecx
    xor eax, eax
    ret

global k_longjmp
k_longjmp:
    mov edx, [esp + 4]
    mov eax, [esp + 8]
    test eax, eax
    jnz .nz
    inc eax
.nz:
    mov ebx, [edx]
    mov esi, [edx + 4]
    mov edi, [edx + 8]
    mov ebp, [edx + 12]
    mov esp, [edx + 16]
    jmp [edx + 20]

; -----------------------------------------------------------------------------
; int call_on_stack(u32 entry, u32 stack_top, int argc, char **argv)
; Runs a user program's main(argc, argv) on its own stack.
; -----------------------------------------------------------------------------
global call_on_stack
call_on_stack:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov eax, [ebp + 8]
    mov ecx, [ebp + 12]
    mov edx, [ebp + 16]
    mov esi, [ebp + 20]
    mov esp, ecx
    push esi
    push edx
    call eax
    lea esp, [ebp - 12]
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
