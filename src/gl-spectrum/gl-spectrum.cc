/*
 * OpenGL Spectrum Analyzer for Audacious
 * Copyright 2013 Christophe Budé, John Lindgren, and Carlo Bramini
 *
 * Based on the XMMS plugin:
 * Copyright 1998-2000 Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson, and
 *                     4Front Technologies
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <math.h>
#include <string.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <GL/gl.h>

#ifdef GDK_WINDOWING_X11
#include <GL/glx.h>
#include <gdk/gdkx.h>
#endif

#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#endif

#define NUM_BANDS 32
#define DB_RANGE 40

#define BAR_SPACING (3.2f / NUM_BANDS)
#define BAR_WIDTH (0.8f * BAR_SPACING)

static const char gl_about[] =
 N_("OpenGL Spectrum Analyzer for Audacious\n"
    "Copyright 2013 Christophe Budé, John Lindgren, and Carlo Bramini\n\n"
    "Based on the XMMS plugin:\n"
    "Copyright 1998-2000 Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson, "
    "and 4Front Technologies\n\n"
    "License: GPLv2+");

class GLSpectrum : public VisPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("OpenGL Spectrum Analyzer"),
        PACKAGE,
        gl_about,
        nullptr, // prefs
        PluginGLibOnly
    };

    constexpr GLSpectrum () : VisPlugin (info, Visualizer::Freq) {}

    bool init ();

    void * get_gtk_widget ();

    void clear ();
    void render_freq (const float * freq);
};

EXPORT GLSpectrum aud_plugin_instance;

static float logscale[NUM_BANDS + 1];
static float colors[NUM_BANDS][NUM_BANDS][3];

#ifdef GDK_WINDOWING_X11
static Display * s_display;
static Window s_xwindow;
static GLXContext s_context;
#endif

#ifdef GDK_WINDOWING_WIN32
static HWND s_hwnd;
static HDC s_hdc;
static HGLRC s_glrc;
#endif

static GtkWidget * s_widget = nullptr;

static int s_pos = 0;
static float s_angle = 25, s_anglespeed = 0.05f;
static float s_bars[NUM_BANDS][NUM_BANDS];

bool GLSpectrum::init ()
{
    for (int i = 0; i <= NUM_BANDS; i ++)
        logscale[i] = powf (256, (float) i / NUM_BANDS) - 0.5f;

    for (int y = 0; y < NUM_BANDS; y ++)
    {
        float yf = (float) y / (NUM_BANDS - 1);

        for (int x = 0; x < NUM_BANDS; x ++)
        {
            float xf = (float) x / (NUM_BANDS - 1);

            colors[x][y][0] = (1 - xf) * (1 - yf);
            colors[x][y][1] = xf;
            colors[x][y][2] = yf;
        }
    }

    return true;
}

/* stolen from the skins plugin */
/* convert linear frequency graph to logarithmic one */
static void make_log_graph (const float * freq, float * graph)
{
    for (int i = 0; i < NUM_BANDS; i ++)
    {
        /* sum up values in freq array between logscale[i] and logscale[i + 1],
           including fractional parts */
        int a = ceilf (logscale[i]);
        int b = floorf (logscale[i + 1]);
        float sum = 0;

        if (b < a)
            sum += freq[b] * (logscale[i + 1] - logscale[i]);
        else
        {
            if (a > 0)
                sum += freq[a - 1] * (a - logscale[i]);
            for (; a < b; a ++)
                sum += freq[a];
            if (b < 256)
                sum += freq[b] * (logscale[i + 1] - b);
        }

        /* fudge factor to make the graph have the same overall height as a
           12-band one no matter how many bands there are */
        sum *= (float) NUM_BANDS / 12;

        /* convert to dB */
        float val = 20 * log10f (sum);

        /* scale (-DB_RANGE, 0.0) to (0.0, 1.0) */
        val = 1 + val / DB_RANGE;

        graph[i] = aud::clamp (val, 0.0f, 1.0f);
    }
}

