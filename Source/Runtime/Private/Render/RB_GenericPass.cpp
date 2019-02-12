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
#include "RBackEnd.h"

BE_NAMESPACE_BEGIN

// Render sky only
void RB_BackgroundPass(int numDrawSurfs, DrawSurf **drawSurfs) {
    const VisObject *   prevSpace = nullptr;
    const Material *    prevMaterial = nullptr;

    backEnd.batch.SetCurrentLight(nullptr);

    for (int i = 0; i < numDrawSurfs; i++) {
        const DrawSurf *drawSurf = drawSurfs[i];

        if (drawSurf->material->GetSort() == Material::Sort::SkySort) {
            continue;
        }

        bool isDifferentObject = drawSurf->space != prevSpace;
        bool isDifferentMaterial = drawSurf->material != prevMaterial;

        if (isDifferentMaterial || isDifferentObject) {
            if (prevMaterial) {
                backEnd.batch.Flush();
            }

            backEnd.batch.Begin(Batch::BackgroundFlush, drawSurf->material, drawSurf->materialRegisters, drawSurf->space);

            prevMaterial = drawSurf->material;

            if (isDifferentObject) {
                prevSpace = drawSurf->space;

                backEnd.modelViewMatrix = drawSurf->space->modelViewMatrix;
                backEnd.modelViewProjMatrix = drawSurf->space->modelViewProjMatrix;
            }
        }

        backEnd.batch.DrawSubMesh(drawSurf->subMesh);
    }

    // Flush previous batch
    if (prevMaterial) {
        backEnd.batch.Flush();
    }
}

void RB_SelectionPass(int numDrawSurfs, DrawSurf **drawSurfs) {
    const VisObject *   prevSpace = nullptr;
    const Material *    prevMaterial = nullptr;
    bool                prevDepthHack = false;

    backEnd.batch.SetCurrentLight(nullptr);

    for (int i = 0; i < numDrawSurfs; i++) {
        const DrawSurf *drawSurf = drawSurfs[i];

        if ((drawSurf->flags & DrawSurf::SkipSelection) || !(drawSurf->flags & DrawSurf::Visible)) {
            continue;
        }

        if (drawSurf->material->GetSort() == Material::Sort::SkySort) {
            continue;
        }

        bool isDifferentObject = drawSurf->space != prevSpace;
        bool isDifferentMaterial = drawSurf->material != prevMaterial;

        if (isDifferentMaterial || isDifferentObject) {
            if (drawSurf->space->def->GetState().flags & RenderObject::SkipSelectionFlag) {
                continue;
            }

            if (!prevMaterial) {
                backEnd.batch.Begin(Batch::SelectionFlush, drawSurf->material, drawSurf->materialRegisters, drawSurf->space);
            } else {
                if (isDifferentObject || prevMaterial->GetCullType() != drawSurf->material->GetCullType() ||
                    (prevMaterial->GetSort() == Material::Sort::AlphaTestSort) || (drawSurf->material->GetSort() == Material::Sort::AlphaTestSort)) {
                    backEnd.batch.Flush();
                    backEnd.batch.Begin(Batch::SelectionFlush, drawSurf->material, drawSurf->materialRegisters, drawSurf->space);
                }
            }

            prevMaterial = drawSurf->material;

            if (isDifferentObject) {
                bool depthHack = !!(drawSurf->space->def->GetState().flags & RenderObject::DepthHackFlag);

                if (prevDepthHack != depthHack) {
                    if (depthHack) {
                        rhi.SetDepthRange(0.0f, 0.1f);
                    } else {
                        rhi.SetDepthRange(0.0f, 1.0f);
                    }

                    prevDepthHack = depthHack;
                }

                backEnd.modelViewMatrix = drawSurf->space->modelViewMatrix;
                backEnd.modelViewProjMatrix = drawSurf->space->modelViewProjMatrix;

                prevSpace = drawSurf->space;
            }
        }

        backEnd.batch.DrawSubMesh(drawSurf->subMesh);
    }

    // Flush previous batch
    if (prevMaterial) {
        backEnd.batch.Flush();
    }
    
    // Restore depth hack
    if (prevDepthHack) {
        rhi.SetDepthRange(0.0f, 1.0f);
    }
}

