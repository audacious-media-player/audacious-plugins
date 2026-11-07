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

#include <epoxy/gl.h>
#include <gtk/gtk.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>
#include <libaudcore/templates.h>

#define NUM_BANDS 32
#define DB_RANGE 40

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

    bool init () override;

    void * get_gtk_widget () override;

    void clear () override;
    void render_freq (const float * freq) override;
};

EXPORT GLSpectrum aud_plugin_instance;

static GtkWidget * s_widget = nullptr;

static float s_logscale[NUM_BANDS + 1];
static float s_bars[NUM_BANDS][NUM_BANDS];
static int s_pos = 0;
static float s_angle = 25, s_anglespeed = 0.05f;

bool GLSpectrum::init ()
{
    compute_log_xscale (s_logscale, NUM_BANDS);
    return true;
}

void GLSpectrum::render_freq (const float * freq)
{
    float * bars = s_bars[s_pos];

    for (int i = 0; i < NUM_BANDS; i ++)
    {
        /* scale (-DB_RANGE, 0.0) to (0.0, 1.0) */
        float val = 1 + compute_freq_band (freq, s_logscale, i, NUM_BANDS) / DB_RANGE;
        bars[i] = aud::clamp (val, 0.0f, 1.0f);
    }

    s_pos = (s_pos + 1) % NUM_BANDS;

    s_angle += s_anglespeed;
    if (s_angle > 45 || s_angle < -45)
        s_anglespeed = -s_anglespeed;

    if (s_widget)
        gtk_gl_area_queue_render ((GtkGLArea *) s_widget);
}

void GLSpectrum::clear ()
{
    memset (s_bars, 0, sizeof s_bars);

    if (s_widget)
        gtk_gl_area_queue_render ((GtkGLArea *) s_widget);
}

/* Minimal 4x4 matrix math (replaces glFrustum / glTranslatef / glRotatef) */
struct Mat4
{
    float m[16]; // column-major, as expected by glUniformMatrix4fv

    static Mat4 identity ()
    {
        return {{1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1}};
    }

    static Mat4 frustum (float l, float r, float b, float t, float n, float f)
    {
        Mat4 p {};
        p.m[0] = 2 * n / (r - l);
        p.m[5] = 2 * n / (t - b);
        p.m[8] = (r + l) / (r - l);
        p.m[9] = (t + b) / (t - b);
        p.m[10] = -(f + n) / (f - n);
        p.m[11] = -1;
        p.m[14] = -2 * f * n / (f - n);
        return p;
    }

    static Mat4 translate (float x, float y, float z)
    {
        Mat4 t = identity ();
        t.m[12] = x;
        t.m[13] = y;
        t.m[14] = z;
        return t;
    }

    static Mat4 rotate_x (float deg)
    {
        Mat4 r = identity ();
        float c = cosf (deg * (float) G_PI / 180);
        float s = sinf (deg * (float) G_PI / 180);
        r.m[5] = c;  r.m[9] = -s;
        r.m[6] = s;  r.m[10] = c;
        return r;
    }

    static Mat4 rotate_y (float deg)
    {
        Mat4 r = identity ();
        float c = cosf (deg * (float) G_PI / 180);
        float s = sinf (deg * (float) G_PI / 180);
        r.m[0] = c;  r.m[8] = s;
        r.m[2] = -s; r.m[10] = c;
        return r;
    }

    friend Mat4 operator* (const Mat4 & a, const Mat4 & b)
    {
        Mat4 r {};
        for (int col = 0; col < 4; col ++)
            for (int row = 0; row < 4; row ++)
                for (int k = 0; k < 4; k ++)
                    r.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
        return r;
    }
};

static const char vertex_shader_body[] = R"glsl(
in vec3 a_position; // corner of a unit box, see bar_vertices
in float a_shade;   // per-face brightness

uniform mat4 u_mvp;
uniform highp sampler2D u_bars; // ring buffer of heights, x = band, y = row
uniform int u_pos;              // ring buffer row holding the oldest data

out vec3 v_color;

const float BAR_SPACING = 3.2 / float(NUM_BANDS);
const float BAR_WIDTH = 0.8 * BAR_SPACING;

void main()
{
    int row = gl_InstanceID / NUM_BANDS; // 0 = oldest, NUM_BANDS - 1 = newest
    int band = gl_InstanceID - row * NUM_BANDS;

    float h = 1.6 * texelFetch(u_bars, ivec2(band, (u_pos + row) % NUM_BANDS), 0).r;

    vec3 origin = vec3(1.6 - BAR_SPACING * float(band), 0.0,
                       -1.6 + BAR_SPACING * float(NUM_BANDS - row));

    gl_Position = u_mvp * vec4(origin + a_position * vec3(BAR_WIDTH, h, BAR_WIDTH), 1.0);

    float x = float(row) / float(NUM_BANDS - 1);
    float y = float(band) / float(NUM_BANDS - 1);
    vec3 color = vec3((1.0 - x) * (1.0 - y), x, y);

    v_color = color * a_shade * (0.2 + 0.8 * h);
}
)glsl";

