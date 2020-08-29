

#include <src/hotkey/api_hotkey.h>

#include <shobjidl.h>
#include <windows.h>

#include <cstring>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkwin32.h>
#include <glib.h>
#include <gtk/gtkeditable.h>
#include <libaudcore/runtime.h>
#include <string>
#include <vector>

#include "windows_key_str_map.h"

constexpr auto W_KEY_ID_PLAY = 18771;
constexpr auto W_KEY_ID_PREV = 18772;
constexpr auto W_KEY_ID_NEXT = 18773;
constexpr auto W_FIRST_GLOBAL_KEY_ID = 18774;

template<typename T>
std::string VirtualKeyCodeToStringMethod2(T virtualKey)
{
    WCHAR name[128];
    static auto key_layout = GetKeyboardLayout(0);
    UINT scanCode = MapVirtualKeyEx(virtualKey, MAPVK_VK_TO_VSC, key_layout);
    LONG lParamValue = (scanCode << 16);
    int result = GetKeyNameTextW(lParamValue, name, 1024);
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

void Hotkey::add_hotkey(HotkeyConfiguration ** pphotkey,
                        Hotkey::OS_KeySym keysym, int mask, int type,
                        EVENT event)
{
    AUDDBG("lHotkeyFlow:Win call.");
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
    auto * hotkey = &(plugin_cfg.first);
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

HRESULT AddThumbarButtons(HWND hwnd)
{
    // Define an array of two buttons. These buttons provide images through an
    // image list and also provide tooltips.
    THUMBBUTTONMASK dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;

    /*
      THUMBBUTTONMASK dwMask;
      UINT iId;
      UINT iBitmap;
      HICON hIcon;
      WCHAR szTip[260];
      THUMBBUTTONFLAGS dwFlags;
      */

    THUMBBUTTON btn[]{
        {dwMask, W_KEY_ID_PREV, 0, nullptr, L"Previous song", THBF_ENABLED},
        {dwMask, W_KEY_ID_PLAY, 0, nullptr, L"Play/pause", THBF_ENABLED},
        {dwMask, W_KEY_ID_NEXT, 0, nullptr, L"Next song", THBF_ENABLED}};

    ITaskbarList3 * ptbl;
    HRESULT hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&ptbl));

    if (SUCCEEDED(hr))
    {
        // Declare the image list that contains the button images.
        // hr = ptbl->ThumbBarSetImageList(hwnd, himl);

        if (SUCCEEDED(hr))
        {
            // Attach the toolbar to the thumbnail.
            hr = ptbl->ThumbBarAddButtons(hwnd, ARRAYSIZE(btn), btn);
        }
        ptbl->Release();
    }
    return hr;
}

GdkFilterReturn buttons_evts_filter(GdkXEvent * gdk_xevent, GdkEvent * event,
                                    gpointer user_data)
{
    auto msg = reinterpret_cast<MSG *>(gdk_xevent);
    if (msg->message == WM_COMMAND)
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
        }
        return GDK_FILTER_REMOVE;
    }
    if (msg->message == WM_HOTKEY)
    {
        auto k_id = static_cast<short>(msg->wParam & 0xFFFF);
        AUDDBG("lHotkeyFlow:Global hotkey: %d", k_id);
        // Max 20 HKs
        if (k_id >= W_FIRST_GLOBAL_KEY_ID && k_id < W_FIRST_GLOBAL_KEY_ID + 20)
        {
            auto idx = k_id - W_FIRST_GLOBAL_KEY_ID;
            auto * config = &plugin_cfg.first;
            while (idx--)
            {
                config = config->next;
            }
            handle_keyevent(config->event);
        }
    }
    return GDK_FILTER_CONTINUE;
}

struct EnumWindowsCallbackArgs
{
    explicit EnumWindowsCallbackArgs(DWORD p) : pid(p) {}
    const DWORD pid;
    std::vector<HWND> handles;
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
        args->handles.push_back(hnd);
    }

    return TRUE;
}

std::vector<HWND> getToplevelWindows()
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
        return std::vector<HWND>();
    }
    // Otherwise, callback function filled the struct. Return important data.
    // Test: GetActiveWindow
    args.handles.push_back(GetActiveWindow());
    args.handles.push_back(GetForegroundWindow());
    return args.handles;
}

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

    AUDDBG("lHotkeyFlow:Window created. Do real stuff.");
    auto wins = getToplevelWindows();
    AUDDBG("lHotkeyFlow:windows: %zu", wins.size());
    for (auto win : wins)
    {
        wchar_t className[311];
        GetClassNameW(win, className, _countof(className));
        auto s_cn = reinterpret_cast<char *>(
            g_utf16_to_utf8(reinterpret_cast<const gunichar2 *>(className), -1,
                            nullptr, nullptr, nullptr));

        GetWindowTextW(win, className, _countof(className));
        auto s_cn2 = reinterpret_cast<char *>(
            g_utf16_to_utf8(reinterpret_cast<const gunichar2 *>(className), -1,
                            nullptr, nullptr, nullptr));
        if (s_cn)
        {
            AUDDBG("lHotkeyFlow:WIN: %s%s", s_cn,
                   ([s_cn2]() {
                       std::string returning{};
                       if (s_cn2)
                       {
                           returning = std::string(" titled: ") + s_cn2;
                           g_free(s_cn2);
                       }
                       return returning;
                   }())
                       .c_str());
            g_free(s_cn);
        }
    }
    auto the_hwnd = wins.back();
    // AddThumbarButtons(the_hwnd); Disabled for now ... first get hotkeys
    // perfectly stable
    auto gdkwin = gdk_win32_handle_table_lookup(the_hwnd);
    gdk_window_add_filter((GdkWindow *)gdkwin, buttons_evts_filter, nullptr);
    register_global_keys(the_hwnd);
    return false;
}

void win_init()
{
    AUDDBG("lHotkeyFlow:Win .. important call.");
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