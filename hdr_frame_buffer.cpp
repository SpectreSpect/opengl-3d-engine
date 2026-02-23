#include "hdr_frame_buffer.h"

void HDRFrameBuffer::init(int width, int height) {
    this->width = width;
    this->height = height;

    glGenFramebuffers(1, &fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);

    glGenTextures(1, &color_texture_id);
    glBindTexture(GL_TEXTURE_2D, color_texture_id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture_id, 0);

    glGenTextures(1, &bright_texture_id);
    glBindTexture(GL_TEXTURE_2D, bright_texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, bright_texture_id, 0);

    GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);

    glGenRenderbuffers(1, &depth_render_buffer_id);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_render_buffer_id);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_render_buffer_id);

    check_frame_buffer("HDRFBO");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::vector<float> quad_vertices = {
        // pos      // tex
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
        1.0f, -1.0f,  1.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f,
    };

    std::vector<unsigned int> quad_indices = {
        0, 1, 2,
        0, 2, 3
    };

    VertexLayout* vertex_layout = new VertexLayout();
    vertex_layout->add("aPos", 0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0, 0, {0.0f, 0.0f});
    vertex_layout->add("aTexCoords", 1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 2 * sizeof(float), 0, {0.0f, 0.0f});
    
    render_quad = new Mesh(quad_vertices, quad_indices, vertex_layout);
}

void HDRFrameBuffer::init_ping_pong(int width, int height) {
    ping_width = width;
    ping_height = height;

    glGenFramebuffers(2, pingpong_fbo);
    glGenTextures(2, pingpong_tex);

    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo[i]);
        glBindTexture(GL_TEXTURE_2D, pingpong_tex[i]);
        // use GL_RGBA16F to keep HDR precision
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, ping_width, ping_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpong_tex[i], 0);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Pingpong FBO not complete: 0x" << std::hex << status << std::dec << "\n";
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void HDRFrameBuffer::check_frame_buffer(const char *name) {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer not complete (" << name << "): 0x" << std::hex << status << std::dec << "\n";
    }
}

// void renderQuad()
// {
//     static GLuint quadVAO = 0;
//     static GLuint quadVBO;

//     if (quadVAO == 0)
//     {
//         float quadVertices[] = {
//             // positions   // texCoords
//             -1.0f,  1.0f,  0.0f, 1.0f,
//             -1.0f, -1.0f,  0.0f, 0.0f,
//              1.0f,  1.0f,  1.0f, 1.0f,
//              1.0f, -1.0f,  1.0f, 0.0f,
//         };

//         glGenVertexArrays(1, &quadVAO);
//         glGenBuffers(1, &quadVBO);

//         glBindVertexArray(quadVAO);
//         glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
//         glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

//         glEnableVertexAttribArray(0);
//         glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

//         glEnableVertexAttribArray(1);
//         glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
//     }

//     glBindVertexArray(quadVAO);
//     glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//     glBindVertexArray(0);
// }

void HDRFrameBuffer::blur(VfProgram* blur_program) {
    bool horizontal = true;
    bool first_iteration = true;
    int blur_amount = 12;

    blur_program->use();
    blur_program->set_int("image", 0);
    
    for (int i = 0; i < blur_amount; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo[horizontal]);

        blur_program->set_bool("horizontal", horizontal);

        // glUniform1i(glGetUniformLocation(blurShader, "horizontal"), horizontal);

        glActiveTexture(GL_TEXTURE0);

        if (first_iteration)
            glBindTexture(GL_TEXTURE_2D, bright_texture_id);
        else
            glBindTexture(GL_TEXTURE_2D, pingpong_tex[!horizontal]);

        // renderQuad(); // fullscreen quad
        render_quad->draw_raw();

        horizontal = !horizontal;

        if (first_iteration)
            first_iteration = false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void HDRFrameBuffer::render_final(VfProgram* final_program)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);

    final_program->use();

    final_program->set_int("scene", 0);
    final_program->set_int("bloomBlur", 1);
    final_program->set_bool("bloom", true);
    final_program->set_float("exposure", 1.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_texture_id);

    glActiveTexture(GL_TEXTURE1);
    // glBindTexture(GL_TEXTURE_2D, pingpong_tex[0]); // last blurred texture
    glBindTexture(GL_TEXTURE_2D, pingpong_tex[0]); // last blurred texture
    

    render_quad->draw_raw();

    glEnable(GL_DEPTH_TEST);
}

void HDRFrameBuffer::bind() {
    // // on HDR FBO:
    // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture_id, 0);
    // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, bright_texture_id, 0);

    // // tell GL which attachments the fragment shader will write to:
    // GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    // glDrawBuffers(2, drawBuffers);


    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);  // 🔥 YOU FORGOT THIS

    GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);
}

void HDRFrameBuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
}