#include "modern_renderer.h"

/* We don't want this to be built without USE_MODERNGL */
#ifdef USE_MODERNGL

#include <vector>

#include <glib.h>

#include <libaudcore/runtime.h>

#include <epoxy/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const char* vertex_shader_src =
R"(#version ${version_string}

in vec3 position;
in vec3 color;
in int id;

uniform int s_pos;
uniform mat4 mvp;
uniform sampler2D s_bars;

out vec4 vertexColor;

void main()
{
    ivec2 texture_size = textureSize (s_bars, 0);

    vec2 uv = vec2
    (
        mod (float (id), ${num_bands}.0) + 0.5,
        mod (float ((id / ${num_bands}) + s_pos), ${num_bands}.0) + 0.5
    );

    float height = texture (s_bars, uv / ${num_bands}.0).r * 1.6;

    gl_Position = mvp * vec4 (position.x, position.y * height, position.z, 1.0);
    vertexColor = vec4 (color * (0.2 + 0.8 * height), 1.0);
}
)";

const char* fragment_shader_src =
R"(#version ${version_string}

precision mediump float;
in vec4 vertexColor;

out vec4 outputColor;

void main() {
    outputColor = vertexColor;
}
)";

struct vertex
{
    GLfloat position[3];
    GLfloat color[3];

    /* If -1 the Y coordinate will be 0, otherwise it'll be used to index the
     * texture */
    GLint id;
};

struct triangle
{
    vertex position[3];
};

static GLuint
create_shader (int shader_type, const char * source, GLuint * shader_out)
{
    GLuint shader = glCreateShader (shader_type);
    glShaderSource (shader, 1, &source, NULL);
    glCompileShader (shader);

    int status;
    glGetShaderiv (shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        int log_len;
        glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &log_len);

        char *buffer = new char[log_len + 1];
        glGetShaderInfoLog (shader, log_len, NULL, buffer);

        AUDERR
        (
            "%s shader compilation error:%s\n",
            (shader_type == GL_VERTEX_SHADER ? "Vertex" : "Fragment"),
            buffer
        );

        delete [] buffer;

        glDeleteShader (shader);
        shader = 0;
    }

    if (shader_out != NULL)
        *shader_out = shader;

    return shader != 0;
}

bool ModernRenderer::init_shaders ()
{
    GLuint program = 0;
    GLuint vertex = 0;
    GLuint fragment = 0;

    bool es = !epoxy_is_desktop_gl ();

    const gchar * version_str = es ? "300 es" : "130";

    AUDINFO
    (
       "Detected OpenGL type: %s, using GLSL version string: %s\n",
       es ? "GLES" : "GL",
       version_str
    );

    GString * vertex_shader_string = g_string_new (vertex_shader_src);

    g_string_replace (vertex_shader_string,
                      "${version_string}",
                      version_str,
                      0);

    g_string_replace (vertex_shader_string,
                      "${num_bands}",
                      g_strdup_printf("%i", NUM_BANDS),
                      0);

    create_shader (GL_VERTEX_SHADER, vertex_shader_string->str, &vertex);

    g_free(vertex_shader_string);

    GString * fragment_shader_string = g_string_new (fragment_shader_src);

    g_string_replace(fragment_shader_string,
                     "${version_string}",
                     version_str,
                     0);

    create_shader (GL_FRAGMENT_SHADER, fragment_shader_string->str, &fragment);

    g_free(fragment_shader_string);

    GLuint mvp_pointer = 0;
    GLuint s_pos_pointer = 0;

    if (vertex != 0 && fragment != 0)
    {
        /* link the vertex and fragment shaders together */
        program = glCreateProgram ();
        glAttachShader (program, vertex);
        glAttachShader (program, fragment);
        glLinkProgram (program);

        int status = GL_FALSE;

        glGetProgramiv (program, GL_LINK_STATUS, &status);

        if (status)
        {
            mvp_pointer = glGetUniformLocation (program, "mvp");
            s_pos_pointer = glGetUniformLocation (program, "s_pos");

            /* the individual shaders can be detached and destroyed */
            glDetachShader (program, vertex);
            glDetachShader (program, fragment);
        }
        else
        {
            int log_len = 0;
            glGetProgramiv (program, GL_INFO_LOG_LENGTH, &log_len);

            char *buffer = new char[log_len + 1];
            glGetProgramInfoLog (program, log_len, NULL, buffer);

            AUDERR ("Linking failure in program: %s\n", buffer);

            delete [] buffer;

            glDeleteProgram (program);
            program = 0;
        }
    }

    if (vertex != 0)
        glDeleteShader (vertex);

    if (fragment != 0)
        glDeleteShader (fragment);

    if (program != 0)
    {
        ModernRenderer::program = program;
        ModernRenderer::mvp_pointer = mvp_pointer;
        ModernRenderer::s_pos_pointer = s_pos_pointer;
    }

    return program != 0;
}

