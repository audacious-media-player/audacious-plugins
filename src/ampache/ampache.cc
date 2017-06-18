// ampache.cc
//
// Project: Ampache Browser Audacious Plugin
// License: GNU GPLv3
//
// Copyright (C) 2015-2016 Róbert Čerňanský and John Lindgren

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/playlist.h>
#include <libaudcore/vfs_async.h>

#include <ampache_browser/settings.h>
#include <ampache_browser/ampache_browser.h>
#include <ampache_browser/application_qt.h>

#define CFG_SECT "ampache_browser"

using NetworkCb = ampache_browser::ApplicationQt::NetworkRequestCb;
using UrlList = std::vector<std::string>;

class AmpacheBrowserPlugin: public GeneralPlugin
{
public:
    static const char about[];

    static constexpr PluginInfo pluginInfo = {
        N_("Ampache Browser"),
        PACKAGE,
        about,
        nullptr,
        PluginQtOnly
    };

    constexpr AmpacheBrowserPlugin(): GeneralPlugin(pluginInfo, false) {}

    bool init() override;
    void cleanup() override;
    void* get_qt_widget() override;
};

const char AmpacheBrowserPlugin::about[] =
    N_("Ampache Browser for Audacious\n"
       "http://ampache-browser.org/\n\n"
       "Copyright (C) Róbert Čerňanský and John Lindgren\n"
       "License: GNU GPLv3");

static SmartPtr<ampache_browser::ApplicationQt> s_app;

static void vfsAsyncCb(const char* url, const Index<char>& data, void* callback)
{
    if (s_app) // ignore callbacks after cleanup()
        (*(NetworkCb*)callback)(url, data.begin(), data.len());
}

static Index<PlaylistAddItem> toAddItems(const UrlList& urls)
{
    Index<PlaylistAddItem> addItems;
    for (auto& url: urls)
        addItems.append(String(url.c_str()));

    return addItems;
}

static void initSettings(ampache_browser::Settings &settings)
{
    static const std::string bool_settings[] = {
        settings.USE_DEMO_SERVER
    };

    static const std::string str_settings[] = {
        settings.SERVER_URL,
        settings.USER_NAME,
        settings.PASSWORD_HASH
    };

    auto verbosity = getenv("AMPACHE_BROWSER_PLUGIN_VERBOSITY");
    settings.setInt(settings.LOGGING_VERBOSITY, verbosity ? str_to_int(verbosity) : 0);

    for (auto& name: bool_settings)
        settings.setBool(name, aud_get_bool(CFG_SECT, name.c_str()));
    for (auto& name: str_settings)
        settings.setString(name, (const char*)aud_get_str(CFG_SECT, name.c_str()));

    settings.connectChanged([&settings]() {
        for (auto& name: bool_settings)
            aud_set_bool(CFG_SECT, name.c_str(), settings.getBool(name));
        for (auto& name: str_settings)
            aud_set_str(CFG_SECT, name.c_str(), settings.getString(name).c_str());
    });
}

bool AmpacheBrowserPlugin::init()
{
    s_app.capture(new ampache_browser::ApplicationQt);

    s_app->setNetworkRequestFunction([](const std::string& url, NetworkCb& networkCb) {
        vfs_async_file_get_contents(url.c_str(), vfsAsyncCb, &networkCb);
    });

    auto& browser = s_app->getAmpacheBrowser();

    browser.connectPlay([](const UrlList& urls) {
        aud_drct_pl_open_list(toAddItems(urls));
    });

    browser.connectCreatePlaylist([](const UrlList& urls) {
        Playlist::new_playlist().insert_items(-1, toAddItems(urls), false);
    });

    browser.connectAddToPlaylist([](const UrlList& urls) {
        Playlist::active_playlist().insert_items(-1, toAddItems(urls), false);
    });

    initSettings(s_app->getSettings());

    return true;
}

void AmpacheBrowserPlugin::cleanup()
{
    s_app.clear();
}

void* AmpacheBrowserPlugin::get_qt_widget()
{
    s_app->run();
    return s_app->getMainWidget();
}

EXPORT AmpacheBrowserPlugin aud_plugin_instance;
