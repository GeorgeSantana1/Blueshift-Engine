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
#include "Render/Render.h"
#include "RenderInternal.h"
#include "Core/Heap.h"
#include "Platform/PlatformTime.h"

BE_NAMESPACE_BEGIN

RenderContext::RenderContext() {
    deviceWidth = 0;
    deviceHeight = 0;
    renderingWidth = 0;
    renderingHeight = 0;

    frameCount = 0;

    screenSelectionScale = 0.25f;

    prevLumTarget = 1;
    currLumTarget = 2;
}

void RenderContext::Init(RHI::WindowHandle hwnd, int renderingWidth, int renderingHeight, RHI::DisplayContextFunc displayFunc, void *displayFuncDataPtr, int flags) {
    this->contextHandle = rhi.CreateContext(hwnd, (flags & Flag::UseSharedContext) ? true : false);
    this->flags = flags;

    RHI::DisplayMetrics displayMetrics;
    rhi.GetDisplayMetrics(this->contextHandle, &displayMetrics);

    this->windowWidth = displayMetrics.screenWidth;
    this->windowHeight = displayMetrics.screenHeight;
    this->deviceWidth = displayMetrics.backingWidth;
    this->deviceHeight = displayMetrics.backingHeight;
    
    // This is actual rendering resolution that will be upscaled if it is smaller than device resolution
    this->renderingWidth = renderingWidth;
    this->renderingHeight = renderingHeight;
    
    this->guiMesh.Clear();
    this->guiMesh.SetCoordFrame(GuiMesh::CoordFrame2D);
    this->guiMesh.SetClipRect(Rect(0, 0, renderingWidth, renderingHeight));
    
    rhi.SetContextDisplayFunc(contextHandle, displayFunc, displayFuncDataPtr, (flags & Flag::OnDemandDrawing) ? true : false);

    InitScreenMapRT();

    InitHdrMapRT();

    InitShadowMapRT();

    InitEnvCubeMapRT();

    //WriteGGXDFGSum("IntegrationLUT_GGX", 512);
    
    this->random.SetSeed(123123);

    this->elapsedTime = PlatformTime::Milliseconds() * 0.001f;
}

void RenderContext::Shutdown() {
    FreeScreenMapRT();

    FreeHdrMapRT();

    FreeShadowMapRT();

    FreeEnvCubeMapRT();

    rhi.DestroyContext(contextHandle);
}

static Image::Format GetScreenImageFormat() {
    if (r_HDR.GetInteger() == 0) {
        return Image::RGBA_8_8_8_8;
    }
    
    if (r_HDR.GetInteger() == 1 && rhi.SupportsPackedFloat()) {
        return Image::RGB_11F_11F_10F;
    }
    
    if (r_HDR.GetInteger() == 3) {
        return Image::RGBA_32F_32F_32F_32F;
    }

    return Image::RGBA_16F_16F_16F_16F;
}

