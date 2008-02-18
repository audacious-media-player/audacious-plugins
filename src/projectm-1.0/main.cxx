/*
 * main.cxx: plugin glue to libprojectm
 * Copyright (c) 2008 William Pitcock <nenolod@sacredspiral.co.uk>
 * Portions copyright (c) 2004-2006 Peter Sperl
 *
 * This program is free software; you may distribute it under the terms
 * of the GNU General Public License; version 2.
 */

#include <stdio.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>

extern "C"
{
#include <audacious/util.h>
#include <audacious/plugin.h>
#include <audacious/playlist.h>
#include <audacious/auddrct.h>
}

#include <math.h>
#include "ConfigFile.h"

#include <libprojectM/projectM.hpp>

#include <GL/gl.h>
#define CONFIG_FILE "/share/projectM/config.inp"

// Forward declarations 
extern "C" void projectM_xmms_init(void);
extern "C" void projectM_cleanup(void);
extern "C" void projectM_render_pcm(gint16 pcm_data[2][512]);
extern "C" int worker_func(void *);
std::string read_config();

extern "C" VisPlugin projectM_vtable;

int SDLThreadWrapper(void *);
void handle_playback_trigger(void *, void *);

static void
projectM_draw_init(GtkWidget *widget,
     void *data);

static gboolean
projectM_idle_func(GtkWidget *widget);

static gboolean
projectM_draw_impl(GtkWidget      *widget,
      GdkEventExpose *event,
      gpointer        data);

static gboolean
projectM_reconfigure(GtkWidget         *widget,
         GdkEventConfigure *event,
         gpointer           data);

class projectMPlugin
{
  public:
    projectM *pm;

    GdkGLConfig *glconfig;
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *drawing_area;
    gboolean is_sync;
    gboolean error;
    gint idle_id;
    GTimer *timer;
    gint frames;

    projectMPlugin()
    {
        gtk_gl_init(NULL, NULL);

        this->pm = NULL;

        this->glconfig = gdk_gl_config_new_by_mode((GdkGLConfigMode) (GDK_GL_MODE_RGBA | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE));
        if (!this->glconfig)
        {
            this->error++;
            return;
        }

        this->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(this->window), "ProjectM");
        gtk_container_set_reallocate_redraws(GTK_CONTAINER(this->window), TRUE);

        this->vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(this->window), this->vbox);
        gtk_widget_show(this->vbox);

        this->drawing_area = gtk_drawing_area_new();
        gtk_widget_set_size_request(this->drawing_area, 512, 512);
        gtk_widget_set_gl_capability(this->drawing_area, this->glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);
        gtk_widget_add_events(this->drawing_area, GDK_VISIBILITY_NOTIFY_MASK);
        gtk_box_pack_start(GTK_BOX(this->vbox), this->drawing_area, TRUE, TRUE, 0);

        g_signal_connect_after(G_OBJECT(this->drawing_area), "realize",
                         G_CALLBACK(projectM_draw_init), NULL);
        g_signal_connect(G_OBJECT(this->drawing_area), "expose_event",
                         G_CALLBACK(projectM_draw_impl), NULL);

        this->timer = g_timer_new();
        this->frames = 0;
    }

    ~projectMPlugin()
    {
        delete this->pm;
        this->pm = 0;

        aud_hook_dissociate("playback begin", handle_playback_trigger);
    }

    void initUI(void)
    {
        gtk_widget_show(this->drawing_area);
        gtk_widget_show(this->window);

        this->idle_id = g_timeout_add (1000 / 30,
                                       (GSourceFunc) projectM_idle_func,
                                       this->drawing_area);

        /* XXX */
        aud_hook_associate("playback begin", handle_playback_trigger, NULL);
    }

    void addPCMData(gint16 pcm_data[2][512])
    {
        if (this->pm)
            this->pm->pcm->addPCM16(pcm_data);
    }

    void triggerPlaybackBegin(PlaylistEntry *entry)
    {
        std::string title(entry->title);

        if (this->pm)
            this->pm->projectM_setTitle(title);
    }
};

/* glue to implementation section */
projectMPlugin *thePlugin = 0;

static void
projectM_draw_init(GtkWidget *widget,
     void *data)
{
    GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return;

    std::string configFile = read_config();
    thePlugin->pm = new projectM(configFile);
    thePlugin->pm->projectM_resetGL(widget->allocation.width, widget->allocation.height);

    gdk_gl_drawable_swap_buffers(gldrawable);
    gdk_gl_drawable_gl_end(gldrawable);

    g_signal_connect(G_OBJECT(widget), "configure_event",
                     G_CALLBACK(projectM_reconfigure), NULL);
}

