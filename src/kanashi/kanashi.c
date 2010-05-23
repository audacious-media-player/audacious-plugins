/*
 * kanashi: iterated javascript-driven visualization framework
 * Copyright (c) 2006, 2007 William Pitcock <nenolod@dereferenced.org>
 * Portions copyright (c) 2001 Jamie Gennis <jgennis@mindspring.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "kanashi.h"
#include "jsglue.h"

GtkWidget *kanashi_win = NULL;
GdkRgbCmap *cmap = NULL;

/* Globals */
struct kanashi_rc         *kanashi_rc;
struct kanashi_image_data *kanashi_image_data;
struct kanashi_sound_data *kanashi_sound_data;

/* Trig Pre-Computes */
float sin_val[360];
float cos_val[360];

gboolean kanashi_new_beat;

JSRuntime *rt = NULL;
JSContext *cx = NULL;
JSObject  *global = NULL;

GStaticMutex kanashi_mutex = G_STATIC_MUTEX_INIT;
gint render_timeout = 0;
extern GCond *render_cond;

/* **************** drawing doodads **************** */
static void
set_colormap (void)
{
  guint32 colors[256];
  gint i;

  if (cmap != NULL)
      gdk_rgb_cmap_free(cmap);

  for (i = 0; i < 256; i++) {
     colors[i] =
         (kanashi_image_data->cmap[i].r << 16) |
         (kanashi_image_data->cmap[i].g << 8) |
         (kanashi_image_data->cmap[i].b);
  }

  cmap = gdk_rgb_cmap_new(colors, 256);
}

static gboolean
blit_to_screen (gpointer unused)
{
  kanashi_render ();
  set_colormap();

  gdk_draw_indexed_image(kanashi_win->window, kanashi_win->style->white_gc, 0, 0,
                         kanashi_image_data->width, kanashi_image_data->height, GDK_RGB_DITHER_NONE, 
                         kanashi_image_data->surface[0], kanashi_image_data->width, cmap);

  return TRUE;
}

static void
resize_video (guint w, guint h)
{
  g_static_mutex_lock(&kanashi_mutex);

  kanashi_image_data->width = w;
  kanashi_image_data->height = h;

  if (kanashi_image_data->surface[0])
    g_free (kanashi_image_data->surface[0]);
  if (kanashi_image_data->surface[1])
    g_free (kanashi_image_data->surface[1]);

  kanashi_image_data->surface[0] = g_malloc0 (w * h);
  kanashi_image_data->surface[1] = g_malloc0 (w * h);

  g_static_mutex_unlock(&kanashi_mutex);
}

/* **************** basic renderer management **************** */
void
kanashi_cleanup (void)
{
  if (kanashi_image_data)
    {
      if (kanashi_image_data->surface[0])
	g_free (kanashi_image_data->surface[0]);
      if (kanashi_image_data->surface[1])
	g_free (kanashi_image_data->surface[1]);
      g_free (kanashi_image_data);
    }
  if (kanashi_sound_data)
    g_free (kanashi_sound_data);

  if (render_timeout)
    g_source_remove(render_timeout);
}

/* Renders one frame and handles the SDL window */
void
kanashi_render (void)
{
  jsval rval;

  kanashi_new_beat = kanashi_is_new_beat();

  if (!JS_CallFunctionName(cx, global, "render_scene", 0, NULL, &rval))
      printf("something broke\n");
}

/* this MUST be called if a builtin's output is to surface[1]
   (by the builtin, after it is done) */
void
kanashi_swap_surfaces (void)
{
  guchar *tmp = kanashi_image_data->surface[0];
  kanashi_image_data->surface[0] = kanashi_image_data->surface[1];
  kanashi_image_data->surface[1] = tmp;
}

/******* presets *******/
static JSClass global_class = {
        "global", JSCLASS_GLOBAL_FLAGS,
        JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
        JSCLASS_NO_OPTIONAL_MEMBERS
};

void
kanashi_report_js_error(JSContext *cx, const char *message, JSErrorReport *report)
{
    printf("%s:%u:%s\n",
            report->filename ? report->filename : "<no filename>",
            (unsigned int) report->lineno,
            message);
}


