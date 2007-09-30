#include <audacious/plugin.h>

extern void projectM_xmms_init(void);
extern void projectM_cleanup(void);
extern void projectM_about(void);
extern void projectM_configure(void);
extern void projectM_playback_start(void);
extern void projectM_playback_stop(void);
extern void projectM_render_pcm(gint16 pcm_data[2][512]);
extern void projectM_render_freq(gint16 pcm_data[2][256]);

VisPlugin projectM_vtable = {
    .description = "projectM v1.0",
    .num_pcm_chs_wanted = 2,
    .init = projectM_xmms_init,
    .cleanup = projectM_cleanup,
    .about = projectM_about,
    .configure = projectM_configure,
    .playback_start = projectM_playback_start,
    .playback_stop = projectM_playback_stop,
    .render_pcm = projectM_render_pcm,
    .render_freq = projectM_render_freq,
};

VisPlugin *projectM_vplist[] = { &projectM_vtable, NULL };

DECLARE_PLUGIN(projectm, NULL, NULL, NULL, NULL, NULL, NULL,
        projectM_vplist, NULL);
