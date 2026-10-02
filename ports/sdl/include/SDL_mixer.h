/* NixDOS 2 - SDL_mixer-compatible audio mixer (subset used by Wolf4SDL),
 * playing through the kernel's Sound Blaster 16 driver. */
#ifndef NIXDOS_SDL_MIXER_H
#define NIXDOS_SDL_MIXER_H
#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MIX_CHANNELS 8
#define MIX_MAX_VOLUME 128
#define MIX_DEFAULT_FORMAT AUDIO_S16SYS

typedef struct Mix_Chunk {
    int allocated;
    Uint8 *abuf;
    Uint32 alen;
    Uint8 volume;
} Mix_Chunk;

typedef void (*Mix_MixFunc)(void *udata, Uint8 *stream, int len);

int  Mix_OpenAudioDevice(int frequency, Uint16 format, int channels, int chunksize,
                         const char *device, int allowed_changes);
int  Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
void Mix_CloseAudio(void);
int  Mix_QuerySpec(int *frequency, Uint16 *format, int *channels);
int  Mix_AllocateChannels(int n);
int  Mix_ReserveChannels(int n);
int  Mix_GroupChannels(int from, int to, int tag);
int  Mix_GroupAvailable(int tag);
int  Mix_GroupOldest(int tag);
int  Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops);
int  Mix_HaltChannel(int channel);
int  Mix_Playing(int channel);
int  Mix_SetPanning(int channel, Uint8 left, Uint8 right);
void Mix_HookMusic(Mix_MixFunc fn, void *arg);
void Mix_SetPostMix(Mix_MixFunc fn, void *arg);
void Mix_ChannelFinished(void (*fn)(int channel));
const char *Mix_GetError(void);

/* NixDOS: keep the sound card fed; called from the event/timer functions */
void NixAudio_Pump(void);

#ifdef __cplusplus
}
#endif
#endif