JSBool
kanashi_branch_callback_hook(JSContext *cx_, JSScript *script)
{
    static int counter = 0;

    if (++counter == 2000) {
        JS_MaybeGC(cx_);
        counter = 0;
    }

    return JS_TRUE;
}

gboolean
kanashi_load_preset(const char *filename)
{
    gchar *data;
    gsize len;
    GError *err = NULL;
    JSScript *script;
    jsval rval;

    if (g_file_get_contents(filename, &data, &len, &err)) 
    {
        script = JS_CompileScript(cx, global, data, len, filename, 1);
        if (script != NULL) 
        {
            JS_ExecuteScript(cx, global, script, &rval);
        }
        else 
        {
            printf("compilation failed\n");
            return FALSE;
        }
    }
    else 
    {
        printf("loading failed\n");
        return FALSE;
    }

    g_free(data);

    return TRUE;
}

static JSFunctionSpec js_global_functions[] = {
    {"value_reduce",               kanashi_js_fade,                       1,      0,      0},
    {"value_invert",               kanashi_js_invert,                     0,      0,      0},
    {"blur",                       kanashi_js_blur,                       1,      0,      0},
    {"mosaic",                     kanashi_js_mosaic,                     1,      0,      0},
    {"set_colormap_gradient",      kanashi_js_set_colormap_gradient,      5,      0,      0},
    {"render_horizontal_waveform", kanashi_js_render_horizontal_waveform, 2,      0,      0},
    {"render_vertical_waveform",   kanashi_js_render_vertical_waveform,   2,      0,      0},
    {"render_line",                kanashi_js_render_line,                5,      0,      0},
    {"render_dot",                 kanashi_js_render_dot,                 3,      0,      0},
    {"is_beat",                    kanashi_js_is_beat,                    0,      0,      0},
    {"get_canvas_width",           kanashi_js_get_canvas_width,           0,      0,      0},
    {"get_canvas_height",          kanashi_js_get_canvas_height,          0,      0,      0},
    {"get_pcm_data",               kanashi_js_get_pcm_data,               0,      0,      0},
    {"translate_polar_x",          kanashi_js_translate_polar_x,          1,      0,      0},
    {"translate_polar_y",          kanashi_js_translate_polar_y,          1,      0,      0},
    {},
};

gboolean
kanashi_reconfigure(GtkWidget *widget, GdkEventConfigure *event, gpointer unused)
{
    resize_video (event->width, event->height);

    return FALSE;
}

gboolean
kanashi_init(void)
{
    int i;

    kanashi_sound_data = g_new0 (struct kanashi_sound_data, 1);
    kanashi_image_data = g_new0 (struct kanashi_image_data, 1);

    kanashi_win = gtk_drawing_area_new();
    gtk_widget_realize(kanashi_win);
    gtk_widget_set_size_request(kanashi_win, 640, 360);
    gtk_widget_show(kanashi_win);
    g_signal_connect(G_OBJECT(kanashi_win), "configure-event", G_CALLBACK(kanashi_reconfigure), NULL);

    resize_video (640, 360);

    for (i=0; i<360; i++)
    {
        sin_val[i] = sin(i*(M_PI/180.0));
        cos_val[i] = cos(i*(M_PI/180.0));
    }

    rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (rt == NULL)
    {
        return FALSE;
    }

    /* Create a context. */
    cx = JS_NewContext(rt, 8192);
    if (cx == NULL)
    {
        return FALSE;
    }

    JS_SetOptions(cx, JSOPTION_VAROBJFIX);
    JS_SetVersion(cx, JSVERSION_1_7);
    JS_SetErrorReporter(cx, kanashi_report_js_error);
    JS_SetBranchCallback(cx, kanashi_branch_callback_hook);

    global = JS_NewObject(cx, &global_class, NULL, NULL);
    if (global == NULL)
    {
        return FALSE;
    }

    if (!JS_InitStandardClasses(cx, global))
    {
        return FALSE;
    }

    if (!JS_DefineFunctions(cx, global, js_global_functions))
    {
        return FALSE;
    }

    kanashi_load_preset(PRESET_PATH "/nenolod_-_kanashi_default.js");

    g_signal_connect(kanashi_win, "expose-event", G_CALLBACK(blit_to_screen), NULL);
    render_timeout = g_timeout_add(16, (GSourceFunc) gtk_widget_queue_draw, kanashi_win);

    return TRUE;
}
