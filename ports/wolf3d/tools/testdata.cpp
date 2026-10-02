// NixDOS - generator for structurally valid Wolfenstein 3-D data files.
//
// This is NOT the real game data. It produces placeholder art (labelled
// pictures, procedural walls and sprites), ten small maps, simple sounds and
// music so the Wolf4SDL port can be tested without id Software's files.
// For the real game, copy the shareware/registered data files instead.
//
// Build on the host (see Makefile target "wolf3d-testdata"); it is compiled
// against the game's own headers, so chunk numbers and structures match.
//
// usage: testdata <outdir>
#include "wl_def.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

typedef std::vector<uint8_t> bytes;

static void put16(bytes &b, unsigned v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF); }
static void put32(bytes &b, uint32_t v) { put16(b, v & 0xFFFF); put16(b, v >> 16); }
static void append(bytes &b, const bytes &a) { b.insert(b.end(), a.begin(), a.end()); }

static void write_file(const std::string &dir, const char *name, const bytes &b)
{
    std::string path = dir + "/" + name + "." + "wl1";
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) { perror(path.c_str()); exit(1); }
    fwrite(b.data(), 1, b.size(), f);
    fclose(f);
    printf("  %-14s %7zu bytes\n", (std::string(name) + ".wl1").c_str(), b.size());
}

// ---- 5x7 font -----------------------------------------------------------------
static const char *glyph_rows(char c)
{
    switch (toupper(c)) {
    case 'A': return "01110100011000111111100011000110001";
    case 'B': return "11110100011000111110100011000111110";
    case 'C': return "01110100011000010000100001000101110";
    case 'D': return "11110100011000110001100011000111110";
    case 'E': return "11111100001000011110100001000011111";
    case 'F': return "11111100001000011110100001000010000";
    case 'G': return "01110100011000010111100011000101111";
    case 'H': return "10001100011000111111100011000110001";
    case 'I': return "01110001000010000100001000010001110";
    case 'J': return "00111000100001000010000101001001100";
    case 'K': return "10001100101010011000101001001010001";
    case 'L': return "10000100001000010000100001000011111";
    case 'M': return "10001110111010110101100011000110001";
    case 'N': return "10001100011100110101100111000110001";
    case 'O': return "01110100011000110001100011000101110";
    case 'P': return "11110100011000111110100001000010000";
    case 'Q': return "01110100011000110001101011001001101";
    case 'R': return "11110100011000111110101001001010001";
    case 'S': return "01111100001000001110000010000111110";
    case 'T': return "11111001000010000100001000010000100";
    case 'U': return "10001100011000110001100011000101110";
    case 'V': return "10001100011000110001100010101000100";
    case 'W': return "10001100011000110101101011010101010";
    case 'X': return "10001100010101000100010101000110001";
    case 'Y': return "10001100010101000100001000010000100";
    case 'Z': return "11111000010001000100010001000011111";
    case '0': return "01110100011001110101110011000101110";
    case '1': return "00100011000010000100001000010001110";
    case '2': return "01110100010000100010001000100011111";
    case '3': return "11111000100010000010000011000101110";
    case '4': return "00010001100101010010111110001000010";
    case '5': return "11111100001111000001000011000101110";
    case '6': return "00110010001000011110100011000101110";
    case '7': return "11111000010001000100010000100001000";
    case '8': return "01110100011000101110100011000101110";
    case '9': return "01110100011000101111000010001001100";
    case '.': return "00000000000000000000000000110001100";
    case ',': return "00000000000000000000001100010001000";
    case ':': return "00000011000110000000011000110000000";
    case '!': return "00100001000010000100001000000000100";
    case '?': return "01110100010000100010001000000000100";
    case '-': return "00000000000000011111000000000000000";
    case '+': return "00000001000010011111001000010000000";
    case '/': return "00000000010001000100010001000000000";
    case '\'': return "00100001000100000000000000000000000";
    case '"': return "01010010100101000000000000000000000";
    case '(': return "00010001000100001000010000010000010";
    case ')': return "01000001000001000010000100010001000";
    case '%': return "11000110010001000100010001001100011";
    case '#': return "01010010101111101010111110101001010";
    case '*': return "00000001001010101110101010010000000";
    case '=': return "00000000001111100000111110000000000";
    case '<': return "00010001000100010000010000010000010";
    case '>': return "01000001000001000001000100010001000";
    case '_': return "00000000000000000000000000000011111";
    }
    return NULL;
}