void RB_OccluderPass(int numDrawSurfs, DrawSurf **drawSurfs) {
    const VisObject *   prevSpace = nullptr;
    const Material *    prevMaterial = nullptr;
    bool                prevDepthHack = false;

    backEnd.batch.SetCurrentLight(nullptr);

    for (int i = 0; i < numDrawSurfs; i++) {
        const DrawSurf *drawSurf = drawSurfs[i];

        if (drawSurf->material->GetSort() != Material::Sort::OpaqueSort) {
            continue;
        }

        bool isDifferentObject = drawSurf->space != prevSpace;
        bool isDifferentMaterial = drawSurf->material != prevMaterial;
            
        if (isDifferentMaterial || isDifferentObject) {
            if (!(drawSurf->space->def->GetState().flags & RenderObject::OccluderFlag)) {
                continue;
            }

            /*if (surf->subMesh->GetAABB().Volume() < MeterToUnit(1) * MeterToUnit(1) * MeterToUnit(1)) {
                continue;
            }*/

            if (!prevMaterial) {
                backEnd.batch.Begin(Batch::OccluderFlush, drawSurf->material, drawSurf->materialRegisters, drawSurf->space);
            } else {
                if (isDifferentObject) {
                    backEnd.batch.Flush();
                    backEnd.batch.Begin(Batch::OccluderFlush, drawSurf->material, drawSurf->materialRegisters, drawSurf->space);
                }
            }

            prevMaterial = drawSurf->material;

            if (isDifferentObject) {
                bool depthHack = !!(drawSurf->space->def->GetState().flags & RenderObject::DepthHackFlag);

                if (prevDepthHack != depthHack) {
                    if (depthHack) {
                        rhi.SetDepthRange(0.0f, 0.1f);
                    } else {
                        rhi.SetDepthRange(0.0f, 1.0f);
                    }

                    prevDepthHack = depthHack;
                }

                backEnd.modelViewMatrix = drawSurf->space->modelViewMatrix;
                backEnd.modelViewProjMatrix = drawSurf->space->modelViewProjMatrix;

                prevSpace = drawSurf->space;
            }
        }

        backEnd.batch.DrawSubMesh(drawSurf->subMesh);
    }

    // Flush previous batch
    if (prevMaterial) {
        backEnd.batch.Flush();
    }

    // Restore depth hack
    if (prevDepthHack) {
        rhi.SetDepthRange(0.0f, 1.0f);
    }
}

void RB_DepthPrePass(int numDrawSurfs, DrawSurf **drawSurfs) {
    if (!r_useDepthPrePass.GetBool()) {
        return;
    }

    const VisObject *   prevSpace = nullptr;
    const SubMesh *     prevSubMesh = nullptr;
    const Material *    prevMaterial = nullptr;
    bool                prevDepthHack = false;

    backEnd.batch.SetCurrentLight(nullptr);

    for (int i = 0; i < numDrawSurfs; i++) {
        const DrawSurf *drawSurf = drawSurfs[i];

        if (drawSurf->material->GetSort() != Material::Sort::OpaqueSort) {
            continue;
        }

        bool isDifferentObject = drawSurf->space != prevSpace;
        bool isDifferentSubMesh = prevSubMesh ? !drawSurf->subMesh->IsShared(prevSubMesh) : true;
        bool isDifferentMaterial = drawSurf->material != prevMaterial;
        bool isDifferentInstance = !(drawSurf->flags & DrawSurf::UseInstancing) || isDifferentMaterial || isDifferentSubMesh || !prevSpace || 
            prevSpace->def->GetState().flags != drawSurf->space->def->GetState().flags || prevSpace->def->GetState().layer != drawSurf->space->def->GetState().layer ? true : false;

        if (isDifferentObject || isDifferentSubMesh || isDifferentMaterial) {
            if (prevMaterial && isDifferentInstance) {
                backEnd.batch.Flush();
            }

            backEnd.batch.Begin(Batch::DepthFlush, drawSurf->material, drawSurf->materialRegisters, drawSurf->space);

            prevSubMesh = drawSurf->subMesh;
            prevMaterial = drawSurf->material;

            if (isDifferentObject) {
                bool depthHack = !!(drawSurf->space->def->GetState().flags & RenderObject::DepthHackFlag);

                if (prevDepthHack != depthHack) {
                    if (drawSurf->flags & DrawSurf::UseInstancing) {
                        backEnd.batch.Flush();
                    }

                    if (depthHack) {
                        rhi.SetDepthRange(0.0f, 0.1f);
                    } else {
                        rhi.SetDepthRange(0.0f, 1.0f);
                    }

                    prevDepthHack = depthHack;
                }

                if (!(drawSurf->flags & DrawSurf::UseInstancing)) {
                    backEnd.modelViewMatrix = drawSurf->space->modelViewMatrix;
                    backEnd.modelViewProjMatrix = drawSurf->space->modelViewProjMatrix;
                }

                prevSpace = drawSurf->space;
            }
        }

        if (drawSurf->flags & DrawSurf::UseInstancing) {
            backEnd.batch.AddInstance(drawSurf);
        }

        backEnd.batch.DrawSubMesh(drawSurf->subMesh);
    }

    // Flush previous batch
    if (prevMaterial) {
        backEnd.batch.Flush();
    }
    
    // Restore depth hack
    if (prevDepthHack) {
        rhi.SetDepthRange(0.0f, 1.0f);
    }
}