void RenderContext::InitScreenMapRT() {
    FreeScreenMapRT();

    //--------------------------------------
    // Create screenRT
    //--------------------------------------

    int screenTextureFlags = Texture::Clamp | Texture::NoMipmaps | Texture::HighQuality | Texture::NonPowerOfTwo | Texture::HighPriority;

    Image::Format screenImageFormat = GetScreenImageFormat();

    screenColorTexture = textureManager.AllocTexture(va("_%i_screenColor", (int)contextHandle));
    screenColorTexture->CreateEmpty(RHI::Texture2D, renderingWidth, renderingHeight, 1, 1, 1, screenImageFormat, screenTextureFlags | Texture::SRGBColorSpace);

    screenDepthTexture = textureManager.AllocTexture(va("_%i_screenDepthStencil", (int)contextHandle));
    screenDepthTexture->CreateEmpty(RHI::Texture2D, renderingWidth, renderingHeight, 1, 1, 1, Image::Depth_24, screenTextureFlags | Texture::Nearest);

    if (r_useDeferredLighting.GetBool()) {
        screenNormalTexture = textureManager.AllocTexture(va("_%i_screenNormal", (int)contextHandle));
        screenNormalTexture->CreateEmpty(RHI::Texture2D, renderingWidth, renderingHeight, 1, 1, 1, Image::RG_16F_16F, screenTextureFlags | Texture::Nearest);

        Texture *mrtTextures[2];
        mrtTextures[0] = screenColorTexture;
        mrtTextures[1] = screenNormalTexture;
        screenRT = RenderTarget::Create(2, (const Texture **)mrtTextures, screenDepthTexture, RHI::SRGBWrite);

        screenLitAccTexture = textureManager.AllocTexture(va("_%i_screenLitAcc", (int)contextHandle));
        screenLitAccTexture->CreateEmpty(RHI::Texture2D, renderingWidth, renderingHeight, 1, 1, 1, screenImageFormat, screenTextureFlags | Texture::Nearest | Texture::SRGBColorSpace);
        screenLitAccRT = RenderTarget::Create(screenLitAccTexture, nullptr, RHI::SRGBWrite);
    } else if (r_usePostProcessing.GetBool() && r_SSAO.GetBool()) {
        screenNormalTexture = textureManager.AllocTexture(va("_%i_screenNormal", (int)contextHandle));
        screenNormalTexture->CreateEmpty(RHI::Texture2D, renderingWidth, renderingHeight, 1, 1, 1, Image::RGBA_8_8_8_8, screenTextureFlags | Texture::Nearest);

        Texture *mrtTextures[2];
        mrtTextures[0] = screenColorTexture;
        mrtTextures[1] = screenNormalTexture;
        screenRT = RenderTarget::Create(2, (const Texture **)mrtTextures, screenDepthTexture, RHI::SRGBWrite);
    } else {
        screenRT = RenderTarget::Create(screenColorTexture, screenDepthTexture, RHI::SRGBWrite);
    }

    screenRT->Clear(Color4::black, 1.0f, 0);

    if (flags & UseSelectionBuffer) {
        screenSelectionTexture = textureManager.AllocTexture(va("_%i_screenSelection", (int)contextHandle));
        screenSelectionTexture->CreateEmpty(RHI::Texture2D, renderingWidth * screenSelectionScale, renderingHeight * screenSelectionScale, 1, 1, 1, Image::RGBA_8_8_8_8, screenTextureFlags);
    
        screenSelectionRT = RenderTarget::Create(screenSelectionTexture, nullptr, RHI::HasDepthBuffer);
    }

    if (r_HOM.GetBool()) {
        int w = Math::FloorPowerOfTwo(renderingWidth >> 1);
        int h = Math::FloorPowerOfTwo(renderingHeight >> 1);

        homTexture = textureManager.AllocTexture(va("_%i_hom", (int)contextHandle));
        homTexture->CreateEmpty(RHI::Texture2D, w, h, 1, 1, 1, Image::Depth_32F, Texture::Clamp | Texture::HighQuality | Texture::HighPriority | Texture::Nearest);
        homTexture->Bind();
        rhi.GenerateMipmap();
        homRT = RenderTarget::Create(nullptr, (const Texture *)homTexture, 0);
    }

    //--------------------------------------
    // Create RT for post processing
    //--------------------------------------

    int halfWidth = Math::Ceil(renderingWidth * 0.5f);
    int halfHeight = Math::Ceil(renderingHeight * 0.5f);
    int quarterWidth = Math::Ceil(renderingWidth * 0.25f);
    int quarterHeight = Math::Ceil(renderingHeight * 0.25f);

    ppTextures[PP_TEXTURE_COLOR_2X] = textureManager.AllocTexture(va("_%i_screenColorD2x", (int)contextHandle));
    ppTextures[PP_TEXTURE_COLOR_2X]->CreateEmpty(RHI::Texture2D, halfWidth, halfHeight, 1, 1, 1, screenImageFormat, screenTextureFlags | Texture::SRGBColorSpace);
    ppRTs[PP_RT_2X] = RenderTarget::Create(ppTextures[PP_TEXTURE_COLOR_2X], nullptr, RHI::SRGBWrite);

    ppTextures[PP_TEXTURE_COLOR_4X] = textureManager.AllocTexture(va("_%i_screenColorD4x", (int)contextHandle));
    ppTextures[PP_TEXTURE_COLOR_4X]->CreateEmpty(RHI::Texture2D, quarterWidth, quarterHeight, 1, 1, 1, screenImageFormat, screenTextureFlags | Texture::SRGBColorSpace);
    ppRTs[PP_RT_4X] = RenderTarget::Create(ppTextures[PP_TEXTURE_COLOR_4X], nullptr, RHI::SRGBWrite);

    ppTextures[PP_TEXTURE_COLOR_TEMP] = textureManager.AllocTexture(va("_%i_screenColorTemp", (int)contextHandle));
    ppTextures[PP_TEXTURE_COLOR_TEMP]->CreateEmpty(RHI::Texture2D, renderingWidth, renderingHeight, 1, 1, 1, screenImageFormat, screenTextureFlags | Texture::SRGBColorSpace);
    ppRTs[PP_RT_TEMP] = RenderTarget::Create(ppTextures[PP_TEXTURE_COLOR_TEMP], nullptr, RHI::SRGBWrite);

    ppTextures[PP_TEXTURE_COLOR_TEMP_2X] = textureManager.AllocTexture(va("_%i_screenColorTempD2x", (int)contextHandle));
    ppTextures[PP_TEXTURE_COLOR_TEMP_2X]->CreateEmpty(RHI::Texture2D, halfWidth, halfHeight, 1, 1, 1, screenImageFormat, screenTextureFlags | Texture::SRGBColorSpace);
    ppRTs[PP_RT_TEMP_2X] = RenderTarget::Create(ppTextures[PP_TEXTURE_COLOR_TEMP_2X], nullptr, RHI::SRGBWrite);

    ppTextures[PP_TEXTURE_COLOR_TEMP_4X] = textureManager.AllocTexture(va("_%i_screenColorTempD4x", (int)contextHandle));
    ppTextures[PP_TEXTURE_COLOR_TEMP_4X]->CreateEmpty(RHI::Texture2D, quarterWidth, quarterHeight, 1, 1, 1, screenImageFormat, screenTextureFlags | Texture::SRGBColorSpace);
    ppRTs[PP_RT_TEMP_4X] = RenderTarget::Create(ppTextures[PP_TEXTURE_COLOR_TEMP_4X], nullptr, RHI::SRGBWrite);

    //ppTextures[PP_TEXTURE_COLOR_TEMP_4X] = textureManager.AllocTexture(va("_%i_screenColorTempD4x", (int)contextHandle));
    //ppTextures[PP_TEXTURE_COLOR_TEMP_4X]->CreateEmpty(RHI::Texture2D, quarterWidth, quarterHeight, 1, 1, 1, screenImageFormat, screenTextureFlags | Texture::SRGBColorSpace);
    //ppRTs[PP_RT_BLUR] = RenderTarget::Create(ppTextures[PP_TEXTURE_COLOR_TEMP_4X], nullptr, RHI::SRGBWrite);

    ppTextures[PP_TEXTURE_LINEAR_DEPTH] = textureManager.AllocTexture(va("_%i_screenLinearDepth", (int)contextHandle));
    ppTextures[PP_TEXTURE_LINEAR_DEPTH]->CreateEmpty(RHI::Texture2D, renderingWidth, renderingHeight, 1, 1, 1, Image::L_16F, screenTextureFlags);
    ppRTs[PP_RT_LINEAR_DEPTH] = RenderTarget::Create(ppTextures[PP_TEXTURE_LINEAR_DEPTH], nullptr, 0);

    ppTextures[PP_TEXTURE_DEPTH_2X] = textureManager.AllocTexture(va("_%i_screenDepthD2x", (int)contextHandle));
    ppTextures[PP_TEXTURE_DEPTH_2X]->CreateEmpty(RHI::Texture2D, halfWidth, halfHeight, 1, 1, 1, Image::L_16F, screenTextureFlags);
    ppRTs[PP_RT_DEPTH_2X] = RenderTarget::Create(ppTextures[PP_TEXTURE_DEPTH_2X], nullptr, 0);

    ppTextures[PP_TEXTURE_DEPTH_4X] = textureManager.AllocTexture(va("_%i_screenDepthD4x", (int)contextHandle));
    ppTextures[PP_TEXTURE_DEPTH_4X]->CreateEmpty(RHI::Texture2D, quarterWidth, quarterHeight, 1, 1, 1, Image::L_16F, screenTextureFlags);
    ppRTs[PP_RT_DEPTH_4X] = RenderTarget::Create(ppTextures[PP_TEXTURE_DEPTH_4X], nullptr, 0);

    ppTextures[PP_TEXTURE_DEPTH_TEMP_4X] = textureManager.AllocTexture(va("_%i_screenDepthTempD4x", (int)contextHandle));
    ppTextures[PP_TEXTURE_DEPTH_TEMP_4X]->CreateEmpty(RHI::Texture2D, quarterWidth, quarterHeight, 1, 1, 1, Image::L_16F, screenTextureFlags);
    ppRTs[PP_RT_DEPTH_TEMP_4X] = RenderTarget::Create(ppTextures[PP_TEXTURE_DEPTH_TEMP_4X], nullptr, 0);

    ppTextures[PP_TEXTURE_VEL] = textureManager.AllocTexture(va("_%i_screenVelocity", (int)contextHandle));
    ppTextures[PP_TEXTURE_VEL]->CreateEmpty(RHI::Texture2D, halfWidth, halfHeight, 1, 1, 1, Image::RGBA_8_8_8_8, screenTextureFlags);
    ppRTs[PP_RT_VEL] = RenderTarget::Create(ppTextures[PP_TEXTURE_VEL], nullptr, 0);

    ppTextures[PP_TEXTURE_AO] = textureManager.AllocTexture(va("_%i_screenAo", (int)contextHandle));
    ppTextures[PP_TEXTURE_AO]->CreateEmpty(RHI::Texture2D, renderingWidth, renderingHeight, 1, 1, 1, Image::RGBA_8_8_8_8, screenTextureFlags);
    ppRTs[PP_RT_AO] = RenderTarget::Create(ppTextures[PP_TEXTURE_AO], nullptr, 0);

    ppTextures[PP_TEXTURE_AO_TEMP] = textureManager.AllocTexture(va("_%i_screenAoTemp", (int)contextHandle));
    ppTextures[PP_TEXTURE_AO_TEMP]->CreateEmpty(RHI::Texture2D, renderingWidth, renderingHeight, 1, 1, 1, Image::RGBA_8_8_8_8, screenTextureFlags);
    ppRTs[PP_RT_AO_TEMP] = RenderTarget::Create(ppTextures[PP_TEXTURE_AO_TEMP], nullptr, 0);

    //--------------------------------------
    // Create screen copy texture for refraction
    //--------------------------------------

    if (!currentRenderTexture) {
        currentRenderTexture = textureManager.AllocTexture(va("_%i_currentRender", (int)contextHandle));
    }

    currentRenderTexture->Purge();
    currentRenderTexture->CreateEmpty(RHI::Texture2D, renderingWidth, renderingHeight, 1, 1, 1, screenImageFormat,
        Texture::Clamp | Texture::NoMipmaps | Texture::NonPowerOfTwo | Texture::HighPriority | Texture::SRGBColorSpace);
}