// draw text into an 8-bit canvas
struct canvas {
    int w, h;
    bytes px;
    canvas(int w_, int h_, uint8_t fill) : w(w_), h(h_), px((size_t)w_ * h_, fill) {}
    void set(int x, int y, uint8_t c) { if (x >= 0 && y >= 0 && x < w && y < h) px[(size_t)y * w + x] = c; }
    void rect(int x0, int y0, int x1, int y1, uint8_t c) { for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) set(x, y, c); }
    void frame(uint8_t c) { for (int x = 0; x < w; x++) { set(x, 0, c); set(x, h - 1, c); } for (int y = 0; y < h; y++) { set(0, y, c); set(w - 1, y, c); } }
    void text(int x, int y, const char *s, uint8_t c, int scale = 1)
    {
        for (; *s; s++, x += 6 * scale) {
            const char *g = glyph_rows(*s);
            if (!g) continue;
            for (int r = 0; r < 7; r++)
                for (int k = 0; k < 5; k++)
                    if (g[r * 5 + k] == '1')
                        rect(x + k * scale, y + r * scale, x + (k + 1) * scale, y + (r + 1) * scale, c);
        }
    }
    void text_center(int y, const char *s, uint8_t c, int scale = 1)
    {
        int tw = (int)strlen(s) * 6 * scale - scale;
        text((w - tw) / 2, y, s, c, scale);
    }
    // Wolf pictures are stored as 4 planes
    bytes planar() const
    {
        bytes out((size_t)w * h);
        int qw = w / 4;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                out[(size_t)(y * qw + x / 4) + (size_t)(x & 3) * qw * h] = px[(size_t)y * w + x];
        return out;
    }
};

// ---- VGAGRAPH ---------------------------------------------------------------------
// The Huffman dictionary is a complete 8-level tree that reads the bits of a
// byte LSB first, so "compressed" data is simply the raw bytes.
static bytes make_dict()
{
    uint16_t nodes[255][2];
    int base[9];
    base[7] = 0; base[6] = 128; base[5] = 192; base[4] = 224; base[3] = 240;
    base[2] = 248; base[1] = 252; base[0] = 254;
    for (int k = 0; k < 8; k++) {
        for (int p = 0; p < (1 << k); p++) {
            for (int bit = 0; bit < 2; bit++) {
                int child = p | (bit << k);
                nodes[base[k] + p][bit] = (uint16_t)(k == 7 ? child : 256 + base[k + 1] + child);
            }
        }
    }
    bytes b;
    for (int i = 0; i < 255; i++) { put16(b, nodes[i][0]); put16(b, nodes[i][1]); }
    return b;
}

static bytes chunk_with_len(const bytes &data)
{
    bytes b;
    put32(b, (uint32_t)data.size());
    append(b, data);
    b.push_back(0);                 // the decoder reads one byte ahead
    return b;
}

static bytes make_font(int height, int scale)
{
    // fontstruct: int16 height, int16 location[256], int8 width[256], glyphs
    bytes glyphs;
    int16_t location[256];
    int8_t width[256];
    int header = 2 + 512 + 256;
    for (int c = 0; c < 256; c++) {
        const char *g = (c >= 32 && c < 127) ? glyph_rows((char)c) : NULL;
        int w = (c == ' ') ? 4 * scale : g ? 6 * scale : 0;
        location[c] = (int16_t)(header + glyphs.size());
        width[c] = (int8_t)w;
        for (int y = 0; y < height; y++)
            for (int x = 0; x < w; x++) {
                int gy = y / scale - (height / scale - 8), gx = x / scale;
                bool on = g && gx < 5 && gy >= 0 && gy < 7 && g[gy * 5 + gx] == '1';
                glyphs.push_back(on ? 1 : 0);
            }
    }
    bytes b;
    put16(b, (unsigned)height);
    for (int c = 0; c < 256; c++) put16(b, (uint16_t)location[c]);
    for (int c = 0; c < 256; c++) b.push_back((uint8_t)width[c]);
    append(b, glyphs);
    return b;
}

struct picinfo { int w, h; const char *label; };

