#ifndef H1_INDEXED_DRAW_TEST_H
#define H1_INDEXED_DRAW_TEST_H

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#include <stdint.h>
#include <stdio.h>

static void h1_test_indexed_draw(const uint8_t vertices[64],
                                 const uint32_t indices[6])
{
    for (unsigned i = 0; i < 6; ++i) {
        if (indices[i] >= 4) {
            fprintf(stderr, "[h1-gles-draw] invalid_index\n");
            return;
        }
    }

    static const char *vs_source =
        "#version 300 es\n"
        "layout(location=0) in vec2 pos;\n"
        "void main(){gl_Position=vec4(pos,0.0,1.0);}\n";

    static const char *fs_source =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 color;\n"
        "void main(){color=vec4(0.2,0.8,0.4,1.0);}\n";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, &vs_source, NULL);
    glShaderSource(fs, 1, &fs_source, NULL);
    glCompileShader(vs);
    glCompileShader(fs);

    GLint vs_ok = 0, fs_ok = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &vs_ok);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &fs_ok);

    if (!vs_ok || !fs_ok) {
        fprintf(stderr, "[h1-gles-draw] shader_compile_failed\n");
        glDeleteShader(vs);
        glDeleteShader(fs);
        return;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if (!linked) {
        fprintf(stderr, "[h1-gles-draw] program_link_failed\n");
        glDeleteProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return;
    }

    GLint old_fbo = 0, old_program = 0, old_vao = 0;
    GLint old_array = 0, old_viewport[4] = {0};
    GLint old_texture = 0, old_active_texture = 0;
    GLint old_pack = 4;
    GLfloat old_clear[4] = {0};

    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend = glIsEnabled(GL_BLEND);
    GLboolean cull = glIsEnabled(GL_CULL_FACE);

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_array);
    glGetIntegerv(GL_VIEWPORT, old_viewport);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active_texture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
    glGetIntegerv(GL_PACK_ALIGNMENT, &old_pack);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, old_clear);

    GLuint vao = 0, vbo = 0, ebo = 0, tex = 0, fbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glGenTextures(1, &tex);
    glGenFramebuffers(1, &fbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 64, vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 24, indices, GL_STATIC_DRAW);

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE) {
        glViewport(0, 0, 32, 32);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glUseProgram(program);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        uint8_t pixel[4] = {0};
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(16, 16, 1, 1, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixel);
        GLenum error = glGetError();

        fprintf(stderr,
                "[h1-gles-draw] pixel=%u,%u,%u,%u error=0x%x result=%s\n",
                pixel[0], pixel[1], pixel[2], pixel[3],
                (unsigned)error,
                error == GL_NO_ERROR && pixel[1] > 150 &&
                pixel[0] < 100 ? "PASS" : "FAIL");
    } else {
        fprintf(stderr, "[h1-gles-draw] fbo_status=0x%x FAIL\n",
                (unsigned)status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)old_fbo);
    glBindVertexArray((GLuint)old_vao);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)old_array);
    glUseProgram((GLuint)old_program);
    glActiveTexture((GLenum)old_active_texture);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture);
    glViewport(old_viewport[0], old_viewport[1],
               old_viewport[2], old_viewport[3]);
    glPixelStorei(GL_PACK_ALIGNMENT, old_pack);
    glClearColor(old_clear[0], old_clear[1],
                 old_clear[2], old_clear[3]);

    if (scissor) glEnable(GL_SCISSOR_TEST);
    else glDisable(GL_SCISSOR_TEST);
    if (depth) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    if (blend) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    if (cull) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
}
#endif
#endif