void
ModernRenderer::update_vbo (const triangle * triangle_buffer,
                            int triangle_count)
{
    GLuint position_pointer = glGetAttribLocation (program, "position");
    GLuint color_pointer    = glGetAttribLocation (program, "color");
    GLuint id_pointer       = glGetAttribLocation (program, "id");

    GLuint vbo;

    glGenBuffers (1, &vbo);

    glBindVertexArray (vao);

    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData
    (
        GL_ARRAY_BUFFER,
        sizeof (struct triangle) * triangle_count,
        triangle_buffer,
        GL_STATIC_DRAW
    );

    /* enable and set the position attribute */
    glEnableVertexAttribArray (position_pointer);
    glVertexAttribPointer
    (
        position_pointer,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof (struct vertex),
        (GLvoid *) (G_STRUCT_OFFSET (struct vertex, position))
    );

    /* enable and set the color attribute */
    glEnableVertexAttribArray (color_pointer);
    glVertexAttribPointer (
        color_pointer,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof (struct vertex),
        (GLvoid *) (G_STRUCT_OFFSET (struct vertex, color))
    );

    /* enable and set the id attribute */
    glEnableVertexAttribArray (id_pointer);
    glVertexAttribIPointer (
    id_pointer,
        1,
        GL_INT,
        sizeof (struct vertex),
        (GLvoid *) (G_STRUCT_OFFSET (struct vertex, id))
    );

    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindVertexArray (0);

    /* the VBO is referenced by the VAO */
    glDeleteBuffers (1, &vbo);
}