static picinfo pic_for(int chunk)
{
    switch (chunk) {
    case TITLEPIC: return { 320, 200, "TITLE" };
    case CREDITSPIC: return { 320, 200, "CREDITS" };
    case PG13PIC: return { 64, 64, "PG13" };
    case HIGHSCORESPIC: return { 224, 48, "HIGH SCORES" };
    case STATUSBARPIC: return { 320, 40, "" };
    case C_OPTIONSPIC: return { 136, 24, "OPTIONS" };
    case C_CURSOR1PIC: case C_CURSOR2PIC: return { 24, 16, ">" };
    case C_NOTSELECTEDPIC: case C_SELECTEDPIC: return { 16, 16, "" };
    case C_MOUSELBACKPIC: return { 96, 16, "ESC BACK" };
    case C_BABYMODEPIC: case C_EASYPIC: case C_NORMALPIC: case C_HARDPIC: return { 28, 32, "" };
    case C_LOADSAVEDISKPIC: case C_DISKLOADING1PIC: case C_DISKLOADING2PIC: return { 16, 16, "" };
    case C_EPISODE1PIC: case C_EPISODE2PIC: case C_EPISODE3PIC:
    case C_EPISODE4PIC: case C_EPISODE5PIC: case C_EPISODE6PIC: return { 48, 24, "EP" };
    case C_CONTROLPIC: case C_CUSTOMIZEPIC: case C_LOADGAMEPIC: case C_SAVEGAMEPIC:
    case C_FXTITLEPIC: case C_DIGITITLEPIC: case C_MUSICTITLEPIC: return { 120, 24, "MENU" };
    case C_CODEPIC: case C_TIMECODEPIC: case C_LEVELPIC: case C_NAMEPIC: case C_SCOREPIC: return { 64, 16, "" };
    case C_JOY1PIC: case C_JOY2PIC: return { 64, 16, "" };
    case L_GUYPIC: case L_GUY2PIC: case L_BJWINSPIC: return { 88, 112, "BJ" };
    case KNIFEPIC: case GUNPIC: case MACHINEGUNPIC: case GATLINGGUNPIC: return { 48, 24, "GUN" };
    case NOKEYPIC: case GOLDKEYPIC: case SILVERKEYPIC: return { 8, 16, "" };
    case GOTGATLINGPIC: case MUTANTBJPIC: return { 24, 32, "" };
    case PAUSEDPIC: return { 128, 40, "PAUSED" };
    case GETPSYCHEDPIC: return { 224, 48, "GET PSYCHED!" };
    case H_TOPWINDOWPIC: return { 320, 8, "" };
    case H_LEFTWINDOWPIC: case H_RIGHTWINDOWPIC: return { 8, 176, "" };
    case H_BOTTOMINFOPIC: return { 320, 16, "" };
    }
    if (chunk >= N_BLANKPIC && chunk <= N_9PIC) return { 8, 16, "" };
    if (chunk >= FACE1APIC && chunk <= FACE8APIC) return { 24, 32, "" };
    if (chunk >= L_COLONPIC && chunk <= L_APOSTROPHEPIC) return { 16, 16, "" };
    return { 64, 32, "" };
}

