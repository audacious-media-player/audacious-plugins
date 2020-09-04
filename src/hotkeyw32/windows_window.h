/*
 *  windows_window.h
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

#ifndef _AUD_PLUGINS_HOTKEYW32_WINDOWS_WINDOW_H_INCLUDED_
#define _AUD_PLUGINS_HOTKEYW32_WINDOWS_WINDOW_H_INCLUDED_

#include <gdk/gdkwin32.h>
#include <string>
#include <vector>

enum class AudaciousWindowKind
{
    NONE,
    MAIN_WINDOW,
    TASKBAR_WITH_WINHANDLE_AND_GDKHANDLE,
    GTKSTATUSICON_OBSERVER,
    UNKNOWN
};

struct WindowsWindow
{
    static bool main_window_hidden_;
    WindowsWindow() = default;
    ~WindowsWindow() = default;
    WindowsWindow(const WindowsWindow & right) = delete;
    WindowsWindow & operator=(const WindowsWindow & right) = delete;
    WindowsWindow & operator=(WindowsWindow && right) = default;
    WindowsWindow(WindowsWindow && right) = default;

    WindowsWindow(HWND handle, std::string className, std::string winHeader,                  AudaciousWindowKind kind);
    static std::string translated_title();
    bool is_main_window(bool allow_hidden = false) const;
    explicit operator std::string() const;
    static WindowsWindow get_window_data(HWND pHwnd);
    static WindowsWindow find_message_receiver_window();

    operator bool() const; // NOLINT(google-explicit-constructor)
    // clang-format off
    void unref();
    WindowsWindow& operator=(std::nullptr_t dummy) { unref(); return *this; }
    // clang-format on

    HWND handle() const { return handle_; }
    const std::string & class_name() const { return class_name_; }
    const std::string & win_header() const { return win_header_; }
    AudaciousWindowKind kind() const { return kind_; }
    GdkWindow * gdk_window() const;

    const char * kind_string();

private:
    HWND handle_{};
    std::string class_name_{};
    std::string win_header_{};
    mutable AudaciousWindowKind kind_{};
    mutable GdkWindow * gdk_window_{};
};

struct MainWindowSearchFilterData
{
    gpointer window_;
    void * function_ptr_;

    MainWindowSearchFilterData(gpointer window, void * functionPtr)
        : window_(window), function_ptr_(functionPtr)
    {
    }

    MainWindowSearchFilterData(const MainWindowSearchFilterData &) = delete;
    MainWindowSearchFilterData(MainWindowSearchFilterData &&) = delete;
    MainWindowSearchFilterData &
    operator=(const MainWindowSearchFilterData &) = delete;
    MainWindowSearchFilterData &
    operator=(MainWindowSearchFilterData &&) = delete;
    ~MainWindowSearchFilterData() = default;
};

std::vector<WindowsWindow> get_this_app_windows();
std::string stringify_win_evt(UINT id);

#endif