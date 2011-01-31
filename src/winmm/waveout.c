/*
 * MS-Windows WaveMapper Output Plugin
 * Copyright (c) 2010-2011 Carlo Bramini
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <audacious/debug.h>
#include <audacious/configdb.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

static gboolean winmm_init (void)
{
/* TODO: Read UID to be used */
    return TRUE;
}

static void winmm_about(void)
{
/* TODO*/
}

static void winmm_configure(void)
{
/* TODO: Select UID */
}

#define _WINMM_SIZE_IN_MS   50
#define _WINMM_NUM_OF_BUF   10

static struct {
    HWAVEOUT    hWaveOut;
    UINT        uiBufferSize;
    BOOL        bIsRunning;
    UINT        CurrentBlock;
    UINT        FreeSize;
    LPBYTE      OutData;
    UINT        PlayingSize;
    DWORD       LastVolume;
    UINT        BaseTime;
    WAVEFORMATEX wfx;
    CRITICAL_SECTION LockPlaying;
    WAVEHDR     WaveHeader[_WINMM_NUM_OF_BUF];
    HANDLE      hBlockBusy[_WINMM_NUM_OF_BUF];
    PVOID       pSoundBuffer[_WINMM_NUM_OF_BUF];
} wave;

static  void CALLBACK waveOutProc(HWAVEOUT hwo,UINT uMsg,DWORD dwInstance,DWORD dwParam1,DWORD dwParam2)
{
    if (uMsg == WOM_DONE) {
        WAVEHDR *Block = (WAVEHDR *)dwParam1;

        // New buffer in playback
        EnterCriticalSection(&wave.LockPlaying);
        wave.PlayingSize -= wave.uiBufferSize;
        LeaveCriticalSection(&wave.LockPlaying);

        SetEvent((HANDLE)Block->dwUser);
    }
}

static int winmm_open(gint fmt, int rate, int nch)
{
    WAVEFORMATEX wfx;
    MMRESULT     errCode;
    int          i;

    AUDDBG ("winmm_open(%d,%d,%d)\n",fmt,rate,nch);

    // Bits per sample
    switch (fmt) {
    case FMT_U8:     wfx.wBitsPerSample = 8;  break;
    case FMT_S16_LE: wfx.wBitsPerSample = 16; break;
    case FMT_S24_LE: wfx.wBitsPerSample = 24; break;
    default:
        return 0;
    }

    wfx.wFormatTag      = WAVE_FORMAT_PCM;  // PCM standard.
    wfx.nChannels       = nch;              // mono/stereo
    wfx.nSamplesPerSec  = rate;
    wfx.nBlockAlign     = (wfx.wBitsPerSample/8) * wfx.nChannels;
    wfx.nAvgBytesPerSec = rate * wfx.nBlockAlign;
    wfx.cbSize          = 0;

    wave.uiBufferSize   = (wfx.nAvgBytesPerSec * _WINMM_SIZE_IN_MS / 1000UL) & ~3;

    errCode = waveOutOpen(&wave.hWaveOut,
                          WAVE_MAPPER,
                          &wfx,
                          (DWORD)waveOutProc,
                          (DWORD)NULL, // User data.
                          (DWORD)CALLBACK_FUNCTION);

    if (errCode != MMSYSERR_NOERROR)
        return 0;

    for (i=0; i<_WINMM_NUM_OF_BUF; i++)
    {
        // Allocate the buffer
        wave.pSoundBuffer[i] = HeapAlloc(GetProcessHeap(),
                                         HEAP_ZERO_MEMORY,
                                         wave.uiBufferSize);

        // Clear the heared content
        ZeroMemory(&wave.WaveHeader[i], sizeof(WAVEHDR));

        // Allocate an handle for each block
        wave.hBlockBusy[i] = CreateEvent(NULL, FALSE, TRUE, NULL);

        // Copy the handle in the wave header struct too
        wave.WaveHeader[i].dwUser = (DWORD)wave.hBlockBusy[i];
    }

    // Initialize object for incrementing Play size
    InitializeCriticalSection(&wave.LockPlaying);

    // Engine is running
    wave.bIsRunning = TRUE;

    // First buffer is surely free
    wave.CurrentBlock = 0;
    wave.OutData = wave.pSoundBuffer[0];
    wave.FreeSize = wave.uiBufferSize;

    // Save obtained wfx structure
    wave.wfx = wfx;

    // Start from zero!
    wave.BaseTime = 0;

    return 1;
}