static bytes draw_pic(int chunk, const picinfo &pi)
{
    canvas c(pi.w, pi.h, (uint8_t)(0x18 + (chunk * 7) % 8));
    if (chunk == TITLEPIC) {
        c.rect(0, 0, 320, 200, 0x00);
        c.rect(0, 40, 320, 160, 0x29);
        c.text_center(60, "WOLFENSTEIN 3-D", 0x0E, 3);
        c.text_center(100, "ON NIXDOS", 0x0F, 2);
        c.text_center(130, "TEST DATA - NOT THE REAL GAME", 0x1C, 1);
        c.text_center(142, "COPY THE SHAREWARE FILES FOR THE REAL ONE", 0x1C, 1);
    } else if (chunk == CREDITSPIC) {
        c.rect(0, 0, 320, 200, 0x00);
        c.text_center(70, "WOLF4SDL ON NIXDOS", 0x0F, 2);
        c.text_center(100, "ID SOFTWARE  WOLF4SDL  NIXDOS", 0x0E, 1);
    } else if (chunk == STATUSBARPIC) {
        c.rect(0, 0, 320, 40, 0x04);
        c.frame(0x1C);
    } else if (chunk >= FACE1APIC && chunk <= FACE8APIC) {
        c.rect(2, 2, 22, 30, 0x45);                 // a face
        c.rect(6, 10, 9, 13, 0x00); c.rect(15, 10, 18, 13, 0x00);
        c.rect(7, 22, 17, 24, 0x28);
    } else if (chunk >= N_0PIC && chunk <= N_9PIC) {
        c.rect(0, 0, 8, 16, 0x00);
        char d[2] = { (char)('0' + chunk - N_0PIC), 0 };
        c.text(1, 4, d, 0x0F);
    } else if (chunk == N_BLANKPIC) {
        c.rect(0, 0, 8, 16, 0x00);
    } else if (chunk >= L_NUM0PIC && chunk <= L_NUM9PIC) {
        char d[2] = { (char)('0' + chunk - L_NUM0PIC), 0 };
        c.rect(0, 0, 16, 16, 0x29);
        c.text(4, 4, d, 0x0F);
    } else if (chunk >= L_APIC && chunk <= L_ZPIC) {
        char d[2] = { (char)('A' + chunk - L_APIC), 0 };
        c.rect(0, 0, 16, 16, 0x29);
        c.text(4, 4, d, 0x0F);
    } else {
        c.frame(0x0F);
        if (pi.label[0] && (int)strlen(pi.label) * 6 <= pi.w) c.text_center(pi.h / 2 - 3, pi.label, 0x0F);
    }
    return c.planar();
}

static bytes make_article(const char *title)
{
    std::string s = "^P\r\n^C0F";
    s += title;
    s += "\r\n\r\nThis is placeholder data generated by NixDOS\r\n"
         "to test the Wolf4SDL port. Copy the real Wolfenstein\r\n"
         "3-D data files into ports/wolf3d/data to play the\r\n"
         "actual game.\r\n^E\r\n";
    return bytes(s.begin(), s.end());
}

static bytes make_demo(int seed)
{
    bytes moves;
    int tics = 70 * 8;
    for (int t = 0; t < tics; t++) {
        int8_t buttons = 0, cx = 0, cy = 0;
        int phase = (t / 70 + seed) % 4;
        if (phase == 0 || phase == 2) cy = -100;    // walk forward
        else cx = (phase == 1) ? 60 : -60;          // turn
        moves.push_back((uint8_t)buttons);
        moves.push_back((uint8_t)cx);
        moves.push_back((uint8_t)cy);
    }
    bytes b;
    b.push_back(0);                                 // map
    put16(b, (unsigned)(moves.size() + 4));
    b.push_back(0);
    append(b, moves);
    return b;
}

static void make_graphics(const std::string &dir)
{
    int nchunks = T_ENDART1 + 1;                    // shareware: no end art for episodes 2-6
    std::vector<bytes> chunks(nchunks);

    // STRUCTPIC: picture sizes
    bytes table;
    for (int i = 0; i < NUMPICS; i++) {
        picinfo pi = pic_for(STARTPICS + i);
        put16(table, (unsigned)pi.w);
        put16(table, (unsigned)pi.h);
    }
    chunks[STRUCTPIC] = chunk_with_len(table);
    chunks[STARTFONT] = chunk_with_len(make_font(10, 1));
    chunks[STARTFONT + 1] = chunk_with_len(make_font(12, 1));
    for (int i = 0; i < NUMPICS; i++)
        chunks[STARTPICS + i] = chunk_with_len(draw_pic(STARTPICS + i, pic_for(STARTPICS + i)));

    // tile8: implicit size, no length prefix
    bytes tiles;
    for (int t = 0; t < NUMTILE8; t++) {
        canvas c(8, 8, (uint8_t)(t % 2 ? 0x1D : 0x1F));
        append(tiles, c.planar());
    }
    tiles.push_back(0);
    chunks[STARTTILE8] = tiles;

    canvas order(320, 200, 0x00);
    order.text_center(96, "ORDER WOLFENSTEIN 3-D", 0x0F, 2);
    chunks[ORDERSCREEN] = chunk_with_len(order.planar());
    chunks[ERRORSCREEN] = chunk_with_len(bytes(4000, 0x07));
    chunks[T_HELPART] = chunk_with_len(make_article("NIXDOS WOLF3D TEST DATA"));
    for (int d = 0; d < 4; d++) chunks[T_DEMO0 + d] = chunk_with_len(make_demo(d));
    chunks[T_ENDART1] = chunk_with_len(make_article("YOU WIN!"));

    bytes graph, head;
    for (int i = 0; i < nchunks; i++) {
        unsigned off = (unsigned)graph.size();
        head.push_back(off & 0xFF); head.push_back((off >> 8) & 0xFF); head.push_back((off >> 16) & 0xFF);
        append(graph, chunks[i]);
    }
    unsigned off = (unsigned)graph.size();
    head.push_back(off & 0xFF); head.push_back((off >> 8) & 0xFF); head.push_back((off >> 16) & 0xFF);

    write_file(dir, "vgadict", make_dict());
    write_file(dir, "vgahead", head);
    write_file(dir, "vgagraph", graph);
}