void RB_UnlitPass(int numDrawSurfs, DrawSurf **drawSurfs) {
    if (r_skipBlendPass.GetBool()) {
        return;
    }

    const VisObject *   prevSpace = nullptr;
    const SubMesh *     prevSubMesh = nullptr;
    const Material *    prevMaterial = nullptr;
    bool                prevDepthHack = false;

    backEnd.batch.SetCurrentLight(nullptr);

    for (int i = 0; i < numDrawSurfs; i++) {
        const DrawSurf *drawSurf = drawSurfs[i];

        if (drawSurf->material->IsLitSurface() || drawSurf->material->IsSkySurface()) {
            continue;
        }

        bool isDifferentObject = drawSurf->space != prevSpace;
        bool isDifferentSubMesh = prevSubMesh ? !drawSurf->subMesh->IsShared(prevSubMesh) : true;
        bool isDifferentMaterial = drawSurf->material != prevMaterial;
        bool isDifferentInstance = !(drawSurf->flags & DrawSurf::UseInstancing) || isDifferentMaterial || isDifferentSubMesh || !prevSpace || 
            prevSpace->def->GetState().flags != drawSurf->space->def->GetState().flags || prevSpace->def->GetState().layer != drawSurf->space->def->GetState().layer ? true : false;

        if (isDifferentObject || isDifferentSubMesh || isDifferentMaterial) {
            if (prevMaterial && isDifferentInstance) {
                backEnd.batch.Flush();
            }

            backEnd.batch.Begin(Batch::UnlitFlush, drawSurf->material, drawSurf->materialRegisters, drawSurf->space);

            prevSubMesh = drawSurf->subMesh;
            prevMaterial = drawSurf->material;

            if (isDifferentObject) {
                bool depthHack = !!(drawSurf->space->def->GetState().flags & RenderObject::DepthHackFlag);

                if (prevDepthHack != depthHack) {
                    if (drawSurf->flags & DrawSurf::UseInstancing) {
                        backEnd.batch.Flush();
                    }

                    if (depthHack) {
                        rhi.SetDepthRange(0.0f, 0.1f);
                    } else {
                        rhi.SetDepthRange(0.0f, 1.0f);
                    }

                    prevDepthHack = depthHack;
                }

                if (!(drawSurf->flags & DrawSurf::UseInstancing)) {
                    backEnd.modelViewMatrix = drawSurf->space->modelViewMatrix;
                    backEnd.modelViewProjMatrix = drawSurf->space->modelViewProjMatrix;
                }

                prevSpace = drawSurf->space;
            }
        }

        if (drawSurf->flags & DrawSurf::UseInstancing) {
            backEnd.batch.AddInstance(drawSurf);
        }

        backEnd.batch.DrawSubMesh(drawSurf->subMesh);
    }

    // Flush previous batch
    if (prevMaterial) {
        backEnd.batch.Flush();
    }

    // Restore depth hack
    if (prevDepthHack) {
        rhi.SetDepthRange(0.0f, 1.0f);
    }
}