void RenderContext::FreeScreenMapRT() {
    if (screenRT) {
        if (screenColorTexture) {
            textureManager.ReleaseTexture(screenColorTexture, true);
            screenColorTexture = nullptr;
        }

        if (screenNormalTexture) {
            textureManager.ReleaseTexture(screenNormalTexture, true);
            screenNormalTexture = nullptr;
        }

        if (screenDepthTexture) {
            textureManager.ReleaseTexture(screenDepthTexture, true);
            screenDepthTexture = nullptr;
        }

        RenderTarget::Delete(screenRT);
        screenRT = nullptr;
    }	

    if (screenLitAccRT) {
        if (screenLitAccTexture) {
            textureManager.ReleaseTexture(screenLitAccTexture, true);
            screenLitAccTexture = nullptr;
        }
        RenderTarget::Delete(screenLitAccRT);
        screenLitAccRT = nullptr;
    }

    if (screenSelectionRT) {
        if (screenSelectionTexture) {
            textureManager.ReleaseTexture(screenSelectionTexture, true);
            screenSelectionTexture = nullptr;
        }
        RenderTarget::Delete(screenSelectionRT);
        screenSelectionRT = nullptr;
    }

    if (homRT) {
        if (homTexture) {
            textureManager.ReleaseTexture(homTexture, true);
            homTexture = nullptr;
        }
        RenderTarget::Delete(homRT);
        homRT = nullptr;
    }

    for (int i = 0; i < MAX_PP_TEXTURES; i++) {
        if (ppTextures[i]) {
            textureManager.ReleaseTexture(ppTextures[i], true);
            ppTextures[i] = nullptr;
        }
    }

    for (int i = 0; i < MAX_PP_RTS; i++) {
        if (ppRTs[i]) {
            RenderTarget::Delete(ppRTs[i]);
            ppRTs[i] = nullptr;
        }
    }

    if (currentRenderTexture) {
        textureManager.ReleaseTexture(currentRenderTexture, true);
        currentRenderTexture = nullptr;
    }
}

void RenderContext::InitHdrMapRT() {
    FreeHdrMapRT();

    if (r_HDR.GetInteger() == 0) {
        return;
    }

    int quarterWidth = Math::Ceil(renderingWidth * 0.25f);
    int quarterHeight = Math::Ceil(renderingHeight * 0.25f);

    //--------------------------------------
    // HDR RT 
    //--------------------------------------
   
    Image::Format screenImageFormat = GetScreenImageFormat();
    Image hdrBloomImage;
    hdrBloomImage.Create2D(quarterWidth, quarterHeight, 1, screenImageFormat, nullptr, Image::LinearSpaceFlag);

    for (int i = 0; i < COUNT_OF(hdrBloomRT); i++) {
        hdrBloomTexture[i] = textureManager.AllocTexture(va("_%i_hdrBloom%i", (int)contextHandle, i));
        hdrBloomTexture[i]->Create(RHI::Texture2D, hdrBloomImage, 
            Texture::Clamp | Texture::NoMipmaps | Texture::HighQuality | Texture::NonPowerOfTwo | Texture::HighPriority);
        hdrBloomRT[i] = RenderTarget::Create(hdrBloomTexture[i], nullptr, 0);
    }

    Image::Format lumImageFormat = Image::L_16F;
    if (r_HDR.GetInteger() == 3) {
        lumImageFormat = Image::L_32F;
    }

    int size = Min(Math::CeilPowerOfTwo(Max(quarterWidth, quarterHeight)) >> 2, 1024);

    for (int i = 0; i < COUNT_OF(hdrLumAverageRT); i++) {
        hdrLumAverageTexture[i] = textureManager.AllocTexture(va("_%i_hdrLumAverage%i", (int)contextHandle, i));
        hdrLumAverageTexture[i]->CreateEmpty(RHI::Texture2D, size, size, 1, 1, 1, lumImageFormat, 
            Texture::Clamp | Texture::NoMipmaps | Texture::HighQuality | Texture::NonPowerOfTwo | Texture::HighPriority);
        hdrLumAverageRT[i] = RenderTarget::Create(hdrLumAverageTexture[i], nullptr, 0);
            
        if (size == 1) {
            break;
        }

        size = Max(size >> 2, 1);
    }

    for (int i = 0; i < COUNT_OF(hdrLuminanceTexture); i++) {
        hdrLuminanceTexture[i] = textureManager.AllocTexture(va("_%i_hdrLuminance%i", (int)contextHandle, i));
        hdrLuminanceTexture[i]->CreateEmpty(RHI::Texture2D, 1, 1, 1, 1, 1, lumImageFormat, 
            Texture::Clamp | Texture::Nearest | Texture::NoMipmaps | Texture::HighQuality | Texture::HighPriority);
        hdrLuminanceRT[i] = RenderTarget::Create(hdrLuminanceTexture[i], nullptr, 0);
        //hdrLuminanceRT[i]->Clear(Color4(0.5, 0.5, 0.5, 1.0), 0.0f, 0.0f);
    }
}

void RenderContext::FreeHdrMapRT() {
    for (int i = 0; i < COUNT_OF(hdrBloomRT); i++) {
        if (hdrBloomRT[i]) {
            if (hdrBloomTexture[i]) {
                textureManager.ReleaseTexture(hdrBloomTexture[i], true);
                hdrBloomTexture[i] = nullptr;
            }
            RenderTarget::Delete(hdrBloomRT[i]);
            hdrBloomRT[i] = nullptr;
        }
    }

    for (int i = 0; i < COUNT_OF(hdrLumAverageRT); i++) {
        if (hdrLumAverageRT[i]) {
            if (hdrLumAverageTexture[i]) {
                textureManager.ReleaseTexture(hdrLumAverageTexture[i], true);
                hdrLumAverageTexture[i] = nullptr;
            }
            RenderTarget::Delete(hdrLumAverageRT[i]);
            hdrLumAverageRT[i] = nullptr;
        }
    }

    for (int i = 0; i < 3; i++) {
        if (hdrLuminanceRT[i]) {
            if (hdrLuminanceTexture[i]) {
                textureManager.ReleaseTexture(hdrLuminanceTexture[i], true);
                hdrLuminanceTexture[i] = nullptr;
            }
            RenderTarget::Delete(hdrLuminanceRT[i]);
            hdrLuminanceRT[i] = nullptr;
        }
    }
}

