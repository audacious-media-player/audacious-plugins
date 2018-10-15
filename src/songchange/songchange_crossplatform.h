/*
 *  songchange_crossplatform.h
 *  Audacious
 *
 *  Copyright (C) 2005-2018  Audacious team
 *
 *  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2003  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2003  Haavard Kvaalen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
*/

#ifndef AUDACIOUS1_SONGCHANGE_CROSSPLATFORM_H
#define AUDACIOUS1_SONGCHANGE_CROSSPLATFORM_H

#include <libaudcore/runtime.h>

#ifdef _WIN32

#include <windows.h>
#include <glib.h>


static void signal_child() {
   // signal (SIGCHLD, SIG_DFL);
}

static void execute_command(const char *cmd) {
    auto *windows_cmd = reinterpret_cast<wchar_t *>(g_utf8_to_utf16(cmd, -1, nullptr, nullptr, nullptr));
    g_return_if_fail (windows_cmd);

    STARTUPINFOW si;
    _PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    unsigned long flags = CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW;
    BOOL executed = ::CreateProcessW(NULL, windows_cmd, NULL, NULL, TRUE, flags, NULL, NULL, &si, &pi);

    if (!executed) {
        AUDINFO("SongChange cannot run the command: %s\n", cmd);
    }
    g_free(windows_cmd);
}


#else
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

static void signal_child() {
    signal (SIGCHLD, SIG_DFL);
}

static void bury_child (int signal)
{
    waitpid (-1, nullptr, WNOHANG);
}

static void execute_command (const char * cmd)
{
    const char * argv[4] = {"/bin/sh", "-c", nullptr, nullptr};
    argv[2] = cmd;
    signal (SIGCHLD, bury_child);

    if (fork () == 0)
    {
        /* We don't want this process to hog the audio device etc */
        for (int i = 3; i < 255; i ++)
            close (i);
        execv (argv[0], (char * *) argv);
    }
}
#endif

#endif //AUDACIOUS1_SONGCHANGE_CROSSPLATFORM_H
