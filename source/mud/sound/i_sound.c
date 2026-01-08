/*
==============================================================================

                                 DOOM Retro
           The classic, refined DOOM source port. For Windows PC.

==============================================================================

    Copyright © 1993-2025 by id Software LLC, a ZeniMax Media company.
    Copyright © 2013-2025 by Brad Harding <mailto:brad@doomretro.com>.

    This file is a part of DOOM Retro.

    DOOM Retro is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the license, or (at your
    option) any later version.

    DOOM Retro is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DOOM Retro. If not, see <https://www.gnu.org/licenses/>.

    DOOM is a registered trademark of id Software LLC, a ZeniMax Media
    company, in the US and/or other countries, and is used without
    permission. All other trademarks are the property of their respective
    holders. DOOM Retro is in no way affiliated with nor endorsed by
    id Software.

==============================================================================
*/

#include "atomix.h"
#include "console/c_console.h"
#include "dr_wav.h"
#include "system/i_system.h"
#include "system/i_config.h"
#include "sound/s_sound.h"
#include "sokol_audio.h"
#include "sokol_log.h"
// clang-format off
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.h"
// clang-format on
#include "thread.h"
#include "system/i_version.h"
#include "wad/w_wad.h"

#define DMXPADSIZE 16

typedef struct allocated_sound_s
{
    sfxinfo_t* sfxinfo;
    struct atomix_sound *chunk;
    struct allocated_sound_s* prev;
    struct allocated_sound_s* next;
} allocated_sound_t;

thread_atomic_int_t sound_initialized;

static allocated_sound_t* channels_playing[s_channels_max];

thread_atomic_ptr_t mixer;
int mixer_freq = 0;

// Doubly-linked list of allocated sounds.
// When a sound is played, it is moved to the head, so that the oldest sounds not used recently are at the tail.
static allocated_sound_t* allocated_sounds_head;
static allocated_sound_t* allocated_sounds_tail;

// Used for decoding and expansion of sound effects prior to being sent to atomix.
// Both expansion_buffer_size and the new_size parameter are in bytes.
static float* expansion_buffer = NULL;
static size_t expansion_buffer_size = 0;

static void ResizeExpansionBuffer(size_t new_size)
{
    if (new_size == 0)
        I_Error("ResizeExpansionBuffer: Passed new size of 0!\n");
    if(expansion_buffer == NULL)
    {
        expansion_buffer_size = new_size;
        expansion_buffer = malloc(expansion_buffer_size);
        if(expansion_buffer == NULL)
            I_Error("ResizeExpansionBuffer: Failed to allocate expansion buffer!\n");
    }
    else if(expansion_buffer_size < new_size)
    {
        expansion_buffer_size = new_size;
        float *new_buffer = realloc(expansion_buffer, expansion_buffer_size);
        if(new_buffer == NULL)
        {
            free(expansion_buffer);
            I_Error("ResizeExpansionBuffer: Failed to reallocate expansion buffer!\n");
        }
        expansion_buffer = new_buffer;
    }
}

// Hook a sound into the linked list at the head.
static void AllocatedSoundLink(allocated_sound_t* snd)
{
    snd->prev = NULL;

    snd->next             = allocated_sounds_head;
    allocated_sounds_head = snd;

    if(!allocated_sounds_tail)
        allocated_sounds_tail = snd;
    else
        snd->next->prev = snd;
}

// Unlink a sound from the linked list.
static void AllocatedSoundUnlink(allocated_sound_t* snd)
{
    if(!snd->prev)
        allocated_sounds_head = snd->next;
    else
        snd->prev->next = snd->next;

    if(!snd->next)
        allocated_sounds_tail = snd->prev;
    else
        snd->next->prev = snd->prev;
}

static void AudioStreamCallback(float *buffer, int num_frames, int num_channels) 
{
    if (thread_atomic_int_load(&sound_initialized))
    {
        struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);
        if(mix)
            atomixMixerMix(mix, buffer, num_frames);
        else
            memset(buffer, 0, num_frames * num_channels * sizeof(float));
    }
    else
        memset(buffer, 0, num_frames * num_channels * sizeof(float));
}

static inline float ConvertDoomPanning(const int sep)
{
    return (float)(sep - 127) / 127;
}

static void FreeAllocatedSound(allocated_sound_t* snd)
{
    // Unlink from linked list.
    AllocatedSoundUnlink(snd);
    atomixSoundFree(snd->chunk);
    free(snd);
}