void RenderContext::InitShadowMapRT() {
    FreeShadowMapRT();

    if (r_shadows.GetInteger() == 0) {
        return;
    }

    Image::Format shadowImageFormat = Image::Depth_24;
    Image::Format shadowCubeImageFormat = (r_shadowCubeMapFloat.GetBool() && rhi.SupportsDepthBufferFloat()) ? Image::Depth_32F : Image::Depth_24;

    RHI::TextureType textureType = RHI::Texture2DArray;

    int csmCount = r_CSM_count.GetInteger();

    // Cascaded shadow map
    shadowRenderTexture = textureManager.AllocTexture(va("_%i_shadowRender", (int)contextHandle));
    shadowRenderTexture->CreateEmpty(textureType, r_shadowMapSize.GetInteger(), r_shadowMapSize.GetInteger(), 1, csmCount, 1, shadowImageFormat,
        Texture::Shadow | Texture::Clamp | Texture::NoMipmaps | Texture::HighQuality | Texture::HighPriority);
    shadowMapRT = RenderTarget::Create(nullptr, shadowRenderTexture, 0);

    // Virtual shadow cube map
    vscmTexture = textureManager.AllocTexture(va("_%i_vscmRender", (int)contextHandle));
    vscmTexture->CreateEmpty(RHI::Texture2D, r_shadowCubeMapSize.GetInteger(), r_shadowCubeMapSize.GetInteger(), 1, 1, 1, shadowCubeImageFormat,
        Texture::Shadow | Texture::Clamp | Texture::NoMipmaps | Texture::NonPowerOfTwo | Texture::HighQuality | Texture::HighPriority);
    vscmRT = RenderTarget::Create(nullptr, vscmTexture, 0);
    vscmRT->Clear(Color4(0, 0, 0, 0), 1.0f, 0);

    vscmCleared[0] = false;
    vscmCleared[1] = false;
    vscmCleared[2] = false;
    vscmCleared[3] = false;
    vscmCleared[4] = false;
    vscmCleared[5] = false;

    if (!indirectionCubeMapTexture) {
        indirectionCubeMapTexture = textureManager.AllocTexture(va("_%i_indirectionCubeMap", (int)contextHandle));
        indirectionCubeMapTexture->CreateIndirectionCubemap(256, vscmRT->GetWidth(), vscmRT->GetHeight());
    }
    vscmBiasedFov = DEG2RAD(90.0f + 0.8f);
    vscmBiasedScale = 1.0f / Math::Tan(vscmBiasedFov * 0.5f);
}

void RenderContext::FreeShadowMapRT() {
    if (shadowMapRT) {
        if (shadowRenderTexture) {
            textureManager.ReleaseTexture(shadowRenderTexture, true);
        }

        if (shadowMapRT) {
            RenderTarget::Delete(shadowMapRT);
            shadowMapRT = nullptr;
        }
    }

    if (vscmRT) {
        if (vscmTexture) {
            textureManager.ReleaseTexture(vscmTexture, true);
        }

        if (vscmRT) {
            RenderTarget::Delete(vscmRT);
            vscmRT = nullptr;
        }
    }

    if (indirectionCubeMapTexture) {
        textureManager.ReleaseTexture(indirectionCubeMapTexture, true);
        indirectionCubeMapTexture = nullptr;
    }
}

void RenderContext::InitEnvCubeMapRT() {
    FreeEnvCubeMapRT();

    int envCubeTextureSize = r_envCubeMapSize.GetInteger();
    Image::Format format = r_envCubeMapHdr.GetBool() ? Image::RGB_16F_16F_16F : Image::RGB_8_8_8;

    envCubeTexture = textureManager.AllocTexture(va("_%i_envCube", (int)contextHandle));
    envCubeTexture->CreateEmpty(RHI::TextureCubeMap, envCubeTextureSize, envCubeTextureSize, 1, 1, 1, format,
        Texture::Clamp | Texture::Nearest | Texture::NoMipmaps | Texture::HighQuality);

    envCubeRT = RenderTarget::Create(envCubeTexture, nullptr, RHI::HasDepthBuffer);
}

void RenderContext::FreeEnvCubeMapRT() {
    if (envCubeTexture) {
        textureManager.ReleaseTexture(envCubeTexture, true);
        envCubeTexture = nullptr;
    }

    if (envCubeRT) {
        RenderTarget::Delete(envCubeRT);
        envCubeRT = nullptr;
    }
}

void RenderContext::OnResize(int width, int height) {
    float upscaleX = GetUpscaleFactorX();
    float upscaleY = GetUpscaleFactorY();

    RHI::DisplayMetrics displayMetrics;

    rhi.GetDisplayMetrics(this->contextHandle, &displayMetrics);

    this->windowWidth = displayMetrics.screenWidth;
    this->windowHeight = displayMetrics.screenHeight;
    this->deviceWidth = displayMetrics.backingWidth;
    this->deviceHeight = displayMetrics.backingHeight;
    this->safeAreaInsets = displayMetrics.safeAreaInsets;

    // deferred resizing render targets
    renderingWidth = this->deviceWidth / upscaleX;
    renderingHeight = this->deviceHeight / upscaleY;

    guiMesh.SetClipRect(Rect(0, 0, renderingWidth, renderingHeight));

    //rhi.ChangeDisplaySettings(deviceWidth, deviceHeight, rhi.IsFullScreen());
}

void RenderContext::Display() {
    rhi.DisplayContext(contextHandle);
}

void RenderContext::BeginFrame() {
    startFrameSec = PlatformTime::Seconds();

    frameTime = startFrameSec - elapsedTime;

    elapsedTime = startFrameSec;

    memset(&renderCounter, 0, sizeof(renderCounter));

    rhi.SetContext(GetContextHandle());

    // Window size have changed since last call of BeginFrame()
    if (renderingWidth != screenRT->GetWidth() || renderingHeight != screenRT->GetHeight()) {
        FreeScreenMapRT();
        FreeHdrMapRT();

        InitScreenMapRT();
        InitHdrMapRT();
    }

    BeginCommands();
}

void RenderContext::EndFrame() {
    frameCount++;

    // Adds GUI commands
    renderSystem.primaryWorld->DrawGUICamera(guiMesh);

    // Adds swap buffer command
    SwapBuffersRenderCommand *cmd = (SwapBuffersRenderCommand *)renderSystem.GetCommandBuffer(sizeof(SwapBuffersRenderCommand));
    cmd->commandId = SwapBuffersCommand;

    RenderCommands();

    guiMesh.Clear();

    renderCounter.frameMsec = PlatformTime::Milliseconds() - SEC2MS(startFrameSec);

    if (r_showStats.GetInteger() > 0) {
        switch (r_showStats.GetInteger()) {
        case 1:
            BE_LOG("draw:%i verts:%i tris:%i sdraw:%i sverts:%i stris:%i\n", 
                renderCounter.drawCalls, renderCounter.drawVerts, renderCounter.drawIndexes / 3, 
                renderCounter.shadowDrawCalls, renderCounter.shadowDrawVerts, renderCounter.shadowDrawIndexes / 3);
            break;
        case 2:
            BE_LOG("frame:%i rb:%i homGen:%i homQuery:%i homCull:%i\n", 
                renderCounter.frameMsec, renderCounter.backEndMsec, renderCounter.homGenMsec, renderCounter.homQueryMsec, renderCounter.homCullMsec);
            break;
        case 3:
            BE_LOG("shadowmap:%i skinning:%i\n",
                renderCounter.numShadowMapDraw, renderCounter.numSkinningEntities);
            break;
        }
    }
}