void RB_VelocityMapPass(int numDrawSurfs, DrawSurf **drawSurfs) {
    if ((backEnd.camera->def->GetState().flags & RenderCamera::SkipPostProcess) || !r_usePostProcessing.GetBool() || !(r_motionBlur.GetInteger() & 2)) {
        return;
    }

    const VisObject *   prevSpace = nullptr;
    const VisObject *   skipObject = nullptr;
    const Material *    prevMaterial = nullptr;
    bool                firstDraw = true;

    backEnd.batch.SetCurrentLight(nullptr);

    for (int i = 0; i < numDrawSurfs; i++) {
        const DrawSurf *drawSurf = drawSurfs[i];

        if (drawSurf->space == skipObject) {
            continue;
        }

        if (drawSurf->material->GetSort() == Material::Sort::SkySort) {
            continue;
        }

        bool isDifferentObject = drawSurf->space != prevSpace;
        bool isDifferentMaterial = drawSurf->material != prevMaterial;
            
        if (isDifferentMaterial || isDifferentObject) {
            if (!prevMaterial) {
                backEnd.batch.Begin(Batch::VelocityFlush, drawSurf->material, drawSurf->materialRegisters, drawSurf->space);
            } else {
                if (isDifferentObject || prevMaterial->GetCullType() != drawSurf->material->GetCullType() ||
                    (prevMaterial->GetSort() == Material::Sort::AlphaTestSort) || (drawSurf->material->GetSort() == Material::Sort::AlphaTestSort)) {
                    backEnd.batch.Flush();
                    backEnd.batch.Begin(Batch::VelocityFlush, drawSurf->material, drawSurf->materialRegisters, drawSurf->space);
                }
            }

            prevMaterial = drawSurf->material;

            if (isDifferentObject) {
                Mesh *mesh = drawSurf->space->def->GetState().mesh;

                if (!mesh->IsSkinnedMesh() && (drawSurf->space->def->GetObjectToWorldMatrix() == drawSurf->space->def->GetPrevObjectToWorldMatrix())) {
                    skipObject = drawSurf->space;
                    continue;
                }

                skipObject = nullptr;

                backEnd.modelViewMatrix = drawSurf->space->modelViewMatrix;
                backEnd.modelViewProjMatrix = drawSurf->space->modelViewProjMatrix;

                prevSpace = drawSurf->space;
            }
        }

        if (firstDraw) {
            firstDraw = false;

            backEnd.ctx->ppRTs[PP_RT_VEL]->Begin();

            rhi.SetViewport(Rect(0, 0, backEnd.ctx->ppRTs[PP_RT_VEL]->GetWidth(), backEnd.ctx->ppRTs[PP_RT_VEL]->GetHeight()));
            rhi.SetStateBits(RHI::ColorWrite | RHI::AlphaWrite | RHI::DepthWrite);
            rhi.Clear(RHI::ColorBit | RHI::DepthBit, Color4(0.0f, 0.0f, 0.0f, 1.0f), 1.0f, 0);
        }

        backEnd.batch.DrawSubMesh(drawSurf->subMesh);
    }

    if (!firstDraw) {
        backEnd.batch.Flush();

        backEnd.ctx->ppRTs[PP_RT_VEL]->End();
        
        rhi.SetViewport(backEnd.renderRect);
        rhi.SetScissor(backEnd.renderRect);
    } else {
        firstDraw = false;

        backEnd.ctx->ppRTs[PP_RT_VEL]->Begin();

        rhi.SetViewport(Rect(0, 0, backEnd.ctx->ppRTs[PP_RT_VEL]->GetWidth(), backEnd.ctx->ppRTs[PP_RT_VEL]->GetHeight()));
        rhi.SetStateBits(RHI::ColorWrite | RHI::AlphaWrite | RHI::DepthWrite);
        rhi.Clear(RHI::ColorBit | RHI::DepthBit, Color4(0.0f, 0.0f, 0.0f, 1.0f), 1.0f, 0);
    
        backEnd.ctx->ppRTs[PP_RT_VEL]->End();

        rhi.SetViewport(backEnd.renderRect);
        rhi.SetScissor(backEnd.renderRect);
    }
}