void GLSpectrum::render_freq (const float * freq)
{
    make_log_graph (freq, s_bars[s_pos]);
    s_pos = (s_pos + 1) % NUM_BANDS;

    s_angle += s_anglespeed;
    if (s_angle > 45 || s_angle < -45)
        s_anglespeed = -s_anglespeed;

    if (s_widget)
        gtk_widget_queue_draw (s_widget);
}

void GLSpectrum::clear ()
{
    memset (s_bars, 0, sizeof s_bars);

    if (s_widget)
        gtk_widget_queue_draw (s_widget);
}

static void draw_rectangle (float x1, float y1, float z1, float x2, float y2,
 float z2, float r, float g, float b)
{
    glColor3f (r, g, b);

    glBegin (GL_POLYGON);
    glVertex3f (x1, y2, z1);
    glVertex3f (x2, y2, z1);
    glVertex3f (x2, y2, z2);
    glVertex3f (x1, y2, z2);
    glEnd ();

    glColor3f (0.65f * r, 0.65f * g, 0.65f * b);

    glBegin (GL_POLYGON);
    glVertex3f (x1, y1, z1);
    glVertex3f (x1, y2, z1);
    glVertex3f (x1, y2, z2);
    glVertex3f (x1, y1, z2);
    glEnd ();

    glBegin (GL_POLYGON);
    glVertex3f (x2, y2, z1);
    glVertex3f (x2, y1, z1);
    glVertex3f (x2, y1, z2);
    glVertex3f (x2, y2, z2);
    glEnd ();

    glColor3f (0.8f * r, 0.8f * g, 0.8f * b);

    glBegin (GL_POLYGON);
    glVertex3f (x1, y1, z1);
    glVertex3f (x2, y1, z1);
    glVertex3f (x2, y2, z1);
    glVertex3f (x1, y2, z1);
    glEnd ();
}

static void draw_bar (float x, float z, float h, float r, float g, float b)
{
    draw_rectangle (x, 0, z, x + BAR_WIDTH, h, z + BAR_WIDTH,
     r * (0.2f + 0.8f * h), g * (0.2f + 0.8f * h), b * (0.2f + 0.8f * h));
}

static void draw_bars ()
{
    glPushMatrix ();
    glTranslatef (0.0f, -0.5f, -5.0f);
    glRotatef (38.0f, 1.0f, 0.0f, 0.0f);
    glRotatef (s_angle + 180.0f, 0.0f, 1.0f, 0.0f);

    for (int i = 0; i < NUM_BANDS; i ++)
    {
        float z = -1.6f + (NUM_BANDS - i) * BAR_SPACING;

        for (int j = 0; j < NUM_BANDS; j ++)
        {
            draw_bar (1.6f - BAR_SPACING * j, z,
             s_bars[(s_pos + i) % NUM_BANDS][j] * 1.6,
             colors[i][j][0], colors[i][j][1], colors[i][j][2]);
        }
    }

    glPopMatrix ();
}

static gboolean draw_cb (GtkWidget * widget)
{
#ifdef GDK_WINDOWING_X11
    if (! s_context)
        return false;
#endif

#ifdef GDK_WINDOWING_WIN32
    if (! s_glrc)
        return false;
#endif

    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_bars ();

#ifdef GDK_WINDOWING_X11
    glXSwapBuffers (s_display, s_xwindow);
#endif

#ifdef GDK_WINDOWING_WIN32
    SwapBuffers (s_hdc);
#endif

    return true;
}

static void aspect_viewport(GLint width, GLint height)
{
    glViewport (0, 0, width, height);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glFrustum (-1.1f, 1, -1.5f, 1, 2, 10);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
}