static void add_rectangle (std::vector<triangle> * triangle_buffer, int id,
                           float x1, float z1, float x2, float z2,
                           float r, float g, float b)
{
    /* All triangle vertices should be aligned counter-clockwise */

    /* height multiplier */
    float h_mu = 1.0f;

    /* Top */
    triangle_buffer->push_back
    ({
        vertex{ { x2, h_mu, z2 }, { r, g, b }, id },
        vertex{ { x2, h_mu, z1 }, { r, g, b }, id },
        vertex{ { x1, h_mu, z1 }, { r, g, b }, id },
    });

    triangle_buffer->push_back
    ({
        vertex{ { x1, h_mu, z2 }, { r, g, b }, id },
        vertex{ { x2, h_mu, z2 }, { r, g, b }, id },
        vertex{ { x1, h_mu, z1 }, { r, g, b }, id },
    });

    /* Right */
    triangle_buffer->push_back
    ({
        vertex{ { x1, h_mu, z2 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
        vertex{ { x1, h_mu, z1 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
        vertex{ { x1, 0.0f, z1 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
    });

    triangle_buffer->push_back
    ({
        vertex{ { x1, 0.0f, z2 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
        vertex{ { x1, h_mu, z2 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
        vertex{ { x1, 0.0f, z1 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
    });

    /* Left */
    triangle_buffer->push_back
    ({
        vertex{ { x2, 0.0f, z2 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
        vertex{ { x2, 0.0f, z1 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
        vertex{ { x2, h_mu, z1 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
    });

    triangle_buffer->push_back
    ({
        vertex{ { x2, h_mu, z2 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
        vertex{ { x2, 0.0f, z2 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
        vertex{ { x2, h_mu, z1 }, { 0.65f * r, 0.65f * g, 0.65f * b }, id },
    });

    /* Front */
    triangle_buffer->push_back
    ({
        vertex{ { x2, h_mu, z1 }, { 0.8f * r, 0.8f * g, 0.8f * b }, id },
        vertex{ { x2, 0.0f, z1 }, { 0.8f * r, 0.8f * g, 0.8f * b }, id },
        vertex{ { x1, 0.0f, z1 }, { 0.8f * r, 0.8f * g, 0.8f * b }, id },
    });

    triangle_buffer->push_back
    ({
        vertex{ { x1, h_mu, z1 }, { 0.8f * r, 0.8f * g, 0.8f * b }, id },
        vertex{ { x2, h_mu, z1 }, { 0.8f * r, 0.8f * g, 0.8f * b }, id },
        vertex{ { x1, 0.0f, z1 }, { 0.8f * r, 0.8f * g, 0.8f * b }, id },
    });
}

void
ModernRenderer::add_bars (std::vector<triangle> * triangle_buffer,
                          const float * colors)
{
    for (int i = 0; i < NUM_BANDS; i ++)
    {
        for (int j = 0; j < NUM_BANDS; j ++)
        {
            float x = 1.6f - BAR_SPACING * j;
            float z = -1.6f + (NUM_BANDS - i) * BAR_SPACING;

            int id = (i % NUM_BANDS) * NUM_BANDS + j;

            float r = colors[id * 3 + 0];
            float g = colors[id * 3 + 1];
            float b = colors[id * 3 + 2];

            add_rectangle
            (
                triangle_buffer,
                id,
                x, z,
                x + BAR_WIDTH, z + BAR_WIDTH,
                r, g, b
            );
        }
    }
}

ModernRenderer::ModernRenderer (
    int num_bands,
    float bar_spacing,
    float bar_width,
    const float * colors
) : NUM_BANDS (num_bands),
    BAR_SPACING (bar_spacing),
    BAR_WIDTH (bar_width),
    projection (glm::frustum (-1.1f, 1.0f, -1.5f, 1.0f, 2.0f, 10.0f))
{
    int gl_version = epoxy_gl_version();
    bool has_ext = epoxy_has_gl_extension ("GL_ARB_vertex_array_object");

    AUDINFO
    (
        "OpenGL version %d, %s GL_ARB_vertex_array_object.\n",
        gl_version,
        has_ext ? "has" : "doesn't have"
    );

    if (gl_version < 21 || !has_ext)
    {
        return;
    }

    if (!init_shaders ())
    {
        return;
    }

    glGenVertexArrays (1, &vao);

    std::vector<triangle> triangle_buffer;

    /* We need upload the geometry data only once */
    add_bars (&triangle_buffer, colors);
    update_vbo (triangle_buffer.data (), triangle_buffer.size ());

    triangle_count = triangle_buffer.size ();

    glGenTextures (1, &values_tex);
    glBindTexture (GL_TEXTURE_2D, values_tex);

    glEnable (GL_TEXTURE);
    glEnable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);

    glClearColor (0, 0, 0, 1);

    initialized = true;
}

void
ModernRenderer::render (const float * s_bars, int s_pos, float s_angle)
{
    glUseProgram (program);

    glm::mat4 model = glm::mat4 (1.0f);
    model = glm::translate (model, glm::vec3 (0.0f, -0.5f, -5.0f));

    model = glm::rotate (model,
        glm::radians (38.0f), glm::vec3 (1.0f, 0.0f, 0.0f));

    model = glm::rotate (model,
        glm::radians (s_angle + 180.0f), glm::vec3 (0.0f, 1.0f, 0.0f));

    glm::mat4 mvp = projection * model;

    glUniformMatrix4fv (mvp_pointer, 1, GL_FALSE, glm::value_ptr (mvp));
    glUniform1i (s_pos_pointer, s_pos);

    /* Upload our array as an OpenGL texture */
    glTexImage2D
    (
        GL_TEXTURE_2D,
        0,
        GL_R32F,
        NUM_BANDS, NUM_BANDS,
        0,
        GL_RED,
        GL_FLOAT,
        s_bars
    );

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

    glBindVertexArray (vao);

    glDrawArrays (GL_TRIANGLES, 0, triangle_count * 3);

    glBindVertexArray (0);
    glUseProgram (0);
}

bool ModernRenderer::is_initialized () const
{
    return initialized;
}

ModernRenderer::~ModernRenderer ()
{
    if (values_tex)
        glDeleteTextures (1, &values_tex);

    if (vao)
        glDeleteVertexArrays (1, &vao);

    if (program)
        glDeleteProgram (program);
}

#endif // USE_MODERNGL