void RenderContext::BeginCommands() {
    renderSystem.currentContext = this;

    rhi.SetContext(GetContextHandle());

    bufferCacheManager.BeginWrite();

    BeginContextRenderCommand *cmd = (BeginContextRenderCommand *)renderSystem.GetCommandBuffer(sizeof(BeginContextRenderCommand));
    cmd->commandId = BeginContextCommand;
    cmd->renderContext = this;
}

void RenderContext::RenderCommands() {
    bufferCacheManager.BeginBackEnd();

    renderSystem.IssueCommands();

    bufferCacheManager.EndDrawCommand();

    frameData.ToggleFrame();

    renderSystem.currentContext = nullptr;
}

void RenderContext::AdjustFrom640x480(float *x, float *y, float *w, float *h) const {
    float xScale = deviceWidth / 640.0f;
    float yScale = deviceHeight / 480.0f;

    if (x) {
        *x *= xScale;
    }
    if (y) {
        *y *= yScale;
    }
    if (w) {
        *w *= xScale;
    }
    if (h) {
        *h *= yScale;
    }
}

void RenderContext::SetClipRect(const Rect &clipRect) {
    guiMesh.SetClipRect(clipRect);
}

void RenderContext::DrawPic(float x, float y, float w, float h, const Material *material) {
    guiMesh.SetColor(color);
    guiMesh.DrawPic(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, material);
}

void RenderContext::DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, const Material *material) {
    guiMesh.SetColor(color);
    guiMesh.DrawPic(x, y, w, h, s1, t1, s2, t2, material);
}

void RenderContext::DrawBar(float x, float y, float w, float h) {
    guiMesh.SetColor(color);
    guiMesh.DrawPic(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color.a < 1.0f ? materialManager.blendMaterial : materialManager.unlitMaterial);
}

void RenderContext::DrawRect(float x, float y, float w, float h) {
    guiMesh.SetColor(color);

    if (w > 1) {
        DrawBar(x, y, w, 1);
        if (h > 1) {
            DrawBar(x, y + h - 1, w, 1);
        }
    }

    if (h > 2) {
        DrawBar(x, y + 1, 1, h - 2);
        if (w > 2) {
            DrawBar(x + w - 1, y + 1, 1, h - 2);
        }
    }
}

void RenderContext::UpdateCurrentRenderTexture() const {
    rhi.SetStateBits(RHI::ColorWrite | RHI::AlphaWrite);
    rhi.SelectTextureUnit(0);
    
    //double starttime = PlatformTime::Seconds();

    currentRenderTexture->Bind();
    rhi.CopyTextureSubImage2D(0, 0, 0, 0, currentRenderTexture->GetWidth(), currentRenderTexture->GetHeight());
    //rhi.GenerateMipmap();
    
    //BE_LOG("%lf\n", PlatformTime::Seconds() - starttime);
}

float RenderContext::QueryDepth(const Point &point) {
    Image::Format format = screenSelectionRT->ColorTexture()->GetFormat();
    byte *depthData = (byte *)_alloca(4);

    float scaleX = (float)screenSelectionRT->GetWidth() / screenRT->GetWidth();
    float scaleY = (float)screenSelectionRT->GetHeight() / screenRT->GetHeight();

    Vec2 scaledReadPoint;
    scaledReadPoint.x = point.x * scaleX;
    scaledReadPoint.y = (screenRT->GetHeight() - (point.y + 1)) * scaleY;

    screenSelectionRT->Begin();

    // FIXME: is depth format confirmed ?
    rhi.ReadPixels(scaledReadPoint.x, scaledReadPoint.y, 1, 1, Image::Depth_24, depthData); 
    screenSelectionRT->End();

    float depth = (float)MAKE_FOURCC(depthData[2], depthData[1], depthData[0], 0) / (float)(BIT(24) - 1);
    return depth;
}

int RenderContext::QuerySelection(const Point &point) {
    Image::Format format = screenSelectionRT->ColorTexture()->GetFormat();
    byte *data = (byte *)_alloca(Image::BytesPerPixel(format));

    float scaleX = (float)screenSelectionRT->GetWidth() / screenRT->GetWidth();
    float scaleY = (float)screenSelectionRT->GetHeight() / screenRT->GetHeight();

    Vec2 scaledReadPoint;
    scaledReadPoint.x = point.x * scaleX;
    scaledReadPoint.y = (screenRT->GetHeight() - (point.y + 1)) * scaleY;

    screenSelectionRT->Begin();
    rhi.ReadPixels(scaledReadPoint.x, scaledReadPoint.y, 1, 1, format, data);
    screenSelectionRT->End();

    int id = ((int)data[2] << 16) | ((int)data[1] << 8) | ((int)data[0]);

    return id;
}

bool RenderContext::QuerySelection(const Rect &rect, Inclusion inclusion, Array<int> &indexes) {
    if (rect.IsEmpty()) {
        return false;
    }

    float scaleX = (float)screenSelectionRT->GetWidth() / screenRT->GetWidth();
    float scaleY = (float)screenSelectionRT->GetHeight() / screenRT->GetHeight();

    Rect scaledReadRect;
    scaledReadRect.x = rect.x * scaleX;
    scaledReadRect.y = (screenRT->GetHeight() - (rect.y + rect.h)) * scaleY;
    scaledReadRect.w = Max(rect.w * scaleX, 1.0f);
    scaledReadRect.h = Max(rect.h * scaleY, 1.0f);

    Image::Format format = screenSelectionRT->ColorTexture()->GetFormat();
    int bpp = Image::BytesPerPixel(format);

    if (inclusion == Inclusion::Crossing) {
        int pixelCount = scaledReadRect.w * scaledReadRect.h;
        byte *data = (byte *)Mem_Alloc(bpp * pixelCount);
    
        screenSelectionRT->Begin();
        rhi.ReadPixels(scaledReadRect.x, scaledReadRect.y, scaledReadRect.w, scaledReadRect.h, format, data);
        screenSelectionRT->End();

        indexes.Clear();

        byte *data_ptr = data;

        for (int i = 0; i < pixelCount; i++) {
            int id = (((int)data_ptr[2]) << 16) | (((int)data_ptr[1]) << 8) | ((int)data_ptr[0]);
            indexes.AddUnique(id);

            data_ptr += bpp;
        }

        Mem_Free(data);
    } else if (inclusion == Inclusion::Window) {
        byte *data = (byte *)Mem_Alloc(bpp * screenSelectionRT->GetWidth() * screenSelectionRT->GetHeight());
    
        screenSelectionRT->Begin();
        rhi.ReadPixels(0, 0, screenSelectionRT->GetWidth(), screenSelectionRT->GetHeight(), format, data);
        screenSelectionRT->End();

        indexes.Clear();

        for (int y = scaledReadRect.y; y < scaledReadRect.Y2(); y++) {
            int pitch = y * screenSelectionRT->GetWidth() * bpp;

            for (int x = scaledReadRect.x; x < scaledReadRect.X2(); x++) {
                byte *data_ptr = &data[pitch + x * bpp];

                int id = (((int)data_ptr[2]) << 16) | (((int)data_ptr[1]) << 8) | ((int)data_ptr[0]);
                indexes.AddUnique(id);
            }
        }
        
        for (int y = 0; y < screenSelectionRT->GetHeight(); y++) {
            int pitch = y * screenSelectionRT->GetWidth() * bpp;

            for (int x = 0; x < screenSelectionRT->GetWidth(); x++) {
                if (!scaledReadRect.IsContainPoint(x, y)) {
                    byte *data_ptr = &data[pitch + x * bpp];

                    int id = (((int)data_ptr[2]) << 16) | (((int)data_ptr[1]) << 8) | ((int)data_ptr[0]);
                    indexes.RemoveFast(id);
                }
            }
        }

        Mem_Free(data);
    }

    if (indexes.Count() > 0) {
        return true;
    }

    return false;
}