static void winmm_write(void *pSound, int dwCopySize)
{
    DWORD  dwFreeSize = wave.FreeSize;
    LPBYTE OutData    = (LPBYTE)wave.OutData;

    do
    {
        if (dwCopySize > dwFreeSize)
        {
            UINT     Current = wave.CurrentBlock;
            WAVEHDR *Header  = wave.WaveHeader + Current;
            DWORD    dwWaited;

            // Copy data in the buffer
            memcpy(OutData, pSound, dwFreeSize);

            pSound += dwFreeSize;

            // Update the remaining size to copy
            dwCopySize -= dwFreeSize;

            if (Header->dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(wave.hWaveOut, Header, sizeof(WAVEHDR));

            Header->lpData         = (LPSTR)wave.pSoundBuffer[Current];
            Header->dwBufferLength = wave.uiBufferSize;
            waveOutPrepareHeader(wave.hWaveOut, Header, sizeof(WAVEHDR));

            // Resets the event
            ResetEvent(wave.hBlockBusy[Current]);

            // New buffer in playback
            EnterCriticalSection(&wave.LockPlaying);
            wave.PlayingSize += wave.uiBufferSize;
            LeaveCriticalSection(&wave.LockPlaying);

            // Send the buffer the the WaveOut queue
            waveOutWrite(wave.hWaveOut, Header, sizeof(WAVEHDR));

            // wait a new free block before continuing
            dwWaited = WaitForMultipleObjects(_WINMM_NUM_OF_BUF, wave.hBlockBusy, FALSE, 1000);

            if (dwWaited == WAIT_TIMEOUT || dwWaited == WAIT_FAILED)
                return;

            dwFreeSize = wave.uiBufferSize;
            wave.CurrentBlock = dwWaited;
            OutData = wave.pSoundBuffer[dwWaited];
        }
        else
        {
            // Copy data in the buffer
            memcpy(OutData, pSound, dwCopySize);

            // Update the remaining size to copy
            dwFreeSize -= dwCopySize;

            // Update pointer to OutData
            OutData += dwCopySize;

            break;
        }
    } while (dwCopySize);

    wave.OutData = OutData;
    wave.FreeSize = dwFreeSize;
}

static void winmm_close(void)
{
    if (wave.bIsRunning) {
        int i;

        wave.bIsRunning = FALSE;
        waveOutReset(wave.hWaveOut);                    // Reset tout.
        while (waveOutClose(wave.hWaveOut) == WAVERR_STILLPLAYING)
            Sleep(50);

        for (i=0; i<_WINMM_NUM_OF_BUF; i++)
        {
            if (wave.WaveHeader[i].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(wave.hWaveOut, &wave.WaveHeader[i], sizeof(WAVEHDR));

            free(wave.pSoundBuffer[i]);

            // Free the handle associated to each block
            CloseHandle(wave.hBlockBusy[i]);
        }
    }
}

static void winmm_flush(int timing)
{
    if (wave.bIsRunning) {
        waveOutReset(wave.hWaveOut);
        wave.BaseTime = timing;
    }
}

static void winmm_pause (gboolean paused)
{
    if (wave.bIsRunning) {
        if (paused)
            waveOutPause(wave.hWaveOut);
        else
            waveOutRestart(wave.hWaveOut);
    }
}

static int winmm_buffer_free(void)
{
    if (wave.bIsRunning) {
        return (wave.uiBufferSize * _WINMM_NUM_OF_BUF) - wave.PlayingSize;
    }

    return 0;
}

static int winmm_get_written_time(void)
{
    return (wave.PlayingSize * 1000UL) / wave.wfx.nAvgBytesPerSec;
}

static int winmm_get_output_time(void)
{
    MMTIME        mmt;
    MMRESULT      mmr;
    DWORD         dwTime;
    LARGE_INTEGER li;

    if (!wave.bIsRunning)
        return 0;

    dwTime = 0;

    ZeroMemory(&mmt, sizeof(MMTIME));
    mmt.wType = TIME_MS;
    
    mmr = waveOutGetPosition(wave.hWaveOut, &mmt, sizeof(MMTIME));
    if (mmr == MMSYSERR_NOERROR) {

        switch (mmt.wType) {
        case TIME_MS:
            dwTime = mmt.u.ms;
            break;

        case TIME_BYTES:
            li.u.LowPart  = mmt.u.cb;
            li.u.HighPart = 0;
            dwTime = (li.QuadPart * 1000UL) / wave.wfx.nAvgBytesPerSec;
            break;

        case TIME_SAMPLES:
            li.u.LowPart  = mmt.u.sample;
            li.u.HighPart = 0;
            dwTime = (li.QuadPart * 1000UL) / wave.wfx.nSamplesPerSec;
            break;

        case TIME_SMPTE:
            dwTime = ((mmt.u.smpte.hour * 3600UL) +
                      (mmt.u.smpte.min  *   60UL) +
                      (mmt.u.smpte.sec          ) +
                      (mmt.u.smpte.frame / 30UL)) * 1000UL;
            break;

        default:
            // Unknown?!?!?
            break;
        }
    }

    return wave.BaseTime + dwTime;
}

static void winmm_get_volume(gint *r, gint *l)
{
    DWORD    dwVolume;
    MMRESULT mmr;

    if (!wave.bIsRunning)
        dwVolume = wave.LastVolume;
    else
    {
        mmr = waveOutGetVolume(wave.hWaveOut, &dwVolume);
        if (mmr != MMSYSERR_NOERROR)
        {
            dwVolume = wave.LastVolume;
        }
    }

    *r = (HIWORD(dwVolume) * 100UL) / 65535;
    *l = (LOWORD(dwVolume) * 100UL) / 65535;
}

static void winmm_set_volume(gint r, gint l)
{
    wave.LastVolume = MAKELONG((l * 65535)/100UL, (r * 65535)/100UL);

    if (!wave.bIsRunning)
        return;

    waveOutSetVolume(wave.hWaveOut, wave.LastVolume);
}

static OutputPlugin winmm_op =
{
    .description = "Windows Waveout Plugin",
    .probe_priority = 0,
    .init           = winmm_init,
    .about          = winmm_about,
    .configure      = winmm_configure,
    .open_audio     = winmm_open,
    .write_audio    = winmm_write,
    .close_audio    = winmm_close,
    .flush          = winmm_flush,
    .pause          = winmm_pause,
    .buffer_free    = winmm_buffer_free,
    .output_time    = winmm_get_output_time,
    .written_time   = winmm_get_written_time,
    .get_volume     = winmm_get_volume,
    .set_volume     = winmm_set_volume
};

OutputPlugin *winmm_oplist[] = { &winmm_op, NULL };

DECLARE_PLUGIN(null, NULL, NULL, NULL, winmm_oplist, NULL, NULL, NULL, NULL);