// Allocate a block for a new sound effect.
static allocated_sound_t* AllocateSound(sfxinfo_t* sfxinfo)
{
    allocated_sound_t* snd = calloc(1, sizeof(allocated_sound_t));
    if (!snd)
    {
        I_Error("AllocateSound: Memory allocation failed!");
    }
    snd->sfxinfo   = sfxinfo;
    AllocatedSoundLink(snd);
    return snd;
}

static allocated_sound_t*
GetAllocatedSoundBySfxInfo(const sfxinfo_t* sfxinfo)
{
    allocated_sound_t* p = allocated_sounds_head;

    while(p)
    {
        if(p->sfxinfo == sfxinfo)
            return p;

        p = p->next;
    }

    return NULL;
}

// When a sound stops, check if it is still playing. If it is not, we can mark
// the sound data as CACHE to be freed back for other means.
static void ReleaseSoundOnChannel(const int channel, const int handle)
{
    allocated_sound_t* snd = channels_playing[channel];
    struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);

    if(!snd || !mix || atomixMixerSetSoundState(mix, handle, ATOMIX_STOP) != 1)
        return;

    channels_playing[channel] = NULL;
}

// Load and convert a sound effect
// Returns true if successful
bool CacheSFX(sfxinfo_t* sfxinfo)
{
    // Need to load the sound
    const int lumpnum = sfxinfo->lumpnum;
    byte* data        = W_CacheLumpNum(lumpnum);
    const int lumplen = W_LumpLength(lumpnum);

    // Check the header, and ensure this is a valid sound
    if(lumplen > 4 && (!memcmp(data, "RIFF", 4) || !memcmp(data, "RIFX", 4) || !memcmp(data, "riff", 4) || !memcmp(data, "RF64", 4) || !memcmp(data, "FORM", 4)))
    {
        drwav wav;
        if (!drwav_init_memory(&wav, data, lumplen, NULL))
            return false;
        if (wav.channels < 1 || wav.channels > 2 || wav.totalPCMFrameCount <= 0 || wav.sampleRate == 0)
        {
            drwav_uninit(&wav);
            return false;
        }
        allocated_sound_t* snd = AllocateSound(sfxinfo);
        ResizeExpansionBuffer(wav.totalPCMFrameCount * wav.channels * sizeof(float));
        size_t frames_read = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, expansion_buffer);
        if (frames_read == 0)
        {
            FreeAllocatedSound(snd);
            drwav_uninit(&wav);
            return false;
        }
        if (wav.sampleRate == mixer_freq)
            snd->chunk = atomixSoundNew(wav.channels, expansion_buffer, frames_read);
        else
            snd->chunk = atomixSoundNewResampled(thread_atomic_ptr_load(&mixer), wav.channels, expansion_buffer, frames_read, wav.sampleRate, ATOMIX_F32);
        drwav_uninit(&wav);
        return true;
    }
    else if(lumplen > 4 && !memcmp(data, "OggS", 4))
    {
        int ogg_error = 0;
        struct stb_vorbis *ogg = stb_vorbis_open_memory(data, lumplen, &ogg_error, NULL);
        if (ogg_error || !ogg)
        {
            if (ogg)
                stb_vorbis_close(ogg);
            return false;
        }
        stb_vorbis_info info = stb_vorbis_get_info(ogg);
        if (info.sample_rate == 0 || info.channels <= 0)
        {
            stb_vorbis_close(ogg);
            return false;
        }
        allocated_sound_t* snd = AllocateSound(sfxinfo);
        int channels = info.channels;
        // stb_vorbis 'coerces' the true number of channels present in an Ogg file to fulfill
        // the requested channel count, so it is safe to assign a channel count of 2 to an
        // Ogg sound that actually has more than 2 channels
        if (channels > 2) channels = 2;
        // NOTE: stb_vorbis uses "samples" to mean "frames" (i.e., one sample per channel).
        // stb_vorbis_stream_length_in_samples() returns the number of frames.
        // stb_vorbis_get_samples_float_interleaved() also returns frame count, not sample count.
        size_t frames = stb_vorbis_stream_length_in_samples(ogg);
        ResizeExpansionBuffer(frames * channels * sizeof(float));
        size_t frames_read = stb_vorbis_get_samples_float_interleaved(ogg, channels, expansion_buffer, frames * channels);
        if (info.sample_rate == mixer_freq)
            snd->chunk = atomixSoundNew(channels, expansion_buffer, frames_read);
        else
            snd->chunk = atomixSoundNewResampled(thread_atomic_ptr_load(&mixer), channels, expansion_buffer, frames_read, info.sample_rate, ATOMIX_F32);
        stb_vorbis_close(ogg);
        return true;
    }
    else if(lumplen >= 8 && data[0] == 0x03 && data[1] == 0x00)
    {
        const int length =
        (data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24));

        // If the header specifies that the length of the sound is greater than
        // the length of the lump itself, this is an invalid sound lump.

        // We also discard sound lumps that are less than 49 samples long, as this is how DMX behaves -
        // although the actual cut-off length seems to vary slightly depending on the sample rate. This
        // needs further investigation to better understand the correct behavior.
        if(length > 48 && length <= lumplen - 8)
        {
            allocated_sound_t* snd = AllocateSound(sfxinfo);
            snd->chunk = atomixSoundNewResampled(thread_atomic_ptr_load(&mixer), 1, data+DMXPADSIZE, length - DMXPADSIZE, (data[2] | (data[3] << 8)), ATOMIX_U8);
            return true;
        }
    }
    return false;
}