// ---- VSWAP: walls, sprites, digitized sounds --------------------------------------
#define NUM_WALL_TILES 48
#define NUM_DIGI 64

static bytes make_wall(int tile, bool dark)
{
    // 64x64, stored column by column
    canvas c(64, 64, 0);
    static const uint8_t base[] = { 0x1A, 0x29, 0x18, 0x2F, 0x06, 0x58, 0x77, 0x96, 0x46, 0xB4 };
    uint8_t col = base[tile % 10];
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            bool mortar = (y % 16 == 15) || ((x + ((y / 16) % 2) * 16) % 32 == 31);
            c.set(x, y, mortar ? 0x19 : (uint8_t)(col + ((x * 7 + y * 3) % 3)));
        }
    if (tile == ELEVATORTILE) {
        c.rect(8, 8, 56, 56, 0x1D);
        c.text(14, 28, "EXIT", 0x0E);
    } else {
        char num[4];
        snprintf(num, sizeof(num), "%d", tile);
        c.text(4, 4, num, 0x0F);
    }
    if (dark) for (auto &p : c.px) if (p > 1) p = (uint8_t)(p + 1);
    bytes b(4096);
    for (int x = 0; x < 64; x++)
        for (int y = 0; y < 64; y++) b[(size_t)x * 64 + y] = c.px[(size_t)y * 64 + x];
    return b;
}

static bytes make_door(int which)
{
    canvas c(64, 64, (uint8_t)(which < 2 ? 0x77 : 0x1A));
    for (int x = 0; x < 64; x += 8) c.rect(x, 0, x + 1, 64, 0x75);
    c.rect(52, 28, 56, 36, 0x0E);
    bytes b(4096);
    for (int x = 0; x < 64; x++)
        for (int y = 0; y < 64; y++) b[(size_t)x * 64 + y] = c.px[(size_t)y * 64 + x];
    return b;
}

// t_compshape: leftpix, rightpix, dataofs[], pixel data, post commands
static bytes make_sprite(int n)
{
    int left = 16, right = 47, top = 8, bottom = 63;
    uint8_t color = (uint8_t)(0x20 + (n * 13) % 0xC0);
    bool weapon = n >= SPR_KNIFEREADY;
    if (weapon) { left = 24; right = 39; top = 36; }
    else if (n >= SPR_STAT_0 && n <= SPR_STAT_47) { left = 22; right = 41; top = 34; }
    int ncols = right - left + 1, h = bottom - top;
    size_t header = 4 + 2 * (size_t)ncols;
    bytes pixels, posts;
    std::vector<unsigned> col_ofs(ncols);
    size_t pix_base = header;
    for (int x = 0; x < ncols; x++) {
        size_t start = pixels.size();
        for (int y = 0; y < h; y++) {
            bool edge = x == 0 || x == ncols - 1 || y == 0 || y == h - 1;
            pixels.push_back(edge ? 0x0F : (uint8_t)(color + ((x / 4 + y / 4) % 2)));
        }
        col_ofs[x] = (unsigned)start;
    }
    size_t posts_base = pix_base + pixels.size();
    bytes b;
    put16(b, (unsigned)left);
    put16(b, (unsigned)right);
    for (int x = 0; x < ncols; x++) {
        put16(b, (unsigned)(posts_base + posts.size()));
        unsigned newstart = (unsigned)(pix_base + col_ofs[x]) - (unsigned)top;
        put16(posts, (unsigned)bottom * 2);
        put16(posts, newstart & 0xFFFF);
        put16(posts, (unsigned)top * 2);
        put16(posts, 0);
    }
    append(b, pixels);
    append(b, posts);
    return b;
}