void RenderContext::TakeScreenShot(const char *filename, RenderWorld *renderWorld, int layerMask, const Vec3 &origin, const Mat3 &axis, float fov, int width, int height) {
    char path[256];

    RenderCamera renderCamera;
    RenderCamera::State cameraDef;
    cameraDef.flags = RenderCamera::Flag::TexturedMode | RenderCamera::Flag::NoSubViews | RenderCamera::Flag::SkipDebugDraw;
    cameraDef.clearMethod = RenderCamera::SkyboxClear;
    cameraDef.clearColor = Color4(0.29f, 0.33f, 0.35f, 0);
    cameraDef.layerMask = layerMask;
    cameraDef.renderRect.Set(0, 0, width, height);
    cameraDef.origin = origin;
    cameraDef.axis = axis;
    cameraDef.orthogonal = false;

    Vec3 v;
    renderWorld->GetStaticAABB().GetFarthestVertexFromDir(axis[0], v);
    cameraDef.zFar = Max(BE1::MeterToUnit(100.0f), origin.Distance(v));
    cameraDef.zNear = BE1::CentiToUnit(5.0f);

    RenderCamera::ComputeFov(fov, 1.25f, (float)width / height, &cameraDef.fovX, &cameraDef.fovY);

    renderCamera.Update(&cameraDef);

    BeginFrame();
    renderWorld->RenderScene(&renderCamera);
    Str::snPrintf(path, sizeof(path), "%s.png", filename);
    renderSystem.CmdScreenshot(0, 0, width, height, path);
    EndFrame();

    BE_DLOG("Screenshot saved to \"%s\"\n", path);
}

void RenderContext::CaptureEnvCubeRT(RenderWorld *renderWorld, int layerMask, int staticMask, const Vec3 &origin, RenderTarget *targetCubeRT) {
    RenderCamera renderCamera;
    RenderCamera::State cameraDef;

    memset(&cameraDef, 0, sizeof(cameraDef));
    cameraDef.flags = RenderCamera::Flag::TexturedMode | RenderCamera::Flag::NoSubViews | RenderCamera::Flag::SkipDebugDraw | RenderCamera::Flag::SkipPostProcess;
    cameraDef.clearMethod = RenderCamera::ClearMethod::SkyboxClear;
    cameraDef.layerMask = layerMask;
    cameraDef.staticMask = staticMask;
    cameraDef.renderRect.Set(0, 0, targetCubeRT->GetWidth(), targetCubeRT->GetHeight());
    cameraDef.fovX = 90;
    cameraDef.fovY = 90;
    cameraDef.zNear = BE1::CentiToUnit(5.0f);
    cameraDef.zFar = BE1::MeterToUnit(100.0f);
    cameraDef.origin = origin;
    cameraDef.orthogonal = false;

    if (staticMask) {
        cameraDef.flags |= RenderCamera::Flag::StaticOnly;
    }

    for (int faceIndex = 0; faceIndex < 6; faceIndex++) {
        R_EnvCubeMapFaceToEngineAxis((RHI::CubeMapFace)faceIndex, cameraDef.axis);

        renderCamera.Update(&cameraDef);

        targetCubeRT->Begin(0, faceIndex);

        BeginCommands();

        renderWorld->RenderScene(&renderCamera);

        RenderCommands();

        targetCubeRT->End();
    }
}

void RenderContext::CaptureEnvCubeImage(RenderWorld *renderWorld, int layerMask, int staticMask, const Vec3 &origin, int size, Image &envCubeImage) {
    CaptureEnvCubeRT(renderWorld, layerMask, staticMask, origin, envCubeRT);

    Texture::GetCubeImageFromCubeTexture(envCubeTexture, 1, envCubeImage);
}

void RenderContext::TakeEnvShot(const char *filename, RenderWorld *renderWorld, int layerMask, int staticMask, const Vec3 &origin, int size) {
    Image envCubeImage;
    CaptureEnvCubeImage(renderWorld, layerMask, staticMask, origin, size, envCubeImage);

    char path[256];
    Str::snPrintf(path, sizeof(path), "%s.dds", filename);
    envCubeImage.ConvertFormatSelf(Image::RGB_11F_11F_10F, false, Image::HighQuality);
    envCubeImage.WriteDDS(path);

    BE_LOG("Environment cubemap snapshot saved to \"%s\"\n", path);
}

void RenderContext::TakeIrradianceEnvShot(const char *filename, RenderWorld *renderWorld, int layerMask, int staticMask, const Vec3 &origin) {
    CaptureEnvCubeRT(renderWorld, layerMask, staticMask, origin, envCubeRT);

    Texture *irradianceEnvCubeTexture = new Texture;
    irradianceEnvCubeTexture->CreateEmpty(RHI::TextureCubeMap, 64, 64, 1, 1, 1, Image::RGB_32F_32F_32F, 
        Texture::Clamp | Texture::Nearest | Texture::NoMipmaps | Texture::HighQuality);
    RenderTarget *irradianceEnvCubeRT = RenderTarget::Create(irradianceEnvCubeTexture, nullptr, 0);
#if 1
    GenerateIrradianceEnvCubeRT(envCubeTexture, irradianceEnvCubeRT);
#else
    GenerateSHConvolvIrradianceEnvCubeRT(envCubeTexture, irradianceEnvCubeRT);
#endif

    Image irradianceEnvCubeImage;
    Texture::GetCubeImageFromCubeTexture(irradianceEnvCubeTexture, 1, irradianceEnvCubeImage);

    SAFE_DELETE(irradianceEnvCubeTexture);
    RenderTarget::Delete(irradianceEnvCubeRT);

    char path[256];
    Str::snPrintf(path, sizeof(path), "%s.dds", filename);
    irradianceEnvCubeImage.ConvertFormatSelf(Image::RGB_11F_11F_10F, false, Image::HighQuality);
    irradianceEnvCubeImage.WriteDDS(path);

    BE_LOG("Generated diffuse irradiance cubemap to \"%s\"\n", path);
}

void RenderContext::TakePrefilteredEnvShot(const char *filename, RenderWorld *renderWorld, int layerMask, int staticMask, const Vec3 &origin) {
    CaptureEnvCubeRT(renderWorld, layerMask, staticMask, origin, envCubeRT);

    int size = 256;
    int numMipLevels = Math::Log(2, size) + 1;

    Texture *prefilteredCubeTexture = new Texture;
    prefilteredCubeTexture->CreateEmpty(RHI::TextureCubeMap, size, size, 1, 1, numMipLevels, Image::RGB_32F_32F_32F, 
        Texture::Clamp | Texture::Nearest | Texture::NoMipmaps | Texture::HighQuality);
    RenderTarget *prefilteredCubeRT = RenderTarget::Create(prefilteredCubeTexture, nullptr, 0);
#if 1
    GenerateGGXLDSumRT(envCubeTexture, prefilteredCubeRT);
#else
    GeneratePhongSpecularLDSumRT(envCubeTexture, 2048, prefilteredCubeRT);
#endif

    Image prefilteredCubeImage;
    Texture::GetCubeImageFromCubeTexture(prefilteredCubeTexture, numMipLevels, prefilteredCubeImage);

    SAFE_DELETE(prefilteredCubeTexture);
    RenderTarget::Delete(prefilteredCubeRT);

    char path[256];
    Str::snPrintf(path, sizeof(path), "%s.dds", filename);
    prefilteredCubeImage.ConvertFormatSelf(Image::RGB_11F_11F_10F, false, Image::HighQuality);
    prefilteredCubeImage.WriteDDS(path);

    BE_LOG("Generated specular prefiltered cubemap to \"%s\"\n", path);
}

void RenderContext::WriteGGXDFGSum(const char *filename, int size) const {
    Image integrationImage;
    GenerateGGXDFGSumImage(size, integrationImage);

    char path[256];
    Str::snPrintf(path, sizeof(path), "%s.dds", filename);
    integrationImage.WriteDDS(path);

    BE_LOG("Generated GGX integration LUT to \"%s\"\n", path);
}

void RenderContext::GenerateSHConvolvIrradianceEnvCubeRT(const Texture *envCubeTexture, RenderTarget *targetCubeRT) const {
    //-------------------------------------------------------------------------------
    // Create 4-by-4 envmap sized block weight map for each faces
    //------------------------------------------------------------------------------- 
    int envMapSize = envCubeTexture->GetWidth();
    Texture *weightTextures[6];

    float *weightData = (float *)Mem_Alloc(envMapSize * 4 * envMapSize * 4 * sizeof(float));
    float invSize = 1.0f / (envMapSize - 1);

    for (int faceIndex = 0; faceIndex < 6; faceIndex++) {
        for (int y = 0; y < envMapSize; y++) {
            for (int x = 0; x < envMapSize; x++) {
                float s = (x + 0.5f) * invSize;
                float t = (y + 0.5f) * invSize;

                // Gets sample direction for each faces 
                Vec3 dir = Image::FaceToCubeMapCoords((Image::CubeMapFace)faceIndex, s, t);
                dir.Normalize();

                // 9 terms are required for order 3 SH basis functions
                float basisEval[16] = { 0, };
                // Evaluates the 9 SH basis functions Ylm with the given direction
                SphericalHarmonics::EvalBasis(3, dir, basisEval);

                // Solid angle of the cubemap texel
                float dw = Image::CubeMapTexelSolidAngle(x, y, envMapSize);

                // Precalculates 9 terms (basisEval * dw) for each envmap pixel in the 4-by-4 envmap sized block texture for each faces  
                for (int j = 0; j < 4; j++) {
                    for (int i = 0; i < 4; i++) {
                        int offset = (((j * envMapSize + y) * envMapSize) << 2) + i * envMapSize + x;

                        weightData[offset] = basisEval[(j << 2) + i] * dw;
                    }
                }
            }
        }

        weightTextures[faceIndex] = new Texture;
        weightTextures[faceIndex]->Create(RHI::Texture2D, Image(envMapSize * 4, envMapSize * 4, 1, 1, 1, Image::L_32F, (byte *)weightData, Image::LinearSpaceFlag),
            Texture::Clamp | Texture::Nearest | Texture::NoMipmaps | Texture::HighQuality);
    }

    Mem_Free(weightData);

    //-------------------------------------------------------------------------------
    // SH projection of (Li * dw) and create 9 coefficents in a single 4x4 texture
    //-------------------------------------------------------------------------------
    Shader *weightedSHProjShader = shaderManager.GetShader("Shaders/WeightedSHProj");
    Shader *shader = weightedSHProjShader->InstantiateShader(Array<Shader::Define>());

    Image image;
    image.Create2D(4, 4, 1, Image::RGB_32F_32F_32F, nullptr, Image::LinearSpaceFlag);
    Texture *incidentCoeffTexture = new Texture;
    incidentCoeffTexture->Create(RHI::Texture2D, image, Texture::Clamp | Texture::Nearest | Texture::NoMipmaps | Texture::HighQuality);
    
    RenderTarget *incidentCoeffRT = RenderTarget::Create(incidentCoeffTexture, nullptr, 0);

    incidentCoeffRT->Begin();

    rhi.SetViewport(Rect(0, 0, 4, 4));
    rhi.SetStateBits(RHI::ColorWrite);
    rhi.SetCullFace(RHI::NoCull);

    shader->Bind();
    shader->SetTextureArray("weightMap", 6, (const Texture **)weightTextures);
    shader->SetTexture("radianceCubeMap", envCubeTexture);
    shader->SetConstant1i("radianceCubeMapSize", envCubeTexture->GetWidth());
    shader->SetConstant1f("radianceScale", 1.0f);

    RB_DrawClipRect(0, 0, 1.0f, 1.0f);

    //rhi.ReadPixels(0, 0, 4, 4, Image::RGB_32F_32F_32F, image.GetPixels());

    incidentCoeffRT->End();

    shaderManager.ReleaseShader(shader);
    shaderManager.ReleaseShader(weightedSHProjShader);

    for (int faceIndex = 0; faceIndex < 6; faceIndex++) {
        SAFE_DELETE(weightTextures[faceIndex]);
    }

    //-------------------------------------------------------------------------------
    // SH convolution
    //-------------------------------------------------------------------------------
    Shader *genDiffuseCubeMapSHConvolv = shaderManager.GetShader("Shaders/GenIrradianceEnvCubeMapSHConvolv");
    shader = genDiffuseCubeMapSHConvolv->InstantiateShader(Array<Shader::Define>());

    int size = targetCubeRT->GetWidth();

    // Precompute ZH coefficients * sqrt(4PI/(2l + 1)) of Lambert diffuse spherical function cos(theta) / PI
    // which function is rotationally symmetric so only 3 terms are needed
    float al[3];
    al[0] = SphericalHarmonics::Lambert_Al_Evaluator(0); // 1
    al[1] = SphericalHarmonics::Lambert_Al_Evaluator(1); // 2/3
    al[2] = SphericalHarmonics::Lambert_Al_Evaluator(2); // 1/4

    for (int faceIndex = RHI::PositiveX; faceIndex <= RHI::NegativeZ; faceIndex++) {
        targetCubeRT->Begin(0, faceIndex);

        rhi.SetViewport(Rect(0, 0, size, size));
        rhi.SetStateBits(RHI::ColorWrite);
        rhi.SetCullFace(RHI::NoCull);

        shader->Bind();
        shader->SetTexture("incidentCoeffMap", incidentCoeffRT->ColorTexture());
        shader->SetConstant1i("targetCubeMapSize", size);
        shader->SetConstant1i("targetCubeMapFace", faceIndex);
        shader->SetConstantArray1f("lambertCoeff", COUNT_OF(al), al);

        RB_DrawClipRect(0, 0, 1.0f, 1.0f);

        targetCubeRT->End();
    }

    SAFE_DELETE(incidentCoeffTexture);
    RenderTarget::Delete(incidentCoeffRT);

    shaderManager.ReleaseShader(shader);
    shaderManager.ReleaseShader(genDiffuseCubeMapSHConvolv);
}

