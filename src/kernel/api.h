/* NixDOS 2 - functions available to NixC programs.
 *
 * Compiled programs call these through a table whose address is stored at
 * API_PTR_ADDR, so executables keep working across kernel rebuilds as long
 * as entries are only ever appended to this list.
 *
 *   API(name, kernel function, argument count (-1 = variadic), return type)
 */
#ifndef NIXDOS_API_H
#define NIXDOS_API_H

#define TY_CHAR 1
#define TY_INT  2
#define TY_VOID 3
#define TY_PTR  16

#define API_LIST \
    API(putchar,   api_putchar,   1, TY_INT) \
    API(getchar,   api_getchar,   0, TY_INT) \
    API(puts,      api_puts,      1, TY_INT) \
    API(printf,    api_printf,   -1, TY_INT) \
    API(sprintf,   api_sprintf,  -1, TY_INT) \
    API(gets,      api_gets,      2, TY_CHAR + TY_PTR) \
    API(getkey,    api_getkey,    0, TY_INT) \
    API(kbhit,     api_kbhit,     0, TY_INT) \
    API(strlen,    api_strlen,    1, TY_INT) \
    API(strcmp,    api_strcmp,    2, TY_INT) \
    API(strncmp,   api_strncmp,   3, TY_INT) \
    API(strcpy,    api_strcpy,    2, TY_CHAR + TY_PTR) \
    API(strcat,    api_strcat,    2, TY_CHAR + TY_PTR) \
    API(strchr,    api_strchr,    2, TY_CHAR + TY_PTR) \
    API(memset,    api_memset,    3, TY_VOID + TY_PTR) \
    API(memcpy,    api_memcpy,    3, TY_VOID + TY_PTR) \
    API(atoi,      api_atoi,      1, TY_INT) \
    API(abs,       api_abs,       1, TY_INT) \
    API(isdigit,   api_isdigit,   1, TY_INT) \
    API(isalpha,   api_isalpha,   1, TY_INT) \
    API(isspace,   api_isspace,   1, TY_INT) \
    API(toupper,   api_toupper,   1, TY_INT) \
    API(tolower,   api_tolower,   1, TY_INT) \
    API(malloc,    api_malloc,    1, TY_VOID + TY_PTR) \
    API(free,      api_free,      1, TY_VOID) \
    API(rand,      api_rand,      0, TY_INT) \
    API(srand,     api_srand,     1, TY_VOID) \
    API(time,      api_time,      0, TY_INT) \
    API(ticks,     api_ticks,     0, TY_INT) \
    API(sleep,     api_sleep,     1, TY_VOID) \
    API(exit,      api_exit,      1, TY_VOID) \
    API(cls,       api_cls,       0, TY_VOID) \
    API(gotoxy,    api_gotoxy,    2, TY_VOID) \
    API(setcolor,  api_setcolor,  2, TY_VOID) \
    API(putat,     api_putat,     4, TY_VOID) \
    API(wherex,    api_wherex,    0, TY_INT) \
    API(wherey,    api_wherey,    0, TY_INT) \
    API(beep,      api_beep,      2, TY_VOID) \
    API(readfile,  api_readfile,  3, TY_INT) \
    API(writefile, api_writefile, 3, TY_INT) \
    API(open,      api_open,      2, TY_INT) \
    API(close,     api_close,     1, TY_INT) \
    API(read,      api_read,      3, TY_INT) \
    API(write,     api_write,     3, TY_INT) \
    API(lseek,     api_lseek,     3, TY_INT) \
    API(unlink,    api_unlink,    1, TY_INT) \
    API(filesize,  api_filesize,  1, TY_INT) \
    API(vgamode,   api_vgamode,   1, TY_INT) \
    API(vgapalette, api_vgapalette, 3, TY_VOID) \
    API(vgablit,   api_vgablit,   1, TY_VOID) \
    API(vgawait,   api_vgawait,   0, TY_VOID) \
    API(kbdraw,    api_kbdraw,    1, TY_VOID) \
    API(scancode,  api_scancode,  0, TY_INT) \
    API(heapinfo,  api_heapinfo,  2, TY_VOID) \
    API(conwrite,  api_conwrite,  2, TY_VOID) \
    API(audioopen, api_audioopen, 1, TY_INT) \
    API(audioclose, api_audioclose, 0, TY_VOID) \
    API(audiohalves, api_audiohalves, 0, TY_INT) \
    API(audiobuffer, api_audiobuffer, 0, TY_INT)

#endif
