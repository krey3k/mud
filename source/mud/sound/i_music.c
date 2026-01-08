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
#include "doom/doomstat.h"
#include "system/i_config.h"
#include "menu/m_menu.h"
#include "utils/m_misc.h"
#include "mtlib.h"
#include "sound/s_sound.h"
// clang-format off
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.h"
// clang-format on
#include "thread.h"
#include "wad/w_wad.h"
#include "utils/z_zone.h"

extern thread_atomic_int_t sound_initialized;
extern thread_atomic_ptr_t mixer;

musictype_e musictype = MUS_INVALID;
static bool music_initialized = false;
static uint8_t *midi_bank = NULL;
static size_t midi_bank_size = 0;

// Forward declaration for music decoder callbacks
static void MUDTracker_Free(void *userdata);
static void MUDTracker_Volume(void *userdata, float volume);
static void MUDTracker_Render(void *userdata, float *buffer, uint32_t samples);
static void Ogg_Free(void *userdata);
static void Ogg_Render(void *userdata, float *buffer, uint32_t samples);

// Shutdown music
void I_ShutdownMusic(void)
{
    if(!music_initialized)
        return;

    music_initialized = false;

    if(mus_playing)
    {
        I_StopSong();
        I_UnregisterSong(mus_playing->stream);
    }

    if(midi_bank)
        free(midi_bank);
}

// Initialize music subsystem
bool I_InitMusic(void)
{
    if (nomusic || !thread_atomic_int_load(&sound_initialized))
        return false;

    const char *resourcefolder = M_GetResourceFolder();
    char *bankfile = M_StringJoin(resourcefolder, DIR_SEPARATOR_S, "assets/sound/default.mdtb", NULL);
    
    if (!M_FileExists(bankfile))
    {
        free(bankfile);
        return false;
    }

    fs_file_info bank_info;
    if (FS_GetInfo(&bank_info, bankfile, FS_TRUE) != FS_SUCCESS || !bank_info.size)
    {
        free(bankfile);
        return false;
    }

    fs_file *bank_handle = FS_OpenFile(bankfile, FS_READ, FS_TRUE);
    free(bankfile);
    
    if (!bank_handle)
        return false;

    midi_bank_size = bank_info.size;
    midi_bank = malloc(midi_bank_size);
    if (!midi_bank)
    {
        FS_CloseFile(bank_handle);
        return false;
    }

    if (FS_Read(midi_bank, midi_bank_size, 1, bank_handle) != 1)
    {
        FS_CloseFile(bank_handle);
        free(midi_bank);
        midi_bank = NULL;
        return false;
    }

    FS_CloseFile(bank_handle);
    music_initialized = true;
    return true;
}

// Set music volume (0 - 31)
void I_SetMusicVolume(const int volume)
{
    if (!music_initialized) 
        return;
    if (mus_playing)
    {
        struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);
        if (mix)
            atomixMixerSetStreamGain(mix, mus_playing->handle, ConvertDoomVolume(volume));
    }
}

// Start playing a mid
void I_PlaySong(void* handle, const bool looping)
{
    if(!music_initialized)
        return;
    musicinfo_t *song = (musicinfo_t *)handle;
    if (!song) return;
    struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);
    if (mix)
        song->handle = atomixMixerPlayStream(mix, song->stream, ATOMIX_PLAY, ConvertDoomVolume(s_musicvolume));
}

void I_PauseSong(void)
{
    if(!music_initialized)
        return;
    if (mus_playing)
    {
        struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);
        if (mix)
            atomixMixerSetStreamState(mix, mus_playing->handle, ATOMIX_HALT);
    }
}

void I_ResumeSong(void)
{
    if(!music_initialized)
        return;
    if (mus_playing)
    {
        struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);
        if (mix)
            atomixMixerSetStreamState(mix, mus_playing->handle, ATOMIX_PLAY);
    }
}

void I_StopSong(void)
{
    if(!music_initialized)
        return;
    if (mus_playing)
    {
        struct atomix_mixer *mix = thread_atomic_ptr_load(&mixer);
        if (mix)
        {
            atomixMixerSetStreamState(mix, mus_playing->handle, ATOMIX_STOP);
            mus_playing->handle = 0;
            // stream will be freed asynchronously by mixer in next mix cycle
            mus_playing->stream = NULL;
        }
    }
}

void I_UnregisterSong(void* stream)
{
    if(!music_initialized)
        return;
    if (stream)
    {
        atomixStreamFree(stream);
    }
}

