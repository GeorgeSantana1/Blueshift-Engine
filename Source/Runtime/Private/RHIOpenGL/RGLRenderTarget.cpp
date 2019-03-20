// Copyright(c) 2017 POLYGONTEK
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http ://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Precompiled.h"
#include "RHI/RHIOpenGL.h"
#include "RGLInternal.h"

BE_NAMESPACE_BEGIN

static bool CheckFBOStatus() {
    GLenum status = gglCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE) {
        return true;
    }
    
    switch (status) {
    case GL_FRAMEBUFFER_UNSUPPORTED:
        BE_WARNLOG("FBO format unsupported\n");
        return false;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        BE_WARNLOG("FBO missing an image attachment\n");
        return false;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        BE_WARNLOG("FBO has one or several incomplete image attachments\n");
        return false;
#ifdef GL_EXT_framebuffer_object
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
        BE_WARNLOG("FBO has one or several image attachments with different dimensions\n");
        return false;
    case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
        BE_WARNLOG("FBO has one or several image attachments with different internal formats\n");
        return false;
#endif
#ifdef GL_VERSION_3_0
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        BE_WARNLOG("FBO has invalid draw buffer\n");
        return false;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        BE_WARNLOG("FBO has invalid read buffer\n");
        return false;
#endif
#ifdef GL_VERSION_3_2
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        BE_WARNLOG("FBO missing layer target\n");
        return false;
#endif
#ifdef GL_EXT_geometry_shader4
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_COUNT_EXT:
        BE_WARNLOG("FBO incomplete layer count\n");
        return false;
#endif
    }

    BE_WARNLOG("FBO unknown status %i\n", status);
    return false;
}

RHI::Handle OpenGLRHI::CreateRenderTarget(RenderTargetType::Enum type, int width, int height, int numColorTextures, Handle *colorTextureHandles, Handle depthTextureHandle, int flags) {
    GLuint fbo;
    GLuint colorRenderBuffer = 0;
    GLuint depthRenderBuffer = 0;
    GLuint stencilRenderBuffer = 0;

    GLint oldFBO;
    gglGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);

    gglGenFramebuffers(1, &fbo);
    gglBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLenum target = 0;
    switch (type) {
    case RenderTargetType::RT2D:
        target = GL_TEXTURE_2D;
        break;
    case RenderTargetType::RTCubeMap:
        target = GL_TEXTURE_CUBE_MAP;
        break;
    case RenderTargetType::RT2DArray:
        target = GL_TEXTURE_2D_ARRAY;
        break;
    default:
        assert(0);
        break;
    }

    // numColorTextures may be greater than 1 due to MRT.
    if (numColorTextures > 0) {
        for (int i = 0; i < numColorTextures; i++) {
            GLuint textureObject = textureList[colorTextureHandles[i]]->object;
            
            switch (target) {
            case GL_TEXTURE_2D:
                gglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, target, textureObject, 0);
                break;
            case GL_TEXTURE_3D:
            case GL_TEXTURE_2D_ARRAY:
                gglFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, textureObject, 0, 0);
                break;
            case GL_TEXTURE_CUBE_MAP:
                gglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_CUBE_MAP_POSITIVE_X, textureObject, 0);
                break;
            case GL_TEXTURE_CUBE_MAP_ARRAY:
            default:
                assert(0);
                break;
            }
        }

        OpenGL::DrawBuffer(GL_COLOR_ATTACHMENT0);
        OpenGL::ReadBuffer(GL_COLOR_ATTACHMENT0);
    } else if (flags & RenderTargetFlag::HasColorBuffer) {
        gglGenRenderbuffers(1, &colorRenderBuffer);
        gglBindRenderbuffer(GL_RENDERBUFFER, colorRenderBuffer);
        gglRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
        gglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRenderBuffer);
    }

    if (depthTextureHandle != NullTexture) {
        GLuint textureObject = textureList[depthTextureHandle]->object;
        
        switch (target) {
        case GL_TEXTURE_2D:
            gglFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, textureObject, 0);
            break;
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
            gglFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureObject, 0, 0);
            break;
        case GL_TEXTURE_CUBE_MAP:
            gglFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, textureObject, 0);
            break;
        case GL_TEXTURE_CUBE_MAP_ARRAY:
        default:
            assert(0);
            break;
        }

        if (numColorTextures == 0) {
            // NOTE: this is per-FBO state
            OpenGL::DrawBuffer(GL_NONE);
            OpenGL::ReadBuffer(GL_NONE);
        }
    } else if (flags & RenderTargetFlag::HasDepthBuffer) {
        if (flags & RenderTargetFlag::HasStencilBuffer) {
            gglGenRenderbuffers(1, &depthRenderBuffer);
            gglBindRenderbuffer(GL_RENDERBUFFER, depthRenderBuffer);
            gglRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            gglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderBuffer);
            gglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRenderBuffer);
        } else {
            gglGenRenderbuffers(1, &depthRenderBuffer);
            gglBindRenderbuffer(GL_RENDERBUFFER, depthRenderBuffer);
            gglRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
            gglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderBuffer);
        }
    }

    if (!CheckFBOStatus()) {
        if (depthRenderBuffer) {
            gglDeleteRenderbuffers(1, &depthRenderBuffer);
        }

        if (stencilRenderBuffer) {
            gglDeleteRenderbuffers(1, &stencilRenderBuffer);
        }

        if (fbo) {
            gglDeleteFramebuffers(1, &fbo);
        }

        return NullRenderTarget;
    }

    gglBindFramebuffer(GL_FRAMEBUFFER, oldFBO);

    GLRenderTarget *renderTarget = new GLRenderTarget;
    renderTarget->type = type;
    renderTarget->flags = flags;
    renderTarget->numColorTextures = numColorTextures;
    for (int i = 0; i < numColorTextures; i++) {
        renderTarget->colorTextureHandles[i] = colorTextureHandles[i];
    }
    renderTarget->depthTextureHandle = depthTextureHandle;
    renderTarget->fbo = fbo;
    renderTarget->depthRenderBuffer = depthRenderBuffer;
    renderTarget->stencilRenderBuffer = stencilRenderBuffer;

    int handle = renderTargetList.FindNull();
    if (handle == -1) {
        handle = renderTargetList.Append(renderTarget);
    } else {
        renderTargetList[handle] = renderTarget;
    }

    return (Handle)handle;
}