static const char fragment_shader_body[] = R"glsl(
in vec3 v_color;
out vec4 frag_color;

void main()
{
    frag_color = vec4(min(v_color, vec3(1.0)), 1.0);
}
)glsl";

/* One bar as a unit box (x, y, z, shade).
 * Only the four faces that can ever face the camera are drawn. */
static const float bar_vertices[][4] = {
    /* top */
    {0, 1, 0, 1.0f},  {1, 1, 0, 1.0f},  {1, 1, 1, 1.0f},
    {0, 1, 0, 1.0f},  {1, 1, 1, 1.0f},  {0, 1, 1, 1.0f},
    /* left */
    {0, 0, 0, 0.65f}, {0, 1, 0, 0.65f}, {0, 1, 1, 0.65f},
    {0, 0, 0, 0.65f}, {0, 1, 1, 0.65f}, {0, 0, 1, 0.65f},
    /* right */
    {1, 1, 0, 0.65f}, {1, 0, 0, 0.65f}, {1, 0, 1, 0.65f},
    {1, 1, 0, 0.65f}, {1, 0, 1, 0.65f}, {1, 1, 1, 0.65f},
    /* front */
    {0, 0, 0, 0.8f},  {1, 0, 0, 0.8f},  {1, 1, 0, 0.8f},
    {0, 0, 0, 0.8f},  {1, 1, 0, 0.8f},  {0, 1, 0, 0.8f}
};

static constexpr int BAR_VERTEX_COUNT = aud::n_elems (bar_vertices);

enum {
    ATTRIB_POSITION = 0,
    ATTRIB_SHADE = 1
};

struct GLResources
{
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint texture = 0;
    GLint u_mvp = -1;
    GLint u_pos = -1;
};

/* Bound to the lifetime of the GtkGLArea's context */
static GLResources s_gl;

static GLuint compile_shader (GLenum type, const char * version,
 const char * body, GError * * error)
{
    const char * sources[] = {
        version,
        "#define NUM_BANDS ", aud::numeric_string<NUM_BANDS>::str, "\n",
        "precision highp float;\n",
        "precision highp int;\n",
        body
    };

    GLuint shader = glCreateShader (type);
    glShaderSource (shader, aud::n_elems (sources), sources, nullptr);
    glCompileShader (shader);

    GLint ok = GL_FALSE;
    glGetShaderiv (shader, GL_COMPILE_STATUS, & ok);

    if (! ok)
    {
        char log[1024] = "";
        glGetShaderInfoLog (shader, sizeof log, nullptr, log);
        g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
         "%s shader: %s", type == GL_VERTEX_SHADER ? "Vertex" : "Fragment", log);
        glDeleteShader (shader);
        return 0;
    }

    return shader;
}

static GLuint create_program (bool use_es, GError * * error)
{
    const char * version = use_es ? "#version 300 es\n" : "#version 150\n";

    GLuint vs = compile_shader (GL_VERTEX_SHADER, version, vertex_shader_body, error);
    if (! vs)
        return 0;

    GLuint fs = compile_shader (GL_FRAGMENT_SHADER, version, fragment_shader_body, error);
    if (! fs)
    {
        glDeleteShader (vs);
        return 0;
    }

    GLuint program = glCreateProgram ();
    glAttachShader (program, vs);
    glAttachShader (program, fs);
    glBindAttribLocation (program, ATTRIB_POSITION, "a_position");
    glBindAttribLocation (program, ATTRIB_SHADE, "a_shade");
    glLinkProgram (program);

    /* The program keeps the shaders alive as long as it needs them */
    glDeleteShader (vs);
    glDeleteShader (fs);

    GLint ok = GL_FALSE;
    glGetProgramiv (program, GL_LINK_STATUS, & ok);

    if (! ok)
    {
        char log[1024] = "";
        glGetProgramInfoLog (program, sizeof log, nullptr, log);
        g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
         "Linking shaders: %s", log);
        glDeleteProgram (program);
        return 0;
    }

    return program;
}