static bytes make_digi(int n)
{
    // short 7042 Hz unsigned 8-bit "bleeps" of varying pitch
    int len = 1200 + (n % 5) * 400;
    bytes b((size_t)len);
    int period = 8 + (n * 3) % 40;
    for (int i = 0; i < len; i++) {
        int env = 64 * (len - i) / len;
        b[(size_t)i] = (uint8_t)(128 + ((i / (period / 2)) % 2 ? env : -env));
    }
    return b;
}

static void make_vswap(const std::string &dir)
{
    std::vector<bytes> pages;
    for (int t = 1; t <= NUM_WALL_TILES; t++) {
        pages.push_back(make_wall(t, false));
        pages.push_back(make_wall(t, true));
    }
    for (int d = 0; d < 8; d++) pages.push_back(make_door(d));
    int sprite_start = (int)pages.size();
    for (int s = 0; s < SPR_KNIFEREADY + 20; s++) pages.push_back(make_sprite(s));
    int sound_start = (int)pages.size();
    bytes info;
    for (int d = 0; d < NUM_DIGI; d++) {
        bytes s = make_digi(d);
        put16(info, (unsigned)(pages.size() - sound_start));
        put16(info, (unsigned)s.size());
        pages.push_back(s);
    }
    pages.push_back(info);

    int n = (int)pages.size();
    bytes b;
    put16(b, (unsigned)n);
    put16(b, (unsigned)sprite_start);
    put16(b, (unsigned)sound_start);
    uint32_t pos = 6 + (uint32_t)n * 6;
    for (int i = 0; i < n; i++) { put32(b, pos); pos += (uint32_t)pages[i].size(); }
    for (int i = 0; i < n; i++) put16(b, (unsigned)pages[i].size());
    for (auto &p : pages) append(b, p);
    write_file(dir, "vswap", b);
}

// ---- maps --------------------------------------------------------------------------
static void make_maps(const std::string &dir)
{
    const int levels = 10;
    bytes maps, head;
    put16(head, 0xABCD);                            // RLEW tag
    const char sig[] = "TED5v1.0";
    maps.insert(maps.end(), sig, sig + 8);
    std::vector<int32_t> offsets(100, -1);

    for (int lvl = 0; lvl < levels; lvl++) {
        uint16_t walls[64][64], objs[64][64];
        for (int y = 0; y < 64; y++)
            for (int x = 0; x < 64; x++) { walls[y][x] = (uint16_t)(1 + (lvl % 8)); objs[y][x] = 0; }
        // two rooms joined by a corridor with a door, and an exit room
        auto room = [&](int x0, int y0, int x1, int y1, int area) {
            for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) walls[y][x] = (uint16_t)(AREATILE + area);
        };
        room(20, 20, 30, 30, 1);                    // start room
        room(31, 24, 39, 26, 2);                    // corridor
        room(41, 18, 52, 32, 3);                    // big room
        room(53, 24, 56, 26, 4);                    // exit room
        walls[25][40] = 90;                         // door between corridor and big room (vertical)
        walls[25][57] = ELEVATORTILE;               // elevator switch at the end
        walls[24][57] = walls[26][57] = ELEVATORTILE;
        for (int y = 20; y <= 30; y += 5) walls[y][19] = (uint16_t)(2 + (y / 5) % 6);   // decorations

        objs[25][23] = 20;                          // player start, facing east
        objs[22][22] = 26; objs[28][28] = 26;       // floor lamps
        objs[25][45] = 27; objs[25][50] = 27;       // chandeliers
        objs[20][44] = 52; objs[30][44] = 53;       // treasure
        objs[20][50] = 49; objs[30][50] = 48;       // ammo, first aid
        objs[22][48] = 108; objs[28][48] = 109;     // guards (standing)
        objs[25][55] = 54;                          // chest

        bytes planes[2];
        for (int p = 0; p < 2; p++) {
            bytes rlew;
            put16(rlew, 64 * 64 * 2);
            for (int y = 0; y < 64; y++)
                for (int x = 0; x < 64; x++) put16(rlew, p ? objs[y][x] : walls[y][x]);
            bytes carm;
            put16(carm, (unsigned)rlew.size());     // expanded length of the RLEW data
            append(carm, rlew);
            planes[p] = carm;
        }
        uint32_t starts[3];
        starts[0] = (uint32_t)maps.size(); append(maps, planes[0]);
        starts[1] = (uint32_t)maps.size(); append(maps, planes[1]);
        starts[2] = 0;
        offsets[lvl] = (int32_t)maps.size();
        bytes hdr;
        for (int p = 0; p < 3; p++) put32(hdr, starts[p]);
        put16(hdr, (unsigned)planes[0].size());
        put16(hdr, (unsigned)planes[1].size());
        put16(hdr, 0);
        put16(hdr, 64);
        put16(hdr, 64);
        char name[16] = { 0 };
        snprintf(name, sizeof(name), "Test Level %d", lvl + 1);
        hdr.insert(hdr.end(), name, name + 16);
        append(maps, hdr);
    }
    for (int i = 0; i < 100; i++) put32(head, (uint32_t)offsets[i]);
    write_file(dir, "maphead", head);
    write_file(dir, "gamemaps", maps);
}

// ---- audio: PC speaker + AdLib sound effects, AdLib (IMF) music ------------------
static void make_audio(const std::string &dir)
{
    std::vector<bytes> chunks(NUMSNDCHUNKS);
    for (int s = 0; s < LASTSOUND; s++) {
        bytes pc;                                   // PC speaker: length, priority, data
        int n = 12;
        put32(pc, (uint32_t)n);
        put16(pc, 10);
        for (int i = 0; i < n; i++) pc.push_back((uint8_t)(20 + (s * 5 + i * 3) % 60));
        pc.push_back(0);
        chunks[STARTPCSOUNDS + s] = pc;

        bytes al;                                   // AdLib: length, priority, instrument, block, data
        put32(al, (uint32_t)n);
        put16(al, 10);
        static const uint8_t inst[16] = { 0x21, 0x21, 0x00, 0x00, 0xF2, 0xF2, 0x54, 0x54,
                                          0x00, 0x00, 0x00, 0, 0, 0, 0, 0 };
        al.insert(al.end(), inst, inst + 16);
        al.push_back(4);
        for (int i = 0; i < n; i++) al.push_back((uint8_t)(0x40 + (s * 7 + i * 11) % 0xA0));
        al.push_back(0);
        chunks[STARTADLIBSOUNDS + s] = al;
    }
    for (int m = 0; m < LASTMUSIC; m++) {
        // IMF: word length, then (reg, value, delay) events at 700 Hz
        bytes ev;
        auto out = [&](int reg, int val, int delay) { ev.push_back((uint8_t)reg); ev.push_back((uint8_t)val); put16(ev, (unsigned)delay); };
        out(0x20, 0x01, 0); out(0x40, 0x10, 0); out(0x60, 0xF0, 0); out(0x80, 0x77, 0);
        out(0x23, 0x01, 0); out(0x43, 0x00, 0); out(0x63, 0xF0, 0); out(0x83, 0x77, 0);
        static const int notes[] = { 0x157, 0x181, 0x1B0, 0x1CA, 0x202, 0x241, 0x287, 0x2AE };
        for (int i = 0; i < 32; i++) {
            int f = notes[(i * (m + 3)) % 8];
            out(0xA0, f & 0xFF, 0);
            out(0xB0, 0x20 | (4 << 2) | (f >> 8), 140);
            out(0xB0, (4 << 2) | (f >> 8), 35);
        }
        bytes mus;
        put16(mus, (unsigned)ev.size());
        append(mus, ev);
        chunks[STARTMUSIC + m] = mus;
    }
    bytes t, h;
    for (int i = 0; i < NUMSNDCHUNKS; i++) {
        put32(h, (uint32_t)t.size());
        append(t, chunks[i]);
    }
    put32(h, (uint32_t)t.size());
    write_file(dir, "audiohed", h);
    write_file(dir, "audiot", t);
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s <outdir>\n", argv[0]); return 1; }
    printf("Generating Wolf3D test data (placeholder art, not id's game) in %s\n", argv[1]);
    make_graphics(argv[1]);
    make_vswap(argv[1]);
    make_maps(argv[1]);
    make_audio(argv[1]);
    return 0;
}
