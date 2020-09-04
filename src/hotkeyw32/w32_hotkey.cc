/*
 *  w32_hotkey.cc
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

#include <src/hotkey/api_hotkey.h>

#include <windows.h>

#include <cassert>
#include <cstring>
#include <glib.h>
#include <gtk/gtkeditable.h>
#include <iomanip>
#include <libaudcore/runtime.h>
#include <src/hotkey/grab.h>
#include <sstream>

#include "windows_key_str_map.h"
#include "windows_window.h"

constexpr auto W_KEY_ID_PLAY = 18771;
constexpr auto W_KEY_ID_PREV = 18772;
constexpr auto W_KEY_ID_NEXT = 18773;
constexpr auto W_FIRST_GLOBAL_KEY_ID = 18774;

bool system_up_and_running{false};

/**
 * Handle to window that has thumbbar buttons associated. Nullptr if it is not
 * on display.
 */
WindowsWindow message_receiving_window{
    nullptr, {}, {}, AudaciousWindowKind::UNKNOWN};

template<typename T>
std::string VirtualKeyCodeToStringMethod2(T virtualKey)
{
    wchar_t name[128];
    static auto key_layout = GetKeyboardLayout(0);
    UINT scanCode = MapVirtualKeyEx(virtualKey, MAPVK_VK_TO_VSC, key_layout);
    LONG lParamValue = (scanCode << 16);
    int result = GetKeyNameTextW(lParamValue, name, sizeof name);
    if (result > 0)
    {
        auto * windows_cmd = reinterpret_cast<char *>(
            g_utf16_to_utf8(reinterpret_cast<const gunichar2 *>(name), -1,
                            nullptr, nullptr, nullptr));
        std::string returning(windows_cmd);
        g_free(windows_cmd);
        return returning;
    }
    else
    {
        return {};
    }
}

void grab_keys_onto_window();

void Hotkey::add_hotkey(HotkeyConfiguration ** pphotkey,
                        Hotkey::OS_KeySym keysym, int mask, int type,
                        EVENT event)
{
    HotkeyConfiguration * photkey;
    if (keysym == 0)
        return;
    if (pphotkey == nullptr)
        return;
    photkey = *pphotkey;
    if (photkey == nullptr)
        return;
    // keycode =
    // XKeysymToKeycode(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
    // keysym); if (keycode == 0) return;
    if (photkey->key)
    {
        photkey->next = g_new(HotkeyConfiguration, 1);
        photkey = photkey->next;
        *pphotkey = photkey;
        photkey->next = nullptr;
    }
    photkey->key = (int)42;
    photkey->mask = mask;
    photkey->event = event;
    photkey->type = type;
}

void register_global_keys(HWND handle)
{
    auto * hotkey = &(plugin_cfg_gtk_global_hk.first);
    auto _id = W_FIRST_GLOBAL_KEY_ID;
    while (hotkey)
    {
        RegisterHotKey(handle, _id++, hotkey->mask, hotkey->key);
        hotkey = hotkey->next;
    }
}

// void assign(wchar_t *ptr_first_element, const wchar_t *text) {
//  int loc = 0;
//  while (text[loc]) {
//    ptr_first_element[loc] = text[loc];
//    ++loc;
//  }
//  ptr_first_element[loc] = 0;
//}

