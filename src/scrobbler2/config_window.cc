//audacious includes
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>

//plugin includes
#include "scrobbler.h"


//shared variables
gboolean          permission_check_requested   = false;
gboolean          invalidate_session_requested = false;
enum permission perm_result                  = PERMISSION_UNKNOWN;
String          username;


static gboolean permission_checker_thread (void *) {
    if (permission_check_requested == true) {
        //the answer hasn't arrived yet
        hook_call("ui show progress", (void *)N_("Checking Last.fm access ..."));
        return true;

    } else {
        //the answer has arrived
        hook_call("ui hide progress", nullptr);
        g_assert(perm_result != PERMISSION_UNKNOWN);

        auto msg2 = _("Your scrobbles are being saved on your computer "
         "temporarily.  They will be submitted as soon as Audacious is "
         "allowed access.");

        if (perm_result == PERMISSION_ALLOWED) {
            hook_call("ui show info", (void *)(const char *)str_printf
             (_("Permission granted.  Scrobbling for user %s."),
             (const char *)username));

        } else if (perm_result == PERMISSION_DENIED) {
            auto msg1 = _("Permission denied.  Open the following "
             "URL in a browser, allow Audacious access to your account, and "
             "then click 'Check Permission' again:");
            auto url = str_printf("http://www.last.fm/api/auth/?api_key=%s"
             "&token=%s", SCROBBLER_API_KEY, (const char *)request_token);

            hook_call("ui show error", (void *)(const char *)str_concat
             ({msg1, "\n\n", url, "\n\n", msg2}));

        } else if (perm_result == PERMISSION_NONET) {
            auto msg1 = _("There was a problem contacting Last.fm.");

            hook_call("ui show error", (void *)(const char *)str_concat
             ({msg1, "\n\n", msg2}));
        }

        perm_result = PERMISSION_UNKNOWN;
        return false;
    }
}

static void permission_checker () {
    //This will make the communication thread check the permission
    //and set the current status on the perm_result enum
    permission_check_requested = true;

    //This is only to accelerate the check.
    //If scrobbles are being made, they are stopped for the request to be done sooner.
    scrobbling_enabled = false;

    //Wake up the communication thread in case it's waiting for track plays
    pthread_mutex_lock(&communication_mutex);
    pthread_cond_signal(&communication_signal);
    pthread_mutex_unlock(&communication_mutex);

    //The button is clicked. Wait for the permission check to be done.
    g_timeout_add(250, permission_checker_thread, nullptr);
}

static void revoke_permissions () {
    pthread_mutex_lock(&communication_mutex);
    invalidate_session_requested = true;

    scrobbling_enabled = false;
    pthread_cond_signal(&communication_signal);
    pthread_mutex_unlock(&communication_mutex);
}

static const PreferencesWidget buttons[] = {
    WidgetButton (N_("Check Permission"), {permission_checker}),
    WidgetButton (N_("Revoke Permission"), {revoke_permissions})
};

static const PreferencesWidget config_contents[] = {
    WidgetLabel (N_("You need to allow Audacious to scrobble tracks to your Last.fm account.")),
    WidgetBox ({{buttons}, true})
};

const PluginPreferences configuration = {{config_contents}};
