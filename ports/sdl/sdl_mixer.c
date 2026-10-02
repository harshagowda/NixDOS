/* NixDOS 2 - SDL_mixer-compatible mixer.
 *
 * Output is signed 16-bit stereo. With a Sound Blaster 16 the mixed audio is
 * written into the card's DMA ring buffer (two halves, refilled as the card
 * finishes each one). Without a card a "null device" mixes and discards in
 * real time, so music and sound effects still advance (like SDL's dummy driver).
 *
 * There are no threads: NixAudio_Pump() is called from SDL_PollEvent,
 * SDL_Delay and SDL_RenderPresent and mixes whatever is due.
 */
#include "SDL_mixer.h"
#include <nixdos.h>

#define K_AUDIOOPEN(r)  NX_CALL(audioopen, int (*)(int))(r)
#define K_AUDIOCLOSE()  NX_CALL(audioclose, void (*)(void))()
#define K_AUDIOHALVES() NX_CALL(audiohalves, int (*)(void))()
#define K_AUDIOBUF()    NX_CALL(audiobuffer, int (*)(void))()

#define OUT_RATE 22050

struct channel {
    Mix_Chunk *chunk;
    Uint32 pos;
    int playing;
    Uint8 left, right;
    int tag;
    Uint32 started;
};

static struct channel chans[MIX_CHANNELS];
static int reserved;
static Uint32 play_counter;

static int opened, hw;
static int half_bytes;
static Uint8 *ring;
static Uint32 written_halves;
static Uint32 null_last_ms;
static Sint16 *scratch;
static int scratch_frames;
static int mixing;

static Mix_MixFunc music_fn, post_fn;
static void *music_arg, *post_arg;
static void (*finished_fn)(int);

const char *Mix_GetError(void) { return "audio error"; }

int Mix_OpenAudioDevice(int frequency, Uint16 format, int channels, int chunksize,
                        const char *device, int allowed_changes)
{
    (void)frequency; (void)chunksize; (void)device; (void)allowed_changes;
    if (format != AUDIO_S16SYS || channels != 2) return -1;
    if (opened) return 0;
    memset(chans, 0, sizeof(chans));
    for (int i = 0; i < MIX_CHANNELS; i++) { chans[i].left = chans[i].right = 255; chans[i].tag = -1; }
    reserved = 0;
    half_bytes = K_AUDIOOPEN(OUT_RATE);
    hw = half_bytes > 0;
    if (hw) {
        ring = (Uint8 *)K_AUDIOBUF();
        written_halves = 0;
    } else {
        half_bytes = 2048;
    }
    scratch_frames = half_bytes / 4;
    scratch = malloc((size_t)half_bytes);
    if (!scratch) { if (hw) K_AUDIOCLOSE(); return -1; }
    null_last_ms = (Uint32)nx_ticks();
    opened = 1;
    return 0;
}

int Mix_OpenAudio(int f, Uint16 fmt, int c, int cs) { return Mix_OpenAudioDevice(f, fmt, c, cs, NULL, 0); }

void Mix_CloseAudio(void)
{
    if (!opened) return;
    if (hw) K_AUDIOCLOSE();
    free(scratch);
    scratch = NULL;
    opened = hw = 0;
}

int Mix_QuerySpec(int *f, Uint16 *fmt, int *c)
{
    if (f) *f = OUT_RATE;
    if (fmt) *fmt = AUDIO_S16SYS;
    if (c) *c = 2;
    return opened;
}

int Mix_AllocateChannels(int n) { (void)n; return MIX_CHANNELS; }
int Mix_ReserveChannels(int n) { reserved = n < 0 ? 0 : n > MIX_CHANNELS ? MIX_CHANNELS : n; return reserved; }

int Mix_GroupChannels(int from, int to, int tag)
{
    int n = 0;
    for (int i = from; i <= to && i < MIX_CHANNELS; i++, n++) chans[i].tag = tag;
    return n;
}

int Mix_GroupAvailable(int tag)
{
    for (int i = 0; i < MIX_CHANNELS; i++)
        if ((tag == -1 || chans[i].tag == tag) && !chans[i].playing) return i;
    return -1;
}

int Mix_GroupOldest(int tag)
{
    int best = -1;
    for (int i = 0; i < MIX_CHANNELS; i++)
        if ((tag == -1 || chans[i].tag == tag) && chans[i].playing &&
            (best < 0 || chans[i].started < chans[best].started)) best = i;
    return best;
}

int Mix_PlayChannel(int ch, Mix_Chunk *chunk, int loops)
{
    (void)loops;
    if (!opened || !chunk || !chunk->abuf) return -1;
    if (ch < 0) {
        for (ch = reserved; ch < MIX_CHANNELS && chans[ch].playing; ch++) ;
        if (ch >= MIX_CHANNELS) return -1;
    }
    if (ch >= MIX_CHANNELS) return -1;
    chans[ch].chunk = chunk;
    chans[ch].pos = 0;
    chans[ch].playing = 1;
    chans[ch].started = ++play_counter;
    return ch;
}

static void finish(int ch)
{
    chans[ch].playing = 0;
    chans[ch].chunk = NULL;
    if (finished_fn) finished_fn(ch);
}

int Mix_HaltChannel(int ch)
{
    for (int i = 0; i < MIX_CHANNELS; i++)
        if ((ch < 0 || i == ch) && chans[i].playing) finish(i);
    return 0;
}

int Mix_Playing(int ch)
{
    if (ch >= 0) return ch < MIX_CHANNELS && chans[ch].playing;
    int n = 0;
    for (int i = 0; i < MIX_CHANNELS; i++) n += chans[i].playing;
    return n;
}