void *I_RegisterSong(void* data, int size)
{
    if(!music_initialized)
        return NULL;
    musictype = MUS_INVALID;
    if(size > 4)
    {
        if(!memcmp(data, "MThd", 4)) // is it a MIDI?
            musictype = MUS_MIDI;
        else if(!memcmp(data, "MUS\x1A", 4)) // is it a MUS?
            musictype = MUS_MUS;
        else if(!memcmp(data, "MDTS", 4)) // is it a MUDTracker song?
            musictype = MUS_MDTS;
        else if (!memcmp(data, "OggS", 4)) // is it an Ogg song?
            musictype = MUS_OGG;
    }
    switch (musictype)
    {
        case MUS_MUS:
        case MUS_MIDI:
        case MUS_MDTS:
        {
            mtsynth *synth = mt_create(mixer_freq);
            if (!synth) return NULL;
            if (musictype != MUS_MDTS)
            {
                if (!midi_bank || !midi_bank_size)
                {
                    mt_destroy(synth);
                    return NULL;
                }
                synth->totalFileSize = midi_bank_size;
                if (mt_loadInstrumentBankFromMemory(synth, (char *)midi_bank) != 0)
                {
                    mt_destroy(synth);
                    return NULL;
                }
            }
            if (mt_loadSongFromMemory(synth, (char *)data, size) != 0)
            {
                mt_destroy(synth);
                return NULL;
            }
            else
            {
                struct atomix_stream_callbacks cb;
                cb.free = MUDTracker_Free;
                cb.render = MUDTracker_Render;
                cb.volume = MUDTracker_Volume;
                struct atomix_stream *strm = atomixStreamNew(synth, &cb, mixer_freq);
                if (!strm)
                {
                    mt_destroy(synth);
                    return NULL;
                }
                mt_play(synth); // prep for playback
                return strm;
            }
            break;
        }
        case MUS_OGG:
        {
            int ogg_error = 0;
            struct stb_vorbis *ogg = stb_vorbis_open_memory(data, size, &ogg_error, NULL);
            if (ogg_error || !ogg)
            {
                if (ogg)
                    stb_vorbis_close(ogg);
                return NULL;
            }
            else
            {
                struct atomix_stream_callbacks cb;
                cb.free = Ogg_Free;
                cb.render = Ogg_Render;
                cb.volume = NULL;
                stb_vorbis_info info = stb_vorbis_get_info(ogg);
                if (info.sample_rate == 0)
                {
                    stb_vorbis_close(ogg);
                    return NULL;
                }
                struct atomix_stream *strm = atomixStreamNew(ogg, &cb, info.sample_rate);
                if (!strm)
                {
                    stb_vorbis_close(ogg);
                    return NULL;
                }
                return strm;
            }
        }
        default:
            return NULL;
    }
}

static void MUDTracker_Free(void *userdata)
{
    if (userdata)
    {
        mtsynth *mt = (mtsynth *)userdata;
        mt_destroy(mt);
    }
}
static void MUDTracker_Volume(void *userdata, float volume)
{
    if (userdata)
    {
        mtsynth *mt = (mtsynth *)userdata;
        mt_setPlaybackVolume(mt, (int)(volume * 100));
    }
}
static void MUDTracker_Render(void *userdata, float *buffer, uint32_t samples)
{
    if (userdata && buffer)
    {
        mtsynth *mt = (mtsynth *)userdata;
        mt_render(mt, buffer, samples * 2, MT_RENDER_FLOAT);
    }
    else if (buffer)
        memset(buffer, 0, samples * 2 * sizeof(float));
}

static void Ogg_Free(void *userdata)
{
    if (userdata)
    {
        stb_vorbis *ogg = (stb_vorbis *)userdata;
        stb_vorbis_close(ogg);
    }
}
static void Ogg_Render(void *userdata, float *buffer, uint32_t samples)
{
    if (userdata && buffer)
    {
        stb_vorbis *ogg = (stb_vorbis *)userdata;
        int samples_read = stb_vorbis_get_samples_float_interleaved(ogg, 2, buffer, samples * 2);
        // Try to loop if we did not get enough samples
        if (samples_read < (int)samples) 
        {
            int frames_remaining = (int)samples - samples_read;
            stb_vorbis_seek_start(ogg);
            int samples_read2 = stb_vorbis_get_samples_float_interleaved(ogg, 2, buffer + samples_read * 2, frames_remaining * 2);
            int total_read = samples_read + samples_read2;
            // If we still didn't get enough samples, just zero out the remaining buffer
            if (total_read < (int)samples)
                memset(buffer + total_read * 2, 0, ((int)samples - total_read) * 2 * sizeof(float));
        }
    }
    else if (buffer)
        memset(buffer, 0, samples * 2 * sizeof(float));
}
