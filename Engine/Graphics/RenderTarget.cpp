#include <GL/glew.h>
#include "RenderTarget.h"
#include <iostream>

bool RenderTarget::resize(int w, int h, ColorFormat fmt)
{
    if (fbo && w == width && h == height && fmt == format)
        return true;
    if (fbo && fmt != format)
        destroy();
    width = w;
    height = h;
    format = fmt;

    if (!fbo)
    {
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &depth);
        if (format != ColorFormat::None)
            glGenTextures(1, &color);
    }

    if (format != ColorFormat::None)
    {
        glBindTexture(GL_TEXTURE_2D, color);
        if (format == ColorFormat::RGBA16F)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(GL_TEXTURE_2D, depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (format != ColorFormat::None)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        GLenum db = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &db);
    }
    else
    {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!complete)
        std::cerr << "[RenderTarget] Framebuffer incomplet (" << w << "x" << h << ")" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return complete;
}

void RenderTarget::destroy()
{
    if (color)
        glDeleteTextures(1, &color);
    if (depth)
        glDeleteTextures(1, &depth);
    if (fbo)
        glDeleteFramebuffers(1, &fbo);
    fbo = color = depth = 0;
    width = height = 0;
}

void RenderTarget::bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
}

void RenderTarget::bindDefault()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::blitDepthTo(const RenderTarget &target) const
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.fbo);
    glBlitFramebuffer(0, 0, width, height, 0, 0, target.width, target.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}