void OpenGLRHI::DestroyRenderTarget(Handle renderTargetHandle) {
    if (renderTargetHandle == NullRenderTarget) {
        BE_WARNLOG("OpenGLRHI::DestroyRenderTarget: invalid render target\n");
        return;
    }
    
    if (currentContext->state->renderTargetHandleStackDepth > 0 && 
        currentContext->state->renderTargetHandleStack[currentContext->state->renderTargetHandleStackDepth - 1] == renderTargetHandle) {
        BE_WARNLOG("OpenGLRHI::DestroyRenderTarget: render target is using\n");
        return;
    }

    GLRenderTarget *renderTarget = renderTargetList[renderTargetHandle];
    if (renderTarget->depthRenderBuffer) {
        gglDeleteRenderbuffers(1, &renderTarget->depthRenderBuffer);
    }
    if (!renderTarget->stencilRenderBuffer) {
        gglDeleteRenderbuffers(1, &renderTarget->stencilRenderBuffer);
    }
    if (renderTarget->fbo) {
        gglDeleteFramebuffers(1, &renderTarget->fbo);
    }

    delete renderTarget;
    renderTargetList[renderTargetHandle] = nullptr;
}

void OpenGLRHI::BeginRenderTarget(Handle renderTargetHandle, int level, int sliceIndex) {
    if (currentContext->state->renderTargetHandleStackDepth > 0 && currentContext->state->renderTargetHandleStack[currentContext->state->renderTargetHandleStackDepth - 1] == renderTargetHandle) {
        BE_WARNLOG("OpenGLRHI::BeginRenderTarget: same render target\n");
    }

    const GLRenderTarget *renderTarget = renderTargetList[renderTargetHandle];
    currentContext->state->renderTargetHandleStack[currentContext->state->renderTargetHandleStackDepth++] = currentContext->state->renderTargetHandle;
    currentContext->state->renderTargetHandle = renderTargetHandle;

    gglBindFramebuffer(GL_FRAMEBUFFER, renderTarget->fbo);

    if (renderTarget->type == RenderTargetType::RT2DArray) {
        if (renderTarget->numColorTextures > 0) {
            for (int i = 0; i < renderTarget->numColorTextures; i++) {
                GLuint textureObject = textureList[renderTarget->colorTextureHandles[i]]->object;
                gglFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, textureObject, level, sliceIndex);
            }
        }

        if (renderTarget->depthTextureHandle != NullTexture) {
            GLuint textureObject = textureList[renderTarget->depthTextureHandle]->object;
            gglFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureObject, level, sliceIndex);
        }
    } else {
        GLenum target = renderTarget->type == RenderTargetType::RTCubeMap ? (GL_TEXTURE_CUBE_MAP_POSITIVE_X + sliceIndex) : GL_TEXTURE_2D;

        if (renderTarget->numColorTextures > 0) {
            for (int i = 0; i < renderTarget->numColorTextures; i++) {
                GLuint textureObject = textureList[renderTarget->colorTextureHandles[i]]->object;
                gglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, target, textureObject, level);
            }
        }

        if (renderTarget->depthTextureHandle != NullTexture) {
            GLuint textureObject = textureList[renderTarget->depthTextureHandle]->object;
            gglFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, textureObject, level);
        }
    } 

    if (gl_sRGB.GetBool()) {
        SetSRGBWrite(!!(renderTarget->flags & RenderTargetFlag::SRGBWrite));
    }
}