static void widget_realized ()
{
    GdkWindow * window = gtk_widget_get_window (s_widget);

#ifdef GDK_WINDOWING_X11
    GdkScreen * screen = gdk_window_get_screen (window);
    int nscreen = GDK_SCREEN_XNUMBER (screen);

    s_display = GDK_SCREEN_XDISPLAY (screen);
    s_xwindow = GDK_WINDOW_XID (window);

    /* Create s_context */
    int attribs[] = {
     GLX_RGBA,
     GLX_RED_SIZE, 1,
     GLX_GREEN_SIZE, 1,
     GLX_BLUE_SIZE, 1,
     GLX_ALPHA_SIZE, 1,
     GLX_DOUBLEBUFFER,
     GLX_DEPTH_SIZE, 1,
     None
    };

    XVisualInfo * xvinfo = glXChooseVisual (s_display, nscreen, attribs);
    g_return_if_fail (xvinfo);

    /* Fix up visual/colormap */
    GdkVisual * visual = gdk_x11_screen_lookup_visual (screen, xvinfo->visualid);
    g_return_if_fail (visual);

    gtk_widget_set_visual (s_widget, visual);

    s_context = glXCreateContext (s_display, xvinfo, 0, true);
    g_return_if_fail (s_context);

    XFree (xvinfo);

    glXMakeCurrent (s_display, s_xwindow, s_context);
#endif

#ifdef GDK_WINDOWING_WIN32
    s_hwnd = (HWND) GDK_WINDOW_HWND (window);
    s_hdc = GetDC (s_hwnd);

    PIXELFORMATDESCRIPTOR desc = {
     sizeof (PIXELFORMATDESCRIPTOR),
     1,                                 // version number (?)
     PFD_DRAW_TO_WINDOW |               // format must support window
     PFD_SUPPORT_OPENGL |               // format must support OpenGL
     PFD_DOUBLEBUFFER,                  // must support double buffering
     PFD_TYPE_RGBA,                     // request an RGBA format
     24,                                // select a 8:8:8 bit color depth
     0, 0, 0, 0, 0, 0,                  // color bits ignored (?)
     0,                                 // no alpha buffer
     0,                                 // shift bit ignored (?)
     0,                                 // no accumulation buffer
     0, 0, 0, 0,                        // accumulation bits ignored (?)
     16,                                // 16-bit z-buffer (depth buffer)
     0,                                 // no stencil buffer
     0,                                 // no auxiliary buffer (?)
     PFD_MAIN_PLANE,                    // main drawing layer
     0,                                 // reserved (?)
     0, 0, 0                            // layer masks ignored (?)
    };

    int format = ChoosePixelFormat (s_hdc, & desc);
    g_return_if_fail (format != 0);

    SetPixelFormat (s_hdc, format, & desc);

    s_glrc = wglCreateContext (s_hdc);
    g_return_if_fail (s_glrc);

    wglMakeCurrent (s_hdc, s_glrc);
#endif

    /* Initialize OpenGL */
    GtkAllocation alloc;
    gtk_widget_get_allocation (s_widget, & alloc);

    aspect_viewport (alloc.width, alloc.height);

    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LESS);
    glDepthMask (GL_TRUE);
    glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
    glClearColor (0, 0, 0, 1);
}

static void widget_destroyed ()
{
    s_widget = nullptr;

#ifdef GDK_WINDOWING_X11
    if (s_context)
    {
        glXMakeCurrent (s_display, None, nullptr);
        glXDestroyContext (s_display, s_context);
        s_context = nullptr;
    }

    s_display = nullptr;
#endif

#ifdef GDK_WINDOWING_WIN32
    if (s_glrc)
    {
        wglMakeCurrent (s_hdc, nullptr);
        wglDeleteContext (s_glrc);
        s_glrc = nullptr;
    }

    if (s_hdc)
    {
        ReleaseDC (s_hwnd, s_hdc);
        s_hdc = nullptr;
    }

    s_hwnd = nullptr;
#endif
}

static void widget_resize (GtkWidget * widget, GdkEvent * event, gpointer data)
{
    aspect_viewport (event->configure.width, event->configure.height);
}

void * GLSpectrum::get_gtk_widget ()
{
    if (s_widget)
        return s_widget;

    s_widget = gtk_drawing_area_new ();

    g_signal_connect (s_widget, "expose-event", (GCallback) draw_cb, nullptr);
    g_signal_connect (s_widget, "realize", (GCallback) widget_realized, nullptr);
    g_signal_connect (s_widget, "destroy", (GCallback) widget_destroyed, nullptr);
    g_signal_connect (s_widget, "configure-event", (GCallback) widget_resize, nullptr);

    /* Disable GTK double buffering */
    gtk_widget_set_double_buffered (s_widget, false);

    return s_widget;
}