int Mix_SetPanning(int ch, Uint8 l, Uint8 r)
{
    if (ch < 0 || ch >= MIX_CHANNELS) return 0;
    chans[ch].left = l;
    chans[ch].right = r;
    return 1;
}

void Mix_HookMusic(Mix_MixFunc fn, void *arg) { music_fn = fn; music_arg = arg; }
void Mix_SetPostMix(Mix_MixFunc fn, void *arg) { post_fn = fn; post_arg = arg; }
void Mix_ChannelFinished(void (*fn)(int)) { finished_fn = fn; }

static inline Sint16 clamp16(int v) { return (Sint16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

/* Mix `frames` stereo frames into out. */
static void mix(Sint16 *out, int frames)
{
    int bytes = frames * 4;
    memset(out, 0, (size_t)bytes);
    if (music_fn) music_fn(music_arg, (Uint8 *)out, bytes);
    for (int c = 0; c < MIX_CHANNELS; c++) {
        struct channel *ch = &chans[c];
        if (!ch->playing) continue;
        const Sint16 *src = (const Sint16 *)(ch->chunk->abuf + ch->pos);
        Uint32 left_bytes = ch->chunk->alen - ch->pos;
        int n = (int)(left_bytes / 4) < frames ? (int)(left_bytes / 4) : frames;
        int vol = ch->chunk->volume;
        int lv = vol * ch->left / 255, rv = vol * ch->right / 255;
        for (int i = 0; i < n; i++) {
            out[i * 2] = clamp16(out[i * 2] + src[i * 2] * lv / MIX_MAX_VOLUME);
            out[i * 2 + 1] = clamp16(out[i * 2 + 1] + src[i * 2 + 1] * rv / MIX_MAX_VOLUME);
        }
        ch->pos += (Uint32)n * 4;
        if (ch->pos + 4 > ch->chunk->alen) finish(c);
    }
    if (post_fn) post_fn(post_arg, (Uint8 *)out, bytes);
}

void NixAudio_Pump(void)
{
    if (!opened || mixing) return;
    mixing = 1;
    if (hw) {
        /* the card has finished `done` halves; keep two halves ahead of it */
        Uint32 done = (Uint32)K_AUDIOHALVES();
        if (written_halves < done) written_halves = done;   /* fell behind: skip */
        while (written_halves < done + 2) {
            Sint16 *dst = (Sint16 *)(ring + (written_halves & 1) * (Uint32)half_bytes);
            mix(scratch, scratch_frames);
            memcpy(dst, scratch, (size_t)half_bytes);
            written_halves++;
        }
    } else {
        Uint32 now = (Uint32)nx_ticks();
        Uint32 due = (now - null_last_ms) * OUT_RATE / 1000;
        if (due > (Uint32)OUT_RATE / 4) due = OUT_RATE / 4;     /* don't catch up forever */
        if (due >= (Uint32)scratch_frames) {
            Uint32 chunks = due / (Uint32)scratch_frames;
            for (Uint32 i = 0; i < chunks; i++) mix(scratch, scratch_frames);
            null_last_ms += chunks * (Uint32)scratch_frames * 1000 / OUT_RATE;
            if (now - null_last_ms > 1000) null_last_ms = now;
        }
    }
    mixing = 0;
}

/* ---- format conversion: U8/S8/S16, mono/stereo, any rate -> S16 at the output rate */
int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, SDL_AudioFormat sf, Uint8 sc, int sr,
                      SDL_AudioFormat df, Uint8 dc, int dr)
{
    memset(cvt, 0, sizeof(*cvt));
    if (df != AUDIO_S16SYS || sr <= 0 || dr <= 0 || !sc || !dc) return -1;
    cvt->src_format = sf;
    cvt->dst_format = df;
    cvt->src_channels = sc;
    cvt->dst_channels = dc;
    cvt->src_rate = sr;
    cvt->dst_rate = dr;
    cvt->needed = 1;
    int in_frame = (sf & 0xFF) / 8 * sc, out_frame = 2 * dc;
    cvt->len_ratio = (double)out_frame * dr / ((double)in_frame * sr);
    cvt->len_mult = (out_frame * dr + in_frame * sr - 1) / (in_frame * sr) + 1;
    return 0;
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt)
{
    int in_bps = (cvt->src_format & 0xFF) / 8, in_frame = in_bps * cvt->src_channels;
    int nin = cvt->len / in_frame;
    int nout = (int)((long long)nin * cvt->dst_rate / cvt->src_rate);
    int maxout = cvt->len * cvt->len_mult / (2 * cvt->dst_channels);
    if (nout > maxout) nout = maxout;
    Sint16 *tmp = malloc((size_t)(nout ? nout : 1) * 2 * (size_t)cvt->dst_channels);
    if (!tmp) return -1;
    const Uint8 *in = cvt->buf;
    for (int i = 0; i < nout; i++) {
        int j = (int)((long long)i * cvt->src_rate / cvt->dst_rate);
        int v;
        if (in_bps == 1)
            v = (cvt->src_format == AUDIO_U8 ? (int)in[j * in_frame] - 128 : (Sint8)in[j * in_frame]) << 8;
        else
            v = ((const Sint16 *)in)[j * cvt->src_channels];
        for (int c = 0; c < cvt->dst_channels; c++) tmp[i * cvt->dst_channels + c] = (Sint16)v;
    }
    memcpy(cvt->buf, tmp, (size_t)nout * 2 * (size_t)cvt->dst_channels);
    free(tmp);
    cvt->len_cvt = nout * 2 * cvt->dst_channels;
    return 0;
}
