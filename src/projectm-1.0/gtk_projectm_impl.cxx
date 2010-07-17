/*
 * gtk_projectm_impl.cxx: GTK+ ProjectM Implementation.
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
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>

#include "gtk_projectm_impl.h"

#include <math.h>

#include <libprojectM/projectM.hpp>
#include <libprojectM/event.h>

#include <GL/gl.h>
#define CONFIG_FILE "/share/projectM/config.inp"

// Forward declarations
static std::string read_config();

int SDLThreadWrapper(void *);
void handle_playback_trigger(void *, void *);

static void _gtk_projectm_realize_impl(GtkWidget *widget, gpointer data);
static void unrealize_cb (GtkWidget * widget, struct _GtkProjectMPrivate * priv);
static gboolean _gtk_projectm_redraw_impl(GtkWidget *widget);
static gboolean _gtk_projectm_expose_impl(GtkWidget *widget, GdkEventExpose *event, gpointer data);
static gboolean _gtk_projectm_configure_impl(GtkWidget *widget, GdkEventConfigure *event, gpointer data);
static void _gtk_projectm_destroy_impl(GtkWidget *widget);

struct _GtkProjectMPrivate {
    projectM *pm;
    GdkGLConfig *glconfig;
    GtkWidget *drawing_area;
    gint idle_id;
    GTimer *timer;
    gint frames;
};

extern "C" GtkWidget *
gtk_projectm_new(void)
{
    struct _GtkProjectMPrivate *priv = g_slice_new0(struct _GtkProjectMPrivate);

    gtk_gl_init(NULL, NULL);

    priv->glconfig = gdk_gl_config_new_by_mode((GdkGLConfigMode) (GDK_GL_MODE_RGBA | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE));
    if (!priv->glconfig)
        return NULL;

    priv->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(priv->drawing_area, 512, 512);
    gtk_widget_set_gl_capability(priv->drawing_area, priv->glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);
    gtk_widget_add_events(priv->drawing_area, GDK_VISIBILITY_NOTIFY_MASK);

    g_signal_connect_after(G_OBJECT(priv->drawing_area), "realize",
                           G_CALLBACK(_gtk_projectm_realize_impl), priv);
    g_signal_connect (priv->drawing_area, "unrealize", (GCallback) unrealize_cb,
     priv);
    g_signal_connect(G_OBJECT(priv->drawing_area), "expose_event",
                     G_CALLBACK(_gtk_projectm_expose_impl), priv);
    g_signal_connect(G_OBJECT(priv->drawing_area), "destroy",
                     G_CALLBACK(_gtk_projectm_destroy_impl), priv);

    priv->timer = g_timer_new();
    priv->frames = 0;

    g_object_set_data(G_OBJECT(priv->drawing_area), "GtkProjectMPrivate", priv);

    return priv->drawing_area;
}

extern "C" void
gtk_projectm_add_pcm_data(GtkWidget *widget, gint16 pcm_data[2][512])
{
    struct _GtkProjectMPrivate *priv = (struct _GtkProjectMPrivate *) g_object_get_data(G_OBJECT(widget), "GtkProjectMPrivate");

    g_return_if_fail(priv != NULL);

    if (priv->pm != NULL)
        priv->pm->pcm()->addPCM16 (pcm_data);
}

extern "C" void
gtk_projectm_toggle_preset_lock(GtkWidget *widget)
{
    struct _GtkProjectMPrivate *priv = (struct _GtkProjectMPrivate *) g_object_get_data(G_OBJECT(widget), "GtkProjectMPrivate");

    g_return_if_fail(priv != NULL);
    g_return_if_fail(priv->pm != NULL);

    priv->pm->key_handler(PROJECTM_KEYDOWN, PROJECTM_K_l, PROJECTM_KMOD_LSHIFT);
}

extern "C" void
gtk_projectm_preset_prev(GtkWidget *widget)
{
    struct _GtkProjectMPrivate *priv = (struct _GtkProjectMPrivate *) g_object_get_data(G_OBJECT(widget), "GtkProjectMPrivate");

    g_return_if_fail(priv != NULL);
    g_return_if_fail(priv->pm != NULL);

    priv->pm->key_handler(PROJECTM_KEYDOWN, PROJECTM_K_p, PROJECTM_KMOD_LSHIFT);
}

extern "C" void
gtk_projectm_preset_next(GtkWidget *widget)
{
    struct _GtkProjectMPrivate *priv = (struct _GtkProjectMPrivate *) g_object_get_data(G_OBJECT(widget), "GtkProjectMPrivate");

    g_return_if_fail(priv != NULL);
    g_return_if_fail(priv->pm != NULL);

    priv->pm->key_handler(PROJECTM_KEYDOWN, PROJECTM_K_n, PROJECTM_KMOD_LSHIFT);
}

static void
_gtk_projectm_realize_impl(GtkWidget *widget, gpointer data)
{
    GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);
    struct _GtkProjectMPrivate *priv = (struct _GtkProjectMPrivate *) data;

    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return;

    std::string configFile = read_config();
    priv->pm = new projectM(configFile);

    priv->pm->projectM_resetGL(widget->allocation.width, widget->allocation.height);

    gdk_gl_drawable_swap_buffers(gldrawable);
    gdk_gl_drawable_gl_end(gldrawable);

    g_signal_connect(G_OBJECT(widget), "configure_event",
                     G_CALLBACK(_gtk_projectm_configure_impl), priv);

    priv->idle_id = g_timeout_add (1000 / 30,
                                   (GSourceFunc) _gtk_projectm_redraw_impl,
                                   priv->drawing_area);
}

static void unrealize_cb (GtkWidget * widget, struct _GtkProjectMPrivate * priv)
{
    if (priv->idle_id)
    {
        g_source_remove (priv->idle_id);
        priv->idle_id = 0;
    }

    if (priv->pm != NULL)
    {
        delete priv->pm;
        priv->pm = NULL;
    }
}

static gboolean
_gtk_projectm_configure_impl(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
    GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);
    struct _GtkProjectMPrivate *priv = (struct _GtkProjectMPrivate *) data;

    if (priv->pm == NULL)
        return FALSE;

    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return FALSE;

    priv->pm->projectM_resetGL(widget->allocation.width, widget->allocation.height);

    gdk_gl_drawable_swap_buffers(gldrawable);
    gdk_gl_drawable_gl_end(gldrawable);

    return TRUE;
}

static gboolean
_gtk_projectm_expose_impl(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);
    struct _GtkProjectMPrivate *priv = (struct _GtkProjectMPrivate *) data;

    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return FALSE;

    priv->pm->renderFrame();
    priv->frames++;

    gdk_gl_drawable_swap_buffers(gldrawable);
    gdk_gl_drawable_gl_end(gldrawable);

    gdouble seconds = g_timer_elapsed (priv->timer, NULL);
    if (seconds >= 5.0)
    {
        gdouble fps = priv->frames / seconds;
        g_print ("%d frames in %6.3f seconds = %6.3f FPS\n", priv->frames, seconds, fps);
        g_timer_reset (priv->timer);
        priv->frames = 0;
    }

    return TRUE;
}

static gboolean
_gtk_projectm_redraw_impl(GtkWidget *widget)
{
    gdk_window_invalidate_rect(widget->window, &widget->allocation, FALSE);

    return TRUE;
}

static void
_gtk_projectm_destroy_impl(GtkWidget *widget)
{
    struct _GtkProjectMPrivate *priv = (struct _GtkProjectMPrivate *) g_object_get_data(G_OBJECT(widget), "GtkProjectMPrivate");

    if (priv->idle_id)
        g_source_remove(priv->idle_id);

    delete priv->pm;
    g_free(priv->timer);
    g_slice_free(struct _GtkProjectMPrivate, priv);
}

/********************************************************************************
 * XXX: This code is from projectM and still needs to be rewritten!             *
 ********************************************************************************/
static std::string read_config()
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
