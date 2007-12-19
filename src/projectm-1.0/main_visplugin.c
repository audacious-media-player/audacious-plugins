#include <audacious/plugin.h>

extern void projectM_xmms_init(void);
extern void projectM_cleanup(void);
extern void projectM_render_pcm(gint16 pcm_data[2][512]);

VisPlugin projectM_vtable = {
    .description = "projectM v1.0",
    .num_pcm_chs_wanted = 2,
    .init = projectM_xmms_init,
    .cleanup = projectM_cleanup,
    .render_pcm = projectM_render_pcm,
};

VisPlugin *projectM_vplist[] = { &projectM_vtable, NULL };

DECLARE_PLUGIN(projectm, NULL, NULL, NULL, NULL, NULL, NULL,
        projectM_vplist, NULL);
