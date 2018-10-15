//
// Created by domen on 15. 10. 2018.
//

#ifndef AUDACIOUS1_SONGCHANGE_CROSSPLATFORM_H
#define AUDACIOUS1_SONGCHANGE_CROSSPLATFORM_H

#include <libaudcore/runtime.h>

#ifdef _WIN32

#include <string>
#include <windows.h>

std::string utf16_to_utf8(const std::wstring &utf16)
{
    // Special case of empty input string
    if (utf16.empty())
    {
        // Return empty string
        return {};
    }


    // "Code page" value used with WideCharToMultiByte() for UTF-8 conversion
    const UINT codePageUtf8 = CP_UTF8;

    // Safely fails if an invalid UTF-16 character is encountered
    const DWORD flags = WC_ERR_INVALID_CHARS;

    // Get the length, in chars, of the resulting UTF-8 string
    const int utf8Length = ::WideCharToMultiByte(
            codePageUtf8,       // convert to UTF-8
            flags,              // conversion flags
            utf16.c_str(),  // source UTF-16 string
            utf16.size(),  // length of source UTF-16 string, in WCHARs
            nullptr,            // unused - no conversion required in this step
            0,                  // request size of destination buffer, in chars
            nullptr, nullptr) + 1;  // unused
    if (utf8Length == 1)
    {
        // Conversion error
        return {};
    }

    // Allocate destination buffer to store the resulting UTF-8 string
    char* bytes = new char[utf8Length]{0};

    // Do the conversion from UTF-16 to UTF-8
    int result = ::WideCharToMultiByte(
            codePageUtf8,       // convert to UTF-8
            flags,              // conversion flags
            utf16.c_str(),  // source UTF-16 string
            utf16.size(),  // length of source UTF-16 string, in WCHARs
            bytes,         // pointer to destination buffer
            utf8Length,         // size of destination buffer, in chars
            nullptr, nullptr);  // unused
    if (result == 0)
    {
        return {};
    }

    auto returning = std::string(bytes);
    delete [] bytes;
    // Return resulting UTF-8 string
    return returning;
}

std::wstring utf8_to_utf16(const std::string &utf8) {
    int wchars_num = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    auto *wstr = new wchar_t[wchars_num];
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wstr, wchars_num);
    std::wstring result(wstr);
    delete[] wstr;
    return result;
}

static void signal_child() {
   // signal (SIGCHLD, SIG_DFL);
}


//static void bury_child (int signal){
//    // noop for now
//}

static void execute_command (const char * cmd){
    const std::wstring fileToRun = utf8_to_utf16(cmd);
    STARTUPINFOW si;
    _PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    unsigned long flags = CREATE_UNICODE_ENVIRONMENT |  CREATE_NO_WINDOW;
    auto * windows_cmd = const_cast<wchar_t *>(fileToRun.c_str());
    BOOL executed = ::CreateProcessW(NULL, windows_cmd, NULL, NULL, TRUE, flags, NULL, NULL, &si, &pi);

    if (!executed) {
        AUDINFO((std::string("SongChange cannot run the command: ") + cmd + "\n").c_str());
    }
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