static bool create_resources (bool use_es, GError * * error)
{
    s_gl.program = create_program (use_es, error);
    if (! s_gl.program)
        return false;

    s_gl.u_mvp = glGetUniformLocation (s_gl.program, "u_mvp");
    s_gl.u_pos = glGetUniformLocation (s_gl.program, "u_pos");

    glUseProgram (s_gl.program);
    glUniform1i (glGetUniformLocation (s_gl.program, "u_bars"), 0);
    glUseProgram (0);

    /* Bar geometry, uploaded once */
    glGenVertexArrays (1, & s_gl.vao);
    glBindVertexArray (s_gl.vao);

    glGenBuffers (1, & s_gl.vbo);
    glBindBuffer (GL_ARRAY_BUFFER, s_gl.vbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof bar_vertices, bar_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray (ATTRIB_POSITION);
    glVertexAttribPointer (ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE,
     sizeof bar_vertices[0], (void *) 0);
    glEnableVertexAttribArray (ATTRIB_SHADE);
    glVertexAttribPointer (ATTRIB_SHADE, 1, GL_FLOAT, GL_FALSE,
     sizeof bar_vertices[0], (void *) (3 * sizeof (float)));

    glBindVertexArray (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    /* Height map, updated every frame; R32F can't be filtered on GLES, but
     * texelFetch() doesn't filter anyway, so NEAREST is the right choice */
    glGenTextures (1, & s_gl.texture);
    glBindTexture (GL_TEXTURE_2D, s_gl.texture);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_R32F, NUM_BANDS, NUM_BANDS, 0,
     GL_RED, GL_FLOAT, s_bars);
    glBindTexture (GL_TEXTURE_2D, 0);

    return true;
}

static void free_resources ()
{
    glDeleteTextures (1, & s_gl.texture);
    glDeleteBuffers (1, & s_gl.vbo);
    glDeleteVertexArrays (1, & s_gl.vao);
    glDeleteProgram (s_gl.program);

    s_gl = GLResources ();
}

static void realize_cb (GtkGLArea * area)
{
    gtk_gl_area_make_current (area);

    /* GtkGLArea shows the error in place of the rendering, just log it here */
    if (GError * error = gtk_gl_area_get_error (area))
    {
        AUDERR ("Cannot create OpenGL context: %s\n", error->message);
        return;
    }

    bool use_es = gdk_gl_context_get_use_es (gtk_gl_area_get_context (area));
    int version = epoxy_gl_version ();
    int required = use_es ? 30 : 32;

    GError * error = nullptr;

    if (version >= required)
        create_resources (use_es, & error);
    else
        g_set_error (& error, GDK_GL_ERROR, GDK_GL_ERROR_UNSUPPORTED_PROFILE,
         "OpenGL%s %d.%d or newer is required, but only %d.%d is available.",
         use_es ? " ES" : "", required / 10, required % 10, version / 10, version % 10);

    if (error)
    {
        AUDERR ("%s\n", error->message);
        gtk_gl_area_set_error (area, error); // copies the error
        g_error_free (error);
        free_resources ();
        return;
    }

    AUDDBG ("Using OpenGL%s %d.%d, renderer: %s\n", use_es ? " ES" : "",
     version / 10, version % 10, (const char *) glGetString (GL_RENDERER));
}

static void unrealize_cb (GtkGLArea * area)
{
    gtk_gl_area_make_current (area);

    if (! gtk_gl_area_get_error (area))
        free_resources ();
    else
        s_gl = GLResources ();
}

static gboolean render_cb (GtkGLArea * area, GdkGLContext * context)
{
    glClearColor (0, 0, 0, 1);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (! s_gl.program)
        return true;

    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LESS);

    glUseProgram (s_gl.program);
    glBindVertexArray (s_gl.vao);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, s_gl.texture);
    glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, NUM_BANDS, NUM_BANDS,
     GL_RED, GL_FLOAT, s_bars);

    Mat4 mvp = Mat4::frustum (-1.1f, 1, -1.5f, 1, 2, 10) *
               Mat4::translate (0, -0.5f, -5) *
               Mat4::rotate_x (38) *
               Mat4::rotate_y (s_angle + 180);

    glUniformMatrix4fv (s_gl.u_mvp, 1, GL_FALSE, mvp.m);
    glUniform1i (s_gl.u_pos, s_pos);

    glDrawArraysInstanced (GL_TRIANGLES, 0, BAR_VERTEX_COUNT, NUM_BANDS * NUM_BANDS);

    glBindTexture (GL_TEXTURE_2D, 0);
    glBindVertexArray (0);
    glUseProgram (0);

    return true;
}

void * GLSpectrum::get_gtk_widget ()
{
    if (s_widget)
        return s_widget;

    s_widget = gtk_gl_area_new ();

    gtk_gl_area_set_auto_render ((GtkGLArea *) s_widget, false);
    gtk_gl_area_set_has_depth_buffer ((GtkGLArea *) s_widget, true);

    g_signal_connect (s_widget, "realize", (GCallback) realize_cb, nullptr);
    g_signal_connect (s_widget, "unrealize", (GCallback) unrealize_cb, nullptr);
    g_signal_connect (s_widget, "render", (GCallback) render_cb, nullptr);
    g_signal_connect (s_widget, "destroy", (GCallback) gtk_widget_destroyed, & s_widget);

    return s_widget;
}
