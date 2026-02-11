/* Legacy command module (time) built as 16-bit flat binary from C source. */
__attribute__((naked)) void _start(void) {
  __asm__ __volatile__(
    ".code16gcc\n\t"
    "mov $msg, %%si\n\t"
    "1: lodsb\n\t"
    "test %%al, %%al\n\t"
    "jz 2f\n\t"
    "mov $0x0E, %%ah\n\t"
    "xor %%bx, %%bx\n\t"
    "mov $0x07, %%bl\n\t"
    "int $0x10\n\t"
    "jmp 1b\n\t"
    "2: lret\n\t"
    "msg: .ascii \"TIME: legacy C module loaded from flat binary\"\n\t"
    ".byte 13,10,0\n\t"
    : : : "ax", "bx", "si"
  );
}