void RB_FinalPass(int numDrawSurfs, DrawSurf **drawSurfs) {
    if (r_skipFinalPass.GetBool()) {
        return;
    }

    const VisObject *   prevSpace = nullptr;
    const SubMesh *     prevSubMesh = nullptr;
    const Material *    prevMaterial = nullptr;
    bool                prevDepthHack = false;

    backEnd.batch.SetCurrentLight(nullptr);
    
    if (backEnd.ctx->updateCurrentRenderTexture) {
        backEnd.ctx->updateCurrentRenderTexture = false;
        backEnd.ctx->UpdateCurrentRenderTexture();
    }
        
    for (int i = 0; i < numDrawSurfs; i++) {
        const DrawSurf *drawSurf = drawSurfs[i];
        
        if (drawSurf->material->GetSort() == Material::Sort::SkySort) {
            continue;
        }

        if (1) {//!surf->material->HasRefraction()) {
            continue;
        }

        bool isDifferentObject = drawSurf->space != prevSpace;
        bool isDifferentSubMesh = prevSubMesh ? !drawSurf->subMesh->IsShared(prevSubMesh) : true;
        bool isDifferentMaterial = drawSurf->material != prevMaterial;
        bool isDifferentInstance = !(drawSurf->flags & DrawSurf::UseInstancing) || isDifferentMaterial || isDifferentSubMesh || !prevSpace || 
            prevSpace->def->GetState().flags != drawSurf->space->def->GetState().flags || prevSpace->def->GetState().layer != drawSurf->space->def->GetState().layer ? true : false;

        if (isDifferentObject || isDifferentSubMesh || isDifferentMaterial) {
            if (prevMaterial && isDifferentInstance) {
                backEnd.batch.Flush();
            }

            backEnd.batch.Begin(Batch::FinalFlush, drawSurf->material, drawSurf->materialRegisters, drawSurf->space);

            prevSubMesh = drawSurf->subMesh;
            prevMaterial = drawSurf->material;

            if (isDifferentObject) {
                bool depthHack = !!(drawSurf->space->def->GetState().flags & RenderObject::DepthHackFlag);

                if (prevDepthHack != depthHack) {
                    if (drawSurf->flags & DrawSurf::UseInstancing) {
                        backEnd.batch.Flush();
                    }

                    if (depthHack) {
                        rhi.SetDepthRange(0.0f, 0.1f);
                    } else {
                        rhi.SetDepthRange(0.0f, 1.0f);
                    }

                    prevDepthHack = depthHack;
                }

                if (!(drawSurf->flags & DrawSurf::UseInstancing)) {
                    backEnd.modelViewMatrix = drawSurf->space->modelViewMatrix;
                    backEnd.modelViewProjMatrix = drawSurf->space->modelViewProjMatrix;
                }

                prevSpace = drawSurf->space;
            }
        }

        if (drawSurf->flags & DrawSurf::UseInstancing) {
            backEnd.batch.AddInstance(drawSurf);
        }

        backEnd.batch.DrawSubMesh(drawSurf->subMesh);
    }

    // Flush previous batch
    if (prevMaterial) {
        backEnd.batch.Flush();
    }

    // Restore depth hack
    if (prevDepthHack) {
        rhi.SetDepthRange(0.0f, 1.0f);
    }
}

void RB_GuiPass(int numDrawSurfs, DrawSurf **drawSurfs) {
    const VisObject *   prevSpace = nullptr;
    const Material *    prevMaterial = nullptr;
    bool                prevDepthHack = false;

    backEnd.batch.SetCurrentLight(nullptr);
        
    for (int i = 0; i < numDrawSurfs; i++) {
        const DrawSurf *drawSurf = drawSurfs[i];

        bool isDifferentObject = drawSurf->space != prevSpace;
        bool isDifferentMaterial = drawSurf->material != prevMaterial;

        if (isDifferentMaterial || isDifferentObject) {
            if (prevMaterial) {
                backEnd.batch.Flush();
            }

            backEnd.batch.Begin(Batch::GuiFlush, drawSurf->material, drawSurf->materialRegisters, drawSurf->space);

            prevMaterial = drawSurf->material;

            if (isDifferentObject) {
                bool depthHack = !!(drawSurf->space->def->GetState().flags & RenderObject::DepthHackFlag);

                if (prevDepthHack != depthHack) {
                    if (depthHack) {
                        rhi.SetDepthRange(0.0f, 0.1f);
                    } else {
                        rhi.SetDepthRange(0.0f, 1.0f);
                    }

                    prevDepthHack = depthHack;
                }

                backEnd.modelViewMatrix = drawSurf->space->modelViewMatrix;
                backEnd.modelViewProjMatrix = drawSurf->space->modelViewProjMatrix;

                prevSpace = drawSurf->space;
            }
        }
        
        backEnd.batch.DrawSubMesh(drawSurf->subMesh);
    }

    // Flush previous batch
    if (prevMaterial) {
        backEnd.batch.Flush();
    }

    // Restore depth hack
    if (prevDepthHack) {
        rhi.SetDepthRange(0.0f, 1.0f);
    }
}

BE_NAMESPACE_END