//HRESULT AddThumbarButtons(HWND hwnd)
//{
//    // Define an array of two buttons. These buttons provide images through an
//    // image list and also provide tooltips.
//    THUMBBUTTONMASK dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
//
//    /*
//      THUMBBUTTONMASK dwMask;
//      UINT iId;
//      UINT iBitmap;
//      HICON hIcon;
//      WCHAR szTip[260];
//      THUMBBUTTONFLAGS dwFlags;
//      */
//
//    THUMBBUTTON btn[]{
//        {dwMask, W_KEY_ID_PREV, 0, nullptr, L"Previous song", THBF_ENABLED},
//        {dwMask, W_KEY_ID_PLAY, 0, nullptr, L"Play/pause", THBF_ENABLED},
//        {dwMask, W_KEY_ID_NEXT, 0, nullptr, L"Next song", THBF_ENABLED}};
//
//    ITaskbarList3 * ptbl;
//    HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER,
//                                  IID_PPV_ARGS(&ptbl));
//
//    if (SUCCEEDED(hr))
//    {
//        // Declare the image list that contains the button images.
//        // hr = ptbl->ThumbBarSetImageList(hwnd, himl);
//
//        if (SUCCEEDED(hr))
//        {
//            // Attach the toolbar to the thumbnail.
//            hr = ptbl->ThumbBarAddButtons(hwnd, ARRAYSIZE(btn), btn);
//        }
//        ptbl->Release();
//    }
//    return hr;
//}

GdkFilterReturn w32_events_filter_first_everything(GdkXEvent * gdk_xevent,
                                                   GdkEvent * event,
                                                   gpointer user_data)
{
    auto msg = reinterpret_cast<MSG *>(gdk_xevent);
    if (!msg->hwnd)
    {
        AUDWARN("We have event but hwnd is null. This doesn't make sense.");
        return GDK_FILTER_CONTINUE;
    }
    // log_win_msg(msg->message);
    if (msg->message == WM_SHOWWINDOW)
    {
        if (msg->wParam == TRUE)
        {
            auto event_window = WindowsWindow::get_window_data(msg->hwnd);
            AUDDBG("(CommingUp)TRUE %s; %s",
                   stringify_win_evt(msg->message).c_str(),
                   (static_cast<std::string>(event_window)).c_str());
            if (event_window.is_main_window(true))
            {
                event_window.
                WindowsWindow::main_window_hidden_ = false;
                AUDDBG(
                    "TRANSFER EventReceivingWindow from %s to mainwindow HWND "
                    "%p",
                    static_cast<std::string>(message_receiving_window).c_str(),
                    event_window.handle());
                ungrab_keys();
                message_receiving_window = std::move(event_window);
                grab_keys_onto_window();
            }
        }
        //        else
        //        {
        //            // leaving us.
        //            AUDDBG("(GettingDown)FALSE %s; %s",
        //                   stringify_win_evt(msg->message).c_str(),
        //                   (static_cast<std::string>(event_window)).c_str());
        //        }
    }
    //    else if (msg->message == WM_CLOSE)
    //    {
    //        auto event_window = WindowsWindow::get_window_data(msg->hwnd);
    //        AUDDBG("%s: ; %s", stringify_win_evt(msg->message).c_str(),
    //               (static_cast<std::string>(event_window)).c_str());
    //    }
    //    else if (msg->message == WM_CREATE)
    //    {
    //        auto event_window = WindowsWindow::get_window_data(msg->hwnd);
    //        AUDDBG("%s: ; %s", stringify_win_evt(msg->message).c_str(),
    //               (static_cast<std::string>(event_window)).c_str());
    //    }
    //    else if (msg->message == WM_DESTROY)
    //    {
    //        auto event_window = WindowsWindow::get_window_data(msg->hwnd);
    //        AUDDBG("%s: ; %s", stringify_win_evt(msg->message).c_str(),
    //               (static_cast<std::string>(event_window)).c_str());
    //    }
    return GDK_FILTER_CONTINUE;
}