static gboolean
projectM_reconfigure(GtkWidget         *widget,
      GdkEventConfigure *event,
      gpointer           data)
{
    GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return FALSE;

    thePlugin->pm->projectM_resetGL(widget->allocation.width, widget->allocation.height);

    gdk_gl_drawable_swap_buffers(gldrawable);
    gdk_gl_drawable_gl_end(gldrawable);

    return TRUE;
}

static gboolean
projectM_draw_impl(GtkWidget      *widget,
      GdkEventExpose *event,
      gpointer        data)
{
    GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return FALSE;

    thePlugin->pm->renderFrame();
    thePlugin->frames++;

    gdk_gl_drawable_swap_buffers(gldrawable);
    gdk_gl_drawable_gl_end(gldrawable);

    gdouble seconds = g_timer_elapsed (thePlugin->timer, NULL);
    if (seconds >= 5.0)
    {
        gdouble fps = thePlugin->frames / seconds;
        g_print ("%d frames in %6.3f seconds = %6.3f FPS\n", thePlugin->frames, seconds, fps);
        g_timer_reset (thePlugin->timer);
        thePlugin->frames = 0;
    }

    return TRUE;
}

static gboolean
projectM_idle_func(GtkWidget *widget)
{
    gdk_window_invalidate_rect(widget->window, &widget->allocation, FALSE);
    gdk_window_process_updates(widget->window, FALSE);

    return TRUE;
}

void handle_playback_trigger(gpointer plentry_p, gpointer unused)
{
    PlaylistEntry *entry = reinterpret_cast<PlaylistEntry *>(plentry_p);

    if (!thePlugin)
        return;

    thePlugin->triggerPlaybackBegin(entry);
}

extern "C" void projectM_xmms_init(void)
{
    if (thePlugin)
        return;

    thePlugin = new projectMPlugin;
    thePlugin->initUI();
}

extern "C" void projectM_cleanup(void)
{
    if (!thePlugin)
        return;

    delete thePlugin;
}

extern "C" void projectM_render_pcm(gint16 pcm_data[2][512])
{
    if (!thePlugin)
        return;

    thePlugin->addPCMData(pcm_data);
}

/********************************************************************************
 * XXX: This code is from projectM and still needs to be rewritten!             *
 ********************************************************************************/
std::string read_config()
{

//   int n;

    char num[512];
    FILE *in;
    FILE *out;

    char *home;
    char projectM_home[1024];
    char projectM_config[1024];

    strcpy(projectM_config, PROJECTM_PREFIX);
    strcpy(projectM_config + strlen(PROJECTM_PREFIX), CONFIG_FILE);
    projectM_config[strlen(PROJECTM_PREFIX) + strlen(CONFIG_FILE)] = '\0';
    //printf("dir:%s \n",projectM_config);
    home = getenv("HOME");
    strcpy(projectM_home, home);
    strcpy(projectM_home + strlen(home), "/.projectM/config.inp");
    projectM_home[strlen(home) + strlen("/.projectM/config.inp")] = '\0';


    if ((in = fopen(projectM_home, "r")) != 0)
    {
        //printf("reading ~/.projectM/config.inp \n");
        fclose(in);
        return std::string(projectM_home);
    }
    else
    {
        printf("trying to create ~/.projectM/config.inp \n");

        strcpy(projectM_home, home);
        strcpy(projectM_home + strlen(home), "/.projectM");
        projectM_home[strlen(home) + strlen("/.projectM")] = '\0';
        mkdir(projectM_home, 0755);

        strcpy(projectM_home, home);
        strcpy(projectM_home + strlen(home), "/.projectM/config.inp");
        projectM_home[strlen(home) + strlen("/.projectM/config.inp")] = '\0';

        if ((out = fopen(projectM_home, "w")) != 0)
        {

            if ((in = fopen(projectM_config, "r")) != 0)
            {

                while (fgets(num, 80, in) != NULL)
                {
                    fputs(num, out);
                }
                fclose(in);
                fclose(out);


                if ((in = fopen(projectM_home, "r")) != 0)
                {
                    printf("created ~/.projectM/config.inp successfully\n");
                    fclose(in);
                    return std::string(projectM_home);
                }
                else
                {
                    printf("This shouldn't happen, using implementation defualts\n");
                    abort();
                }
            }
            else
            {
                printf("Cannot find projectM default config, using implementation defaults\n");
                abort();
            }
        }
        else
        {
            printf("Cannot create ~/.projectM/config.inp, using default config file\n");
            if ((in = fopen(projectM_config, "r")) != 0)
            {
                printf("Successfully opened default config file\n");
                fclose(in);
                return std::string(projectM_config);
            }
            else
            {
                printf("Using implementation defaults, your system is really messed up, I'm suprised we even got this far\n");
                abort();
            }
        }

    }

    abort();
}