void I_UpdateSoundParms(const int handle, const int vol, const int sep)
{
    struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);
    if (mix)
        atomixMixerSetSoundGainPan(mix, handle, ConvertDoomVolume(vol), ConvertDoomPanning(sep));
}

//
// Starting a sound means adding it to the current list of active sounds in the internal channels.
// As the SFX info struct contains e.g. a pointer to the raw data, it is ignored.
// As our sound handling does not handle priority, it is ignored.
//
int I_StartSound(const sfxinfo_t* sfxinfo, const int channel, const int handle, const int vol, const int sep)
{
    allocated_sound_t* snd;

    // Release a sound effect if there is already one playing on this channel.
    ReleaseSoundOnChannel(channel, handle);

    if(!(snd = GetAllocatedSoundBySfxInfo(sfxinfo)))
        return -1;

    struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);
    if (!mix)
        return -1;

    // Play sound
    uint32_t new_handle = atomixMixerPlaySound(mix, snd->chunk, ATOMIX_PLAY,
                                  ConvertDoomVolume(vol), ConvertDoomPanning(sep));
    if (new_handle == 0)
        return -1;

    channels_playing[channel] = snd;
    return new_handle;
}

void I_StopSound(const int channel, const int handle)
{
    // Sound data is no longer needed; release the sound data being used for this channel.
    ReleaseSoundOnChannel(channel, handle);
}

bool I_SoundIsPlaying(const int handle)
{
    struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);
    if (mix)
        return atomixMixerGetSoundState(mix, handle) > ATOMIX_FREE;
    else
        return false;
}

bool I_AnySoundStillPlaying(void)
{
    struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);
    if (mix)
        return atomixMixerGetActive(mix) > 0;
    else
        return false;
}

void I_ShutdownSound(void)
{
    if(!thread_atomic_int_load(&sound_initialized))
        return;

    thread_atomic_int_store(&sound_initialized, 0);
    struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);
    thread_atomic_ptr_store(&mixer, NULL);
    atomixMixerFree(mix);
    saudio_shutdown();
    free(expansion_buffer);
    expansion_buffer = NULL;
}

bool I_InitSound(void)
{
    // No sounds yet
    for(int i = 0; i < s_channels_max; i++)
        channels_playing[i] = NULL;

    thread_atomic_int_store(&sound_initialized, 0);

    thread_atomic_ptr_store(&mixer, NULL);

    saudio_setup(&(saudio_desc){
        .sample_rate   = SAMPLERATE,
        .stream_cb = AudioStreamCallback,
        .num_channels = 2,
        .logger.func = slog_func,
    });

    mixer_freq = saudio_sample_rate();

    if(!saudio_isvalid() || mixer_freq <= 0 || saudio_channels() < 2) 
    {
        saudio_shutdown();
        return false;
    }

    struct atomix_mixer *mix = atomixMixerNew(1.0f, 0, mixer_freq);

    if(!mix)
    {
        saudio_shutdown();
        return false;
    }
    else
        thread_atomic_ptr_store(&mixer, mix);

    thread_atomic_int_store(&sound_initialized, 1);

    return true;
}
