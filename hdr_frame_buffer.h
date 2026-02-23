#pragma once
#include <GL/glew.h>
#include <iostream>
#include "vf_program.h"
#include "mesh.h"
#include "vertex_layout.h"

class HDRFrameBuffer {
public:
    int width = 0;
    int height = 0;
    GLuint fbo_id = 0;
    GLuint color_texture_id = 0;
    GLuint bright_texture_id = 0;
    GLuint depth_render_buffer_id = 0;
    GLuint pingpong_fbo[2] = {0,0};
    GLuint pingpong_tex[2] = {0,0};
    int ping_width = 0;
    int ping_height = 0;

    Mesh* render_quad;

    HDRFrameBuffer() = default;
    void init(int width, int height);
    void init_ping_pong(int width, int height);
    void blur(VfProgram* blur_program);
    void render_final(VfProgram* final_program);
    void bind();
    void unbind();
    static void check_frame_buffer(const char *name);
};