void RenderContext::GenerateIrradianceEnvCubeRT(const Texture *envCubeTexture, RenderTarget *targetCubeRT) const {
    Shader *genDiffuseCubeMapShader = shaderManager.GetShader("Shaders/GenIrradianceEnvCubeMap");
    Shader *shader = genDiffuseCubeMapShader->InstantiateShader(Array<Shader::Define>());

    int size = targetCubeRT->GetWidth();

    for (int faceIndex = RHI::PositiveX; faceIndex <= RHI::NegativeZ; faceIndex++) {
        targetCubeRT->Begin(0, faceIndex);

        rhi.SetViewport(Rect(0, 0, size, size));
        rhi.SetStateBits(RHI::ColorWrite);
        rhi.SetCullFace(RHI::NoCull);

        shader->Bind();
        shader->SetTexture("radianceCubeMap", envCubeTexture);
        shader->SetConstant1i("radianceCubeMapSize", envCubeTexture->GetWidth());
        shader->SetConstant1i("targetCubeMapSize", size);
        shader->SetConstant1i("targetCubeMapFace", faceIndex);

        RB_DrawClipRect(0, 0, 1.0f, 1.0f);

        targetCubeRT->End();
    }

    shaderManager.ReleaseShader(shader);
    shaderManager.ReleaseShader(genDiffuseCubeMapShader);
}

void RenderContext::GeneratePhongSpecularLDSumRT(const Texture *envCubeTexture, int maxSpecularPower, RenderTarget *targetCubeRT) const {
    Shader *genLDSumPhongSpecularShader = shaderManager.GetShader("Shaders/GenLDSumPhongSpecular");
    Shader *shader = genLDSumPhongSpecularShader->InstantiateShader(Array<Shader::Define>());

    int size = targetCubeRT->GetWidth();

    int numMipLevels = Math::Log(2, size) + 1;

    // power drop range [maxSpecularPower, 2]
    float powerDropOnMip = Math::Pow(maxSpecularPower, -1.0f / numMipLevels);

    for (int faceIndex = RHI::PositiveX; faceIndex <= RHI::NegativeZ; faceIndex++) {
        float specularPower = maxSpecularPower;

        for (int mipLevel = 0; mipLevel < numMipLevels; mipLevel++) {
            int mipSize = size >> mipLevel;

            targetCubeRT->Begin(mipLevel, faceIndex);

            rhi.SetViewport(Rect(0, 0, mipSize, mipSize));
            rhi.SetStateBits(RHI::ColorWrite);
            rhi.SetCullFace(RHI::NoCull);

            shader->Bind();
            shader->SetTexture("radianceCubeMap", envCubeTexture);
            shader->SetConstant1i("radianceCubeMapSize", envCubeTexture->GetWidth());
            shader->SetConstant1i("targetCubeMapSize", mipSize);
            shader->SetConstant1i("targetCubeMapFace", faceIndex);
            shader->SetConstant1f("specularPower", specularPower);

            RB_DrawClipRect(0, 0, 1.0f, 1.0f);

            targetCubeRT->End();

            specularPower *= powerDropOnMip;
        }
    }

    shaderManager.ReleaseShader(shader);
    shaderManager.ReleaseShader(genLDSumPhongSpecularShader);
}

void RenderContext::GenerateGGXLDSumRT(const Texture *envCubeTexture, RenderTarget *targetCubeRT) const {
    Shader *genLDSumGGXShader = shaderManager.GetShader("Shaders/GenLDSumGGX");
    Shader *shader = genLDSumGGXShader->InstantiateShader(Array<Shader::Define>());

    int size = targetCubeRT->GetWidth();

    int numMipLevels = Math::Log(2, size) + 1;

    for (int faceIndex = RHI::PositiveX; faceIndex <= RHI::NegativeZ; faceIndex++) {
        // TODO: We can skip mip level 0 for perfect specular mirror.
        for (int mipLevel = 0; mipLevel < numMipLevels; mipLevel++) {
            int mipSize = size >> mipLevel;

            float roughness = (float)mipLevel / (numMipLevels - 1);
            
            targetCubeRT->Begin(mipLevel, faceIndex);

            rhi.SetViewport(Rect(0, 0, mipSize, mipSize));
            rhi.SetStateBits(RHI::ColorWrite);
            rhi.SetCullFace(RHI::NoCull);

            shader->Bind();
            shader->SetTexture("radianceCubeMap", envCubeTexture);
            shader->SetConstant1i("radianceCubeMapSize", envCubeTexture->GetWidth());
            shader->SetConstant1i("targetCubeMapSize", mipSize);
            shader->SetConstant1i("targetCubeMapFace", faceIndex);
            shader->SetConstant1f("roughness", roughness);

            RB_DrawClipRect(0, 0, 1.0f, 1.0f);

            targetCubeRT->End();
        }
    }

    shaderManager.ReleaseShader(shader);
    shaderManager.ReleaseShader(genLDSumGGXShader);
}

void RenderContext::GenerateGGXDFGSumImage(int size, Image &integrationImage) const {
    Shader *genDFGSumGGXShader = shaderManager.GetShader("Shaders/GenDFGSumGGX");
    Shader *shader = genDFGSumGGXShader->InstantiateShader(Array<Shader::Define>());

    Texture *integrationLutTexture = new Texture;
    integrationLutTexture->CreateEmpty(RHI::Texture2D, size, size, 1, 1, 1, Image::RG_16F_16F, 
        Texture::Clamp | Texture::Nearest | Texture::NoMipmaps | Texture::HighQuality);
    
    RenderTarget *integrationLutRT = RenderTarget::Create(integrationLutTexture, nullptr, 0);

    integrationLutRT->Begin(0, 0);

    rhi.SetViewport(Rect(0, 0, size, size));
    rhi.SetStateBits(RHI::ColorWrite);
    rhi.SetCullFace(RHI::NoCull);

    shader->Bind();

    RB_DrawClipRect(0, 0, 1.0f, 1.0f);

    integrationImage.Create2D(size, size, 1, Image::RG_16F_16F, nullptr, Image::LinearSpaceFlag);

    rhi.ReadPixels(0, 0, size, size, Image::RG_16F_16F, integrationImage.GetPixels());

    integrationLutRT->End();

    SAFE_DELETE(integrationLutTexture);

    RenderTarget::Delete(integrationLutRT);

    shaderManager.ReleaseShader(shader);
    shaderManager.ReleaseShader(genDFGSumGGXShader);
}

BE_NAMESPACE_END
