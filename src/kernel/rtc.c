/* NixDOS 2 - CMOS real-time clock */
#include "kernel.h"

static u8 cmos_read(u8 reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static void cmos_write(u8 reg, u8 v)
{
    outb(0x70, reg);
    outb(0x71, v);
}

static int updating(void) { return cmos_read(0x0A) & 0x80; }
static int bcd2bin(int v) { return (v & 0x0F) + (v >> 4) * 10; }
static int bin2bcd(int v) { return ((v / 10) << 4) | (v % 10); }

static void read_raw(struct rtc_time *t)
{
    while (updating()) ;
    t->sec = cmos_read(0x00);
    t->min = cmos_read(0x02);
    t->hour = cmos_read(0x04);
    t->day = cmos_read(0x07);
    t->month = cmos_read(0x08);
    t->year = cmos_read(0x09);
}

void rtc_read(struct rtc_time *t)
{
    struct rtc_time a, b;
    /* read until two consecutive reads agree (avoids torn updates) */
    read_raw(&b);
    do {
        a = b;
        read_raw(&b);
    } while (a.sec != b.sec || a.min != b.min || a.hour != b.hour ||
             a.day != b.day || a.month != b.month || a.year != b.year);

    u8 regb = cmos_read(0x0B);
    if (!(regb & 0x04)) {
        int pm = b.hour & 0x80;
        b.sec = bcd2bin(b.sec);
        b.min = bcd2bin(b.min);
        b.hour = bcd2bin(b.hour & 0x7F) | pm;
        b.day = bcd2bin(b.day);
        b.month = bcd2bin(b.month);
        b.year = bcd2bin(b.year);
    }
    if (!(regb & 0x02) && (b.hour & 0x80))
        b.hour = ((b.hour & 0x7F) + 12) % 24;
    b.year += 2000;
    *t = b;
}

void rtc_write(const struct rtc_time *t)
{
    u8 regb = cmos_read(0x0B);
    int bcd = !(regb & 0x04);
    int y = t->year % 100;
    cmos_write(0x0B, regb | 0x80);  /* halt updates while we write */
    cmos_write(0x00, (u8)(bcd ? bin2bcd(t->sec) : t->sec));
    cmos_write(0x02, (u8)(bcd ? bin2bcd(t->min) : t->min));
    cmos_write(0x04, (u8)(bcd ? bin2bcd(t->hour) : t->hour));
    cmos_write(0x07, (u8)(bcd ? bin2bcd(t->day) : t->day));
    cmos_write(0x08, (u8)(bcd ? bin2bcd(t->month) : t->month));
    cmos_write(0x09, (u8)(bcd ? bin2bcd(y) : y));
    cmos_write(0x0B, regb & 0x7F);
}

/* days since 1970-01-01 (proleptic Gregorian calendar) */
static int days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

u32 rtc_unix_time(void)
{
    struct rtc_time t;
    rtc_read(&t);
    return (u32)days_from_civil(t.year, t.month, t.day) * 86400u +
           (u32)(t.hour * 3600 + t.min * 60 + t.sec);
}
