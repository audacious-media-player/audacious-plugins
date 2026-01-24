#ifndef MODERN_RENDER_H
#define MODERN_RENDER_H

#include <vector>
#include <glm/mat4x4.hpp>

struct triangle;

class ModernRenderer
{
public:
    const int NUM_BANDS;
    const float BAR_SPACING;
    const float BAR_WIDTH;

    explicit ModernRenderer
    (
        int num_bands,
        float bar_spacing,
        float bar_width,
        const float * colors
    );
    ~ModernRenderer();

    void render(const float * s_bars, int s_pos, float s_angle);
    bool is_initialized() const;

private:
    unsigned int vao = 0;
    unsigned int program = 0;
    unsigned int values_tex = 0;

    unsigned int mvp_pointer = 0;
    unsigned int s_pos_pointer = 0;

    glm::mat4 projection;

    int triangle_count = 0;

    bool initialized = false;

    void add_bars (std::vector<triangle> * triangle_buffer,
                   const float * colors);
    void update_vbo (const triangle * triangle_buffer, int triangle_count);
    bool init_shaders ();
};

#endif // MODERN_RENDER_H