void OpenGLRHI::EndRenderTarget() {
    if (currentContext->state->renderTargetHandleStackDepth == 0) {
        BE_WARNLOG("unmatched BeginRenderTarget() / EndRenderTarget()\n");
        return;
    }
    
    Handle oldRenderTargetHandle = currentContext->state->renderTargetHandleStack[--currentContext->state->renderTargetHandleStackDepth];
    const GLRenderTarget *oldRenderTarget = renderTargetList[oldRenderTargetHandle];

    currentContext->state->renderTargetHandle = oldRenderTargetHandle;

    gglBindFramebuffer(GL_FRAMEBUFFER, oldRenderTarget->fbo);

    if (gl_sRGB.GetBool()) {
        SetSRGBWrite(!!(oldRenderTarget->flags & RenderTargetFlag::SRGBWrite));
    }
}

void OpenGLRHI::SetDrawBuffersMask(unsigned int colorBufferBitMask) {
    if (colorBufferBitMask > 0) {
        GLenum drawBuffers[16];
        int numDrawBuffers = 0;

        while (colorBufferBitMask) {
            if (colorBufferBitMask & 1) {
                drawBuffers[numDrawBuffers] = GL_COLOR_ATTACHMENT0 + numDrawBuffers;
                numDrawBuffers++;
            }
            colorBufferBitMask >>= 1;
        }

        OpenGL::DrawBuffers(numDrawBuffers, drawBuffers);
    } else {
        OpenGL::DrawBuffer(GL_NONE);
    }
}

void OpenGLRHI::DiscardRenderTarget(bool depth, bool stencil, uint32_t colorBitMask) {
    if (!OpenGL::SupportsDiscardFrameBuffer()) {
        return;
    }

    GLenum attachments[10];
    int numAttachments = 0;

    int colorIndex = 0;
    while (colorBitMask) {
        if (colorBitMask & 1) {
            attachments[numAttachments++] = GL_COLOR_ATTACHMENT0 + colorIndex;
        }

        colorBitMask >>= 1;
        colorIndex++;
    }
    if (depth) {
        attachments[numAttachments++] = GL_DEPTH_ATTACHMENT;
    }
    if (stencil) {
        attachments[numAttachments++] = GL_STENCIL_ATTACHMENT;
    }

    OpenGL::DiscardFramebuffer(GL_FRAMEBUFFER, numAttachments, attachments);
}

void OpenGLRHI::BlitRenderTarget(Handle srcRenderTargetHandle, const Rect &srcRect, Handle dstRenderTargetHandle, const Rect &dstRect, int mask, BlitFilter::Enum filter) const {
    const GLRenderTarget *srcRenderTarget = renderTargetList[srcRenderTargetHandle];
    const GLRenderTarget *dstRenderTarget = renderTargetList[dstRenderTargetHandle];

    GLbitfield glmask = 0;
    if (mask & BlitMask::Color) {
        glmask |= GL_COLOR_BUFFER_BIT;
    }

    if (mask & BlitMask::Depth) {
        glmask |= GL_DEPTH_BUFFER_BIT;
    }

    assert(glmask);
    if (!glmask) {
        BE_WARNLOG("OpenGLRHI::BlitRenderTarget: NULL mask\n");
        return;
    }

    gglBindFramebuffer(GL_READ_FRAMEBUFFER, srcRenderTarget->fbo);
    gglBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstRenderTarget->fbo);

    gglBlitFramebuffer(
        srcRect.x, srcRect.y, srcRect.X2(), srcRect.Y2(),
        dstRect.x, dstRect.y, dstRect.X2(), dstRect.Y2(),
        glmask, filter == BlitFilter::Nearest ? GL_NEAREST : GL_LINEAR);

    gglBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gglBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

BE_NAMESPACE_END
