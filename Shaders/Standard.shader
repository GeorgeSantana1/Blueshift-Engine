shader "Lit/Standard" {
    litSurface
    properties {
        _ALBEDO("Albedo") : enum "Color;Texture" = "0" (shaderDefine)
        albedoColor("Albedo Color") : color3 = "1 1 1"
        albedoAlpha("Albedo Alpha") : float range 0 1.0 0.001 = "1"
        albedoMap("Albedo Map") : texture = "_whiteTexture"
        _METALLIC("Metallic") : enum "Scale;Texture (R);Texture (G);Texture (B);Texture (A)" = "0" (shaderDefine)
        metallicMap("Metallic Map") : texture = "_whiteTexture"
        metallicScale("Metallic Scale") : float range 0 1.0 0.01 = "1.0"
        _ROUGHNESS("Roughness") : enum "Scale;Texture (R);Inverted Texture (R);From Metallic Map (R);From Metallic Map (G);From Metallic Map (B);From Metallic Map (A)" = "0" (shaderDefine)
        roughnessMap("Roughness Map") : texture = "_whiteTexture"
        roughnessScale("Roughness Scale") : float range 0 1.0 0.01 = "1.0"
        _NORMAL("Normal") : enum "Vertex;Texture;Texture + Detail Texture" = "0" (shaderDefine)
        normalMap("Normal Map") : texture = "_flatNormalTexture"
        detailNormalMap("Detail Normal Map") : texture = "_flatNormalTexture"
        detailRepeat("Detail Repeat") : float = "8"
        /*_ANISO("Anisotropy") : enum "None;Value;Texture" = "0" (shaderDefine)
        anisotropyMap("Anisotropy Map") : texture = "_flatNormalTexture"
        anisotropy("Anisotropy") : float range -1.0 1.0 0.01 = "0.0"*/
        _CLEARCOAT("Clear Coat") : enum "None;Scale;Texture (R);From Metallic Map (R);From Metallic Map (G);From Metallic Map (B);From Metallic Map (A)" = "0" (shaderDefine)
        clearCoatMap("Clear Coat Map") : texture = "_whiteTexture"
        clearCoatScale("Clear Coat Scale") : float range 0 1.0 0.01 = "1.0"
        _CC_ROUGHNESS("Clear Coat Roughness") : enum "Scale;Texture (R);From Metallic Map (R);From Metallic Map (G);From Metallic Map (B);From Metallic Map (A);" = "0" (shaderDefine)
        clearCoatRoughnessMap("Clear Coat Roughness Map") : texture = "_whiteTexture"
        clearCoatRoughnessScale("Clear Coat Roughness Scale") : float range 0 1.0 0.01 = "0.0"
        _CC_NORMAL("Clear Coat Normal") : enum "Surface Normal;Texture" = "0" (shaderDefine)
        clearCoatNormalMap("Clear Coat Normal Map") : texture = "_flatNormalTexture"
        _PARALLAX("Parallax") : enum "None;Texture (R)" = "0" (shaderDefine)
        heightMap("Height Map") : texture = "_whiteTexture"
        heightScale("Height Scale") : float range 0.01 0.1 0.001 = "1.0"
        _OCC("Occlusion") : enum "None;Texture (R);From Metallic Map (R);From Metallic Map (G);From Metallic Map (B);From Metallic Map (A)" = "0" (shaderDefine)
        occlusionMap("Occlusion Map") : texture = "_whiteTexture"
        occlusionStrength("Occlusion Strength") : float range 0 1 0.001 = "1"
        _EMISSION("Emission") : enum "None;Color;Texture" = "0" (shaderDefine)
        emissionColor("Emission Color") : color3 = "1 1 1"
        emissionMap("Emission Map") : texture = "_blackTexture"
        emissionScale("Emission Scale") : float range 0 16 0.001 = "1"
    }

    generatePerforatedVersion
    generatePremulAlphaVersion
    generateGpuSkinningVersion
    generateGpuInstancingVersion

    ambientLitVersion "StandardAmbientLit"
    directLitVersion "StandardDirectLit"
    ambientLitDirectLitVersion "StandardAmbientLitDirectLit"

    glsl_vp {
        $include "StandardCore.vp"
    }
    glsl_fp {
        $include "StandardCore.fp"
    }
}
