/*
 * OpenGL Spectrum Analyzer for Audacious
 * Copyright 2013 Christophe Budé, John Lindgren, and Carlo Bramini
 * Copyright 2014 William Pitcock
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

#include <QGLWidget>
#include <QGLFunctions>

#define NUM_BANDS 32
#define DB_RANGE 40

#define BAR_SPACING (3.2f / NUM_BANDS)
#define BAR_WIDTH (0.8f * BAR_SPACING)

static const char gl_about[] =
 N_("OpenGL Spectrum Analyzer for Audacious\n"
    "Copyright 2013 Christophe Budé, John Lindgren, and Carlo Bramini\n"
    "Copyright 2014 William Pitcock\n\n"
    "Based on the XMMS plugin:\n"
    "Copyright 1998-2000 Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson, "
    "and 4Front Technologies\n\n"
    "License: GPLv2+");

class GLSpectrumQt : public VisPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("OpenGL Spectrum Analyzer"),
        PACKAGE,
        gl_about,
        nullptr, // prefs
        PluginQtOnly
    };

    constexpr GLSpectrumQt () : VisPlugin (info, Visualizer::Freq) {}

    bool init ();

    void * get_qt_widget ();

    void clear ();
    void render_freq (const float * freq);
};

EXPORT GLSpectrumQt aud_plugin_instance;

static float logscale[NUM_BANDS + 1];
static float colors[NUM_BANDS][NUM_BANDS][3];

static int s_pos = 0;
static float s_angle = 25, s_anglespeed = 0.05f;
static float s_bars[NUM_BANDS][NUM_BANDS];

class GLSpectrumWidget : public QGLWidget, protected QGLFunctions
{
public:
    GLSpectrumWidget (QWidget *parent = nullptr);
    ~GLSpectrumWidget ();

private:
    void paintGL ();
    void resizeGL (int w, int h);
};

GLSpectrumWidget * s_widget = nullptr;

bool GLSpectrumQt::init ()
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

void GLSpectrumQt::render_freq (const float * freq)
{
    make_log_graph (freq, s_bars[s_pos]);
    s_pos = (s_pos + 1) % NUM_BANDS;

    s_angle += s_anglespeed;
    if (s_angle > 45 || s_angle < -45)
        s_anglespeed = -s_anglespeed;

    if (s_widget)
        s_widget->updateGL ();
}

void GLSpectrumQt::clear ()
{
#ifdef XXX_NOTYET
    memset (s_bars, 0, sizeof s_bars);

    if (s_widget)
        s_widget->updateGL ();
#endif
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
    glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

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

    glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix ();
}

GLSpectrumWidget::GLSpectrumWidget (QWidget * parent) : QGLWidget (parent)
{
    setObjectName ("GLSpectrumWidget");
}

GLSpectrumWidget::~GLSpectrumWidget ()
{
    s_widget = nullptr;
}

void GLSpectrumWidget::paintGL ()
{
    glDisable (GL_BLEND);
    glMatrixMode (GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glFrustum (-1.1f, 1, -1.5f, 1, 2, 10);
    glMatrixMode (GL_MODELVIEW);
    glPushMatrix ();
    glLoadIdentity ();
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LESS);
    glPolygonMode (GL_FRONT, GL_FILL);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor (0, 0, 0, 1);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_bars ();

    glPopMatrix ();
    glMatrixMode (GL_PROJECTION);
    glPopMatrix ();
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_BLEND);
    glDepthMask (GL_TRUE);
}

void GLSpectrumWidget::resizeGL (int w, int h)
{
    glViewport (0, 0, w, h);
}

void * GLSpectrumQt::get_qt_widget ()
{
    if (s_widget)
        return s_widget;

    s_widget = new GLSpectrumWidget;
    return s_widget;
}
