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

#include <SDL.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "kanashi.h"
#include "jsglue.h"

/* SDL stuffs */
SDL_Surface *screen;

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


/* **************** drawing doodads **************** */

static void
blit_to_screen (void)
{
  int j;

  SDL_LockSurface(screen);

  SDL_SetPalette(screen, SDL_LOGPAL|SDL_PHYSPAL,
		 (SDL_Color*)kanashi_image_data->cmap, 0, 256);
  SDL_SetAlpha(screen, 0, 255);

  for (j=0; j<kanashi_image_data->height; j++)
      memcpy(screen->pixels + j*screen->pitch,
	     kanashi_image_data->surface[0] + j*kanashi_image_data->width,
	     kanashi_image_data->width);

  SDL_UnlockSurface(screen);

  SDL_Flip(screen);
}

static void
resize_video (guint w, guint h)
{
  kanashi_image_data->width = w;
  kanashi_image_data->height = h;

  if (kanashi_image_data->surface[0])
    g_free (kanashi_image_data->surface[0]);
  if (kanashi_image_data->surface[1])
    g_free (kanashi_image_data->surface[1]);

  kanashi_image_data->surface[0] = g_malloc0 (w * h);
  kanashi_image_data->surface[1] = g_malloc0 (w * h);

  screen = SDL_SetVideoMode (w, h, 8, SDL_HWSURFACE |
			     SDL_HWPALETTE | SDL_RESIZABLE | SDL_DOUBLEBUF);
  if (! screen)
    kanashi_fatal_error ("Unable to create a new SDL window: %s",
		    SDL_GetError ());
}

static void
take_screenshot (void)
{
  char fname[32];
  struct stat buf;
  int i=0;

  do
    sprintf (fname, "kanashi_%05d.bmp", ++i);
  while (stat (fname, &buf) == 0);

  SDL_SaveBMP (screen, fname);
}

/* FIXME: This should resize the video to a user-set
   fullscreen res */
static void
toggle_fullscreen (void)
{
  SDL_WM_ToggleFullScreen (screen);  
  if (SDL_ShowCursor (SDL_QUERY) == SDL_ENABLE)
    SDL_ShowCursor (SDL_DISABLE);
  else
    SDL_ShowCursor (SDL_ENABLE);
}

/* **************** basic renderer management **************** */
void
kanashi_cleanup (void)
{
  SDL_FreeSurface (screen);
  SDL_Quit ();


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
}

/* Renders one frame and handles the SDL window */
void
kanashi_render (void)
{
  SDL_Event event;
  jsval rval;

  /* Handle window events */
  while (SDL_PollEvent (&event))
    {
      switch (event.type)
	{
	case SDL_QUIT:
	  kanashi_quit ();
	  g_assert_not_reached ();
	case SDL_KEYDOWN:
	  switch (event.key.keysym.sym)
	    {
	    case SDLK_ESCAPE:
	      kanashi_quit ();
	      g_assert_not_reached ();
	    case SDLK_RETURN:
	      if (event.key.keysym.mod & (KMOD_ALT | KMOD_META))
		toggle_fullscreen ();
	      break;
	    case SDLK_BACKQUOTE:
	      take_screenshot ();
	      break;
	    default:
              break;
	    }
	  break;
	case SDL_VIDEORESIZE:
	  resize_video (event.resize.w, event.resize.h);	  
	  break;
	}
    }

  kanashi_new_beat = kanashi_is_new_beat();

  if (!JS_CallFunctionName(cx, global, "render_scene", 0, NULL, &rval))
      printf("something broke\n");

  blit_to_screen ();
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
    JS_FS("value_reduce",               kanashi_js_fade,                       1,      0,      0),
    JS_FS("value_invert",               kanashi_js_invert,                     0,      0,      0),
    JS_FS("blur",                       kanashi_js_blur,                       1,      0,      0),
    JS_FS("mosaic",                     kanashi_js_mosaic,                     1,      0,      0),
    JS_FS("set_colormap_gradient",      kanashi_js_set_colormap_gradient,      5,      0,      0),
    JS_FS("render_horizontal_waveform", kanashi_js_render_horizontal_waveform, 2,      0,      0),
    JS_FS("render_vertical_waveform",   kanashi_js_render_vertical_waveform,   2,      0,      0),
    JS_FS("render_line",                kanashi_js_render_line,                5,      0,      0),
    JS_FS("render_dot",                 kanashi_js_render_dot,                 3,      0,      0),
    JS_FS("is_beat",                    kanashi_js_is_beat,                    0,      0,      0),
    JS_FS("get_canvas_width",           kanashi_js_get_canvas_width,           0,      0,      0),
    JS_FS("get_canvas_height",          kanashi_js_get_canvas_height,          0,      0,      0),
    JS_FS("get_pcm_data",               kanashi_js_get_pcm_data,               0,      0,      0),
    JS_FS("translate_polar_x",          kanashi_js_translate_polar_x,          1,      0,      0),
    JS_FS("translate_polar_y",          kanashi_js_translate_polar_y,          1,      0,      0),
    JS_FS_END
};

gboolean
kanashi_init(void)
{
    int i;

    kanashi_sound_data = g_new0 (struct kanashi_sound_data, 1);
    kanashi_image_data = g_new0 (struct kanashi_image_data, 1);

    if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE) < 0)
        kanashi_fatal_error ("Unable to initialize SDL: %s", SDL_GetError ());

    resize_video (640, 360);

    SDL_WM_SetCaption ("Kanashi", PACKAGE);

    for (i=0; i<360; i++)
    {
        sin_val[i] = sin(i*(M_PI/180.0));
        cos_val[i] = cos(i*(M_PI/180.0));
    }

    rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (rt == NULL)
        return FALSE;

    /* Create a context. */
    cx = JS_NewContext(rt, 8192);
    if (cx == NULL)
        return FALSE;
    JS_SetOptions(cx, JSOPTION_VAROBJFIX);
    JS_SetVersion(cx, JSVERSION_LATEST);
    JS_SetErrorReporter(cx, kanashi_report_js_error);
    JS_SetBranchCallback(cx, kanashi_branch_callback_hook);

    global = JS_NewObject(cx, &global_class, NULL, NULL);
    if (global == NULL)
        return FALSE;

    if (!JS_InitStandardClasses(cx, global))
        return FALSE;

    if (!JS_DefineFunctions(cx, global, js_global_functions))
        return FALSE;

    kanashi_load_preset(PRESET_PATH "/nenolod_-_kanashi_default.js");

    return TRUE;
}
