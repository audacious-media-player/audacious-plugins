/*
 *  windows_window.cc
 *  Audacious
 *
 *  Copyright (C) 2005-2020 Audacious team
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

#include "windows_window.h"

#include <algorithm>
#include <iomanip>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <sstream>
#include <vector>

class Utf16CharArrConverter
{
    gchar * converted{nullptr};

public:
    Utf16CharArrConverter(const wchar_t * const input_w_str)
        : converted(reinterpret_cast<char *>(
              g_utf16_to_utf8(reinterpret_cast<const gunichar2 *>(input_w_str),
                              -1, nullptr, nullptr, nullptr)))
    {
    }
    ~Utf16CharArrConverter()
    {
        if (converted)
        {
            g_free(converted);
        }
    };
    operator std::string()
    { // NOLINT(google-explicit-constructor)
        return std::string(converted);
    }
};

template<typename T>
std::string to_hex_string(T integer)
{
    std::ostringstream os;
    os << "0x" << std::setw(4) << std::setfill('0') << std::hex << integer;
    return os.str();
}

std::string print_wins(const std::vector<WindowsWindow> & wins)
{
    std::string printer;
    for (auto & w : wins)
    {
        printer.append("\n   ").append(static_cast<std::string>(w));
    }
    return printer;
}

WindowsWindow::WindowsWindow(HWND handle, std::string className,
                             std::string winHeader, AudaciousWindowKind kind)
    : handle_(handle), class_name_(std::move(className)),
      win_header_(std::move(winHeader)), kind_(kind)
{
}
std::string WindowsWindow::translated_title()
{
    // get "%s - <translatedAudacious>"
    std::string org = N_("%s - Audacious");
    // erase %s
    org.erase(0, 2);
    return org;
}
bool WindowsWindow::is_main_window(bool allow_hidden) const
{
    if (main_window_hidden_ && !allow_hidden)
        return false;
    bool returning =
        (class_name_ == "gdkWindowToplevel" && [](const std::string & title) {
            return title == N_("Audacious") ||
                   title.find(translated_title()) != std::string::npos ||
                   title == N_("Buffering ..."); // msgid "%s - Audacious"
        }(win_header_));
    if (returning)
    {
        kind_ = AudaciousWindowKind::MAIN_WINDOW;
    }
    return returning;
}
WindowsWindow::operator std::string() const
{
    if (!handle_)
    {
        return "INVALID_WINDOW";
    }
    std::string printer;
    printer.append(std::to_string(reinterpret_cast<long long>(handle_)))
        .append("|")
        .append(to_hex_string(handle_))
        .append("(gdk:")
        .append(std::to_string(reinterpret_cast<long long>(gdk_window())))
        .append("|")
        .append(to_hex_string(gdk_window()))
        .append(") ")
        .append(class_name_)
        .append(": ")
        .append(win_header_);
    return printer;
}
WindowsWindow WindowsWindow::get_window_data(HWND pHwnd)
{
    wchar_t char_buf[311];
    auto num_char = GetClassNameW(pHwnd, char_buf, _countof(char_buf));
    if (!num_char)
    {
        AUDDBG("WinApiError (code %ld): cannot get ClassName for %p.\n",
               GetLastError(), pHwnd);
        return {};
    }
    std::string w_class_name = Utf16CharArrConverter(char_buf);
    num_char = GetWindowTextW(pHwnd, char_buf, _countof(char_buf));
    if (!num_char)
    {
        AUDINFO("WinApiError (code %ld) getting WindowHeader. No header "
                "for %p. Ignoring this window.\n",
                GetLastError(), pHwnd);
        char_buf[0] = 0;
    }
    return {pHwnd, std::move(w_class_name),
            char_buf[0] ? Utf16CharArrConverter(char_buf)
                        : std::string{"[NO_TITLE]"},
            AudaciousWindowKind::UNKNOWN};
}

struct EnumWindowsCallbackArgs
{
    explicit EnumWindowsCallbackArgs(DWORD p) : pid(p) {}
    const DWORD pid;
    std::vector<WindowsWindow> handles;

    void add_window(HWND pHwnd)
    {
        handles.emplace_back(WindowsWindow::get_window_data(pHwnd));
    }
};

static BOOL CALLBACK EnumWindowsCallback(HWND hnd, LPARAM lParam)
{
    // Get pointer to created callback object for storing data.
    auto * args = reinterpret_cast<EnumWindowsCallbackArgs *>(lParam);
    // Callback result are all windows (not only my program). I filter by PID /
    // thread ID.
    DWORD windowPID;
    GetWindowThreadProcessId(hnd, &windowPID);
    // Compare to the one we have stored in our callbaack structure.
    if (windowPID == args->pid)
    {
        args->add_window(hnd);
    }
    return TRUE;
}

std::vector<WindowsWindow> get_this_app_windows()
{
    // Create object that will hold a result.
    EnumWindowsCallbackArgs args(GetCurrentProcessId());
    // AUDERR("Testing getlasterror: %ld\n.", GetLastError());
    // Call EnumWindows and pass pointer to function and created callback
    // structure.
    if (EnumWindows(&EnumWindowsCallback, (LPARAM)&args) == FALSE)
    {
        // If the call fails, return empty vector
        AUDERR("WinApiError (code %ld). Cannot get windows, hotkey plugin "
               "won't work.\n",
               GetLastError());
        return std::vector<WindowsWindow>{};
    }
#ifndef NDEBUG
    AUDDBG("Aud has %zu windows: %s", args.handles.size(),
           print_wins(args.handles).c_str());
#endif
    return std::move(args.handles);
}
WindowsWindow WindowsWindow::find_message_receiver_window()
{
    auto ws = get_this_app_windows();
    auto main_win = std::find_if(ws.begin(), ws.end(), [](WindowsWindow & itm) {
        return itm.is_main_window();
    });
    if (main_win != ws.end())
    {
        main_win->kind_ = AudaciousWindowKind::MAIN_WINDOW;
        return std::move(*main_win);
    }
    main_win =
        std::find_if(ws.begin(), ws.end(), [](const WindowsWindow & itm) {
            return itm.gdk_window() != nullptr;
        });
    if (main_win != ws.end())
    {
        main_win->kind_ =
            AudaciousWindowKind::TASKBAR_WITH_WINHANDLE_AND_GDKHANDLE;
        return std::move(*main_win);
    }
    main_win =
        std::find_if(ws.begin(), ws.end(), [](const WindowsWindow & itm) {
            return itm.class_name_ == "gtkstatusicon-observer";
        });
    if (main_win == ws.end())
    {
        AUDDBG("No proper window found. Giving up.");
        return {nullptr, {}, {}, AudaciousWindowKind::NONE};
    }
    main_win->kind_ = AudaciousWindowKind::GTKSTATUSICON_OBSERVER;
    return std::move(*main_win);
}
WindowsWindow::operator bool() const { return handle_; }
GdkWindow * WindowsWindow::gdk_window() const
{
    if (!gdk_window_)
    {
        gdk_window_ = reinterpret_cast<GdkWindow *>(
            gdk_win32_handle_table_lookup(handle_));
    }
    return gdk_window_;
}
void WindowsWindow::unref()
{
    handle_ = nullptr;
    gdk_window_ = nullptr;
}

#define ENUM_KIND_CASE(item)                                                   \
    case item:                                                                 \
        return #item;

const char * WindowsWindow::kind_string()
{
    switch (kind_)
    {
        ENUM_KIND_CASE(AudaciousWindowKind::MAIN_WINDOW)
        ENUM_KIND_CASE(AudaciousWindowKind::UNKNOWN)
        ENUM_KIND_CASE(AudaciousWindowKind::NONE)
        ENUM_KIND_CASE(AudaciousWindowKind::GTKSTATUSICON_OBSERVER)
        ENUM_KIND_CASE(
            AudaciousWindowKind::TASKBAR_WITH_WINHANDLE_AND_GDKHANDLE)
    }
    return "bad WindowsWindow ; unknown";
}

std::string stringify_win_evt(UINT id)
{
    switch (id)
    {
        ENUM_KIND_CASE(WM_DESTROY)
        ENUM_KIND_CASE(WM_CREATE)
        ENUM_KIND_CASE(WM_ACTIVATE)
        ENUM_KIND_CASE(WM_ENABLE)
        ENUM_KIND_CASE(WM_SHOWWINDOW)
        ENUM_KIND_CASE(WM_WINDOWPOSCHANGING)
        ENUM_KIND_CASE(WM_WINDOWPOSCHANGED)
        ENUM_KIND_CASE(WM_NCACTIVATE)
        ENUM_KIND_CASE(WM_NCPAINT)
        ENUM_KIND_CASE(WM_NCCREATE)
        ENUM_KIND_CASE(WM_ACTIVATEAPP)
        ENUM_KIND_CASE(WM_KILLFOCUS)
        ENUM_KIND_CASE(WM_SETFOCUS)
        ENUM_KIND_CASE(WM_IME_SETCONTEXT)
        ENUM_KIND_CASE(WM_IME_NOTIFY)
        ENUM_KIND_CASE(WM_GETICON)
        ENUM_KIND_CASE(WM_PAINT)
        ENUM_KIND_CASE(WM_SETCURSOR)
        ENUM_KIND_CASE(WM_MOUSEMOVE)
        ENUM_KIND_CASE(WM_NCMOUSEMOVE)
        ENUM_KIND_CASE(WM_MOUSEACTIVATE)
        ENUM_KIND_CASE(WM_NCLBUTTONDOWN)
        ENUM_KIND_CASE(WM_CAPTURECHANGED)
        ENUM_KIND_CASE(WM_SYSCOMMAND)
        ENUM_KIND_CASE(WM_CLOSE)
        ENUM_KIND_CASE(WM_MOVING)
        ENUM_KIND_CASE(WM_ERASEBKGND)
        ENUM_KIND_CASE(WM_EXITMENULOOP)
        ENUM_KIND_CASE(WM_NCMOUSELEAVE)
        ENUM_KIND_CASE(WM_NCHITTEST)
        ENUM_KIND_CASE(WM_EXITSIZEMOVE)
        ENUM_KIND_CASE(WM_ENTERSIZEMOVE)
        ENUM_KIND_CASE(WM_MOUSELEAVE)
        ENUM_KIND_CASE(WM_PARENTNOTIFY)
        ENUM_KIND_CASE(WM_NCCALCSIZE)
        ENUM_KIND_CASE(WM_GETMINMAXINFO)
        ENUM_KIND_CASE(WM_QUERYOPEN)
    default:
        return "Unknown msg: " + to_hex_string(id);
    }
}

bool WindowsWindow::main_window_hidden_{false};