GdkFilterReturn w32_evts_filter(GdkXEvent * gdk_xevent, GdkEvent * event,
                                gpointer user_data)
{
    auto msg = reinterpret_cast<MSG *>(gdk_xevent);
    // log_win_msg(msg->message);
    if (msg->message == WM_SHOWWINDOW)
    {
        if (msg->wParam != TRUE)
        {
            AUDDBG("MainWindow is getting hidden. We need to recreate the "
                   "hooks.\n  TRANSFER EventReceivingWindow from %s",
                   static_cast<std::string>(message_receiving_window).c_str());
            WindowsWindow::main_window_hidden_ = true;
            ungrab_keys();
            grab_keys();
        }
    }
    else if (msg->message == WM_COMMAND)
    {
        auto buttonId = static_cast<short>(msg->wParam & 0xFFFF);
        AUDDBG("Clicked button with ID: %d", buttonId);
        switch (buttonId)
        {
        case W_KEY_ID_PLAY:
            handle_keyevent(EVENT_PAUSE);
            break;
        case W_KEY_ID_NEXT:
            handle_keyevent(EVENT_NEXT_TRACK);
            break;
        case W_KEY_ID_PREV:
            handle_keyevent(EVENT_PREV_TRACK);
            break;
        }
        return GDK_FILTER_REMOVE;
    }
    else if (msg->message == WM_HOTKEY)
    {
        auto k_id = LOWORD(msg->wParam);
        AUDDBG("Global hotkey: %du", k_id);
        // Max 20 HKs
        if (k_id >= W_FIRST_GLOBAL_KEY_ID && k_id < W_FIRST_GLOBAL_KEY_ID + 20)
        {
            auto idx = k_id - W_FIRST_GLOBAL_KEY_ID;
            auto * config = &plugin_cfg_gtk_global_hk.first;
            while (idx--)
            {
                config = config->next;
            }
            if (handle_keyevent(config->event))
            {
                return GDK_FILTER_REMOVE;
            }
        }
    }
    return GDK_FILTER_CONTINUE;
}

void register_hotkeys_with_available_window()
{
    assert(static_cast<bool>(message_receiving_window));
    auto * gdkwin = message_receiving_window.gdk_window();
    if (!gdkwin)
    {
        AUDINFO("HWND doesn't have associated gdk window. Cannot setup global "
                "hotkeys.");
        return;
    }
    gdk_window_add_filter(gdkwin, w32_evts_filter, nullptr);
    register_global_keys(message_receiving_window.handle());
}

void release_filter()
{
    if (!message_receiving_window)
    {
        return;
    }
    gdk_window_remove_filter(message_receiving_window.gdk_window(),
                             w32_evts_filter, nullptr);
    message_receiving_window = nullptr;
}
//
// void main_window_created_from_statusbar(
//    MainWindowSearchFilterData * const passed_data,
//    WindowsWindow & created_main_window)
//{
//    // When window fully shows, must add windows buttons.
//    gdk_window_remove_filter(
//        reinterpret_cast<GdkWindow *>(passed_data->window_),
//        reinterpret_cast<GdkFilterFunc>(passed_data->function_ptr_),
//        passed_data);
//    ungrab_keys();
//    release_filter();
//    delete passed_data;
//}
//
// GdkFilterReturn main_window_missing_filter(GdkXEvent * gdk_xevent,
//                                           GdkEvent * event, gpointer
//                                           user_data)
//{
//    auto msg = reinterpret_cast<MSG *>(gdk_xevent);
//    if (msg->message == WM_SHOWWINDOW && msg->wParam)
//    {
//        auto win_data = WindowsWindow::get_window_data(msg->hwnd);
//        if (win_data.is_main_window())
//        {
//            main_window_created_from_statusbar(
//                reinterpret_cast<MainWindowSearchFilterData *>(user_data),
//                win_data);
//        }
//    }
//    return GDK_FILTER_CONTINUE;
//}

/**
 * GSourceFunc:
 * @user_data: data passed to the function, set when the source was
 *     created with one of the above functions
 *
 * Specifies the type of function passed to g_timeout_add(),
 * g_timeout_add_full(), g_idle_add(), and g_idle_add_full().
 *
 * Returns: %FALSE if the source should be removed. #G_SOURCE_CONTINUE and
 * #G_SOURCE_REMOVE are more memorable names for the return value.
 */
gboolean window_created_callback(gpointer user_data)
{
    AUDDBG("Window created. Do real stuff.");
    system_up_and_running = true;
    gdk_window_add_filter(nullptr, w32_events_filter_first_everything, nullptr);
    grab_keys();
    return G_SOURCE_REMOVE;
}

void win_init()
{
    g_idle_add(&window_created_callback, nullptr);
}

void Hotkey::key_to_string(int key, char ** out_keytext)
{
    // Special handling for most OEM keys - they may depend on language or
    // keyboard?
    switch (key)
    {
    case VK_OEM_NEC_EQUAL:
        // case VK_OEM_FJ_JISHO: (note: same as EQUAL)
    case VK_OEM_FJ_MASSHOU:
    case VK_OEM_FJ_TOUROKU:
    case VK_OEM_FJ_LOYA:
    case VK_OEM_FJ_ROYA:
    case VK_OEM_1:
    case VK_OEM_2:
    case VK_OEM_3:
    case VK_OEM_4:
    case VK_OEM_5:
    case VK_OEM_6:
    case VK_OEM_7:
    case VK_OEM_8:
    case VK_OEM_AX:
    case VK_OEM_102:
    case VK_OEM_RESET:
    case VK_OEM_JUMP:
    case VK_OEM_PA1:
    case VK_OEM_PA2:
    case VK_OEM_PA3:
    case VK_OEM_WSCTRL:
    case VK_OEM_CUSEL:
    case VK_OEM_ATTN:
    case VK_OEM_FINISH:
    case VK_OEM_COPY:
    case VK_OEM_AUTO:
    case VK_OEM_ENLW:
    case VK_OEM_BACKTAB:
    case VK_OEM_CLEAR:
    {
        auto win_str = VirtualKeyCodeToStringMethod2(key);
        if (!win_str.empty())
        {
            *out_keytext = g_strdup(win_str.c_str());
            return;
        }
    }
    }
    *out_keytext = g_strdup(WIN_VK_NAMES[key]);
}

char * Hotkey::create_human_readable_keytext(const char * const keytext,
                                             int key, int mask)
{
    const auto w_mask = static_cast<UINT>(mask);
    std::ostringstream produced;
    // "Control", "Shift", "Alt", "Win"
    if (w_mask & static_cast<UINT>(MOD_CONTROL))
    {
        produced << "Control + ";
    }
    if (w_mask & static_cast<UINT>(MOD_SHIFT))
    {
        produced << "Shift + ";
    }
    if (w_mask & static_cast<UINT>(MOD_ALT))
    {
        produced << "Alt + ";
    }
    if (w_mask & static_cast<UINT>(MOD_WIN))
    {
        produced << "Win + ";
    }
    produced << keytext;

    AUDDBG(produced.str().c_str());
    return g_strdup(produced.str().c_str());
}
void grab_keys_onto_window()
{
    AUDDBG("Will hook together the window of kind %s\n :%s",
           message_receiving_window.kind_string(),
           static_cast<std::string>(message_receiving_window).c_str());
    if (message_receiving_window.kind() == AudaciousWindowKind::NONE ||
        message_receiving_window.kind() == AudaciousWindowKind::UNKNOWN)
    {
        AUDERR("Win API did not find the window to receive messages. Global "
               "hotkeys are disabled!");
        message_receiving_window = nullptr;
        return;
    }
    register_hotkeys_with_available_window();
}
void grab_keys()
{
    if (!system_up_and_running)
    {
        return;
    }
    AUDDBG("Grabbing ...");

    message_receiving_window = WindowsWindow::find_message_receiver_window();
    grab_keys_onto_window();
}
void ungrab_keys()
{
    AUDDBG("Releasing ...");
    if (message_receiving_window)
    {
        auto * hotkey = &(plugin_cfg_gtk_global_hk.first);
        auto _id = W_FIRST_GLOBAL_KEY_ID;
        while (hotkey)
        {
            UnregisterHotKey(message_receiving_window.handle(), _id++);
            hotkey = hotkey->next;
        }
    }
    release_filter();
}