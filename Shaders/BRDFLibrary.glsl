#ifndef BRDF_LIBRARY_INCLUDED
#define BRDF_LIBRARY_INCLUDED

#if defined(GL_ES)
// min roughness such that (MIN_ROUGHNESS^4) > 0 in fp16 (i.e. 2^(-14/4), slightly rounded up)
#define MIN_ROUGHNESS           0.089
#define MIN_LINEAR_ROUGHNESS    0.007921
#else
#define MIN_ROUGHNESS           0.045
#define MIN_LINEAR_ROUGHNESS    0.002025
#endif

// Clear coat layers are almost always glossy.
#define MIN_CLEARCOAT_ROUGHNESS MIN_ROUGHNESS
#define MAX_CLEARCOAT_ROUGHNESS 0.6

// Convert perceptual glossiness to specular power from [0, 1] to [2, 8192]
float GlossinessToSpecularPower(float glossiness) {
    return exp2(11.0 * glossiness + 1.0); 
}

// Anisotropic parameters: roughnessT and roughnessB are the roughness along the tangent and bitangent.
// to simplify materials, we derive them from a single roughness parameter.
// Kulla 2017, "Revisiting Physically Based Shading at Imageworks"
void RoughnessToAnisotropyRoughness(float anisotropy, float linearRoughness, out float roughnessT, out float roughnessB) {
    roughnessT = max(linearRoughness * (1.0 + anisotropy), MIN_LINEAR_ROUGHNESS);
    roughnessB = max(linearRoughness * (1.0 - anisotropy), MIN_LINEAR_ROUGHNESS);
}

//---------------------------------------------------
// diffuse BRDF function
//---------------------------------------------------

float Fd_Lambert() {
    return INV_PI;
}

// Energy conserving wrap diffuse term, does *not* include the divide by PI
float Fd_Wrap(float NdotL, float w) {
    float onePlusW = 1.0 + w;
    return saturate((NdotL + w) / (onePlusW * onePlusW));
}

// Disney diffuse
float Fd_Burley(float NdotL, float NdotV, float VdotH, float linearRoughness) {
#if 1
    // renormalized version
    float energyBias = mix(0.0, 0.5, linearRoughness);
    float energyFactor = mix(1.0, 1.0 / 1.51, linearRoughness);
    float F90 = energyBias + 2.0 * VdotH * VdotH * linearRoughness;
    float FdL = 1.0 + (F90 - 1.0) * pow5(1.0 - NdotL);
    float FdV = 1.0 + (F90 - 1.0) * pow5(1.0 - NdotV);
    return FdL * FdV * energyFactor * INV_PI;
#else
    float F90 = 0.5 + 2.0 * VdotH * VdotH * linearRoughness;
    float FdL = 1.0 + (F90 - 1.0) * pow5(1.0 - NdotL);
    float FdV = 1.0 + (F90 - 1.0) * pow5(1.0 - NdotV);
    return FdL * FdV * INV_PI;
#endif
}

float Fd_OrenNayar(float NdotL, float NdotV, float LdotV, float linearRoughness) {
    float sigma2 = linearRoughness * linearRoughness;
    float A = 1.0 - sigma2 * (0.5 / (sigma2 + 0.33) + 0.17 / (sigma2 + 0.13));
    float B = 0.45 * sigma2 / (sigma2  + 0.09);

    // A tiny improvement of Oren-Nayar [Yasuhiro Fujii]
    // http://mimosa-pudica.net/improved-oren-nayar.html
    //float A = 1.0 / (PI + 0.90412966 * roughness);
    //float B = roughness / (PI + 0.90412966 * roughness);

    float s = LdotV - NdotL * NdotV;
    float t = mix(1.0, max(NdotL, NdotV), step(0.0, s));
    
    return (A + B * s / t) * INV_PI;
}

//---------------------------------------------------
// Normal distribution functions
//---------------------------------------------------

float D_Blinn(float NdotH, float linearRoughness) {
    float a2 = linearRoughness * linearRoughness;
    float n = 2.0 / a2 - 2.0;
    n = max(n, 1e-4); // prevent possible zero
    return pow(NdotH, n) * (n + 2.0) * INV_TWO_PI;
}

float D_Beckmann(float NdotH, float linearRoughness) {
    float a2 = linearRoughness * linearRoughness;
    float NdotH2 = NdotH * NdotH;
    return exp((NdotH2 - 1.0) / (a2 * NdotH2)) * INV_PI / (a2 * NdotH2 * NdotH2);
}

// Trowbridge-Reitz aka GGX
float D_GGX(float NdotH, float linearRoughness) {
    HIGHP float a2 = linearRoughness * linearRoughness;
    HIGHP float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 * INV_PI / (denom * denom + 1e-7);
}

// Anisotropic GGX
// Burley 2012, "Physically-Based Shading at Disney"
float D_GGXAniso(float NdotH, float TdotH, float BdotH, float roughnessT, float roughnessB) {
    // The values roughnessT and roughnessB are roughness^2, a2 is therefore roughness^4
    // The dot product below computes roughness^8. We cannot fit in fp16 without clamping
    // the roughness to too high values so we perform the dot product and the division in fp32
#if 0
    // scalar mul: 9
    // scalar div: 3
    float ax2 = roughnessT * roughnessT;
    float ay2 = roughnessB * roughnessB;
    HIGHP float denom = TdotH * TdotH / ax2 + BdotH * BdotH / ay2 + NdotH * NdotH;
    return 1.0 / (PI * roughnessT * roughnessB * denom * denom);
#else
    // scalar mul: 7
    // scalar div: 1
    // vec3 dot: 1
    float a2 = roughnessT * roughnessB;
    HIGHP vec3 v = vec3(roughnessB * TdotH, roughnessT * BdotH, a2 * NdotH);
    HIGHP float v2 = dot(v, v);
    float a2_v2 = a2 / v2;
    return INV_PI * a2 * a2_v2 * a2_v2;
#endif
}

//---------------------------------------------------
// Geometric visibility functions - divided by (4 * NdotL * NdotV)
//---------------------------------------------------

float G_Implicit() {
    return 0.25;
}

float G_Neumann(float NdotV, float NdotL) {
    return 0.25 / (max(NdotL, NdotV));
}

// Kelemen 2001, "A Microfacet Based Coupled Specular-Matte BRDF Model with Importance Sampling"
float G_Kelemen(float VdotH) {
    return 0.25 / (VdotH * VdotH);
}

float G_CookTorrance(float NdotV, float NdotL, float NdotH, float VdotH) {
    float k = 2.0 * NdotH / VdotH;
    float G = min(1.0, min(k * NdotV, k * NdotL));
    return G / (4.0 * NdotL * NdotV);
}

// k_direct = (roughness + 1)^2 / 8
// k_indirect = roughness^2 / 2
float G_SchlickGGX(float NdotV, float NdotL, float k) {
    float oneMinusK = 1.0 - k;
    vec2 G = vec2(NdotV, NdotL) * oneMinusK + k;
    return 0.25 / (G.x * G.y);
}

// Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
float G_SmithGGXCorrelated(float NdotV, float NdotL, float linearRoughness) {
    float a2 = linearRoughness * linearRoughness;
    // TODO: lambdaV can be pre-computed for all the lights, it should be moved out of this function
    float lambdaV = NdotL * sqrt((NdotV - a2 * NdotV) * NdotV + a2);
    float lambdaL = NdotV * sqrt((NdotL - a2 * NdotL) * NdotL + a2);
    // a2 = 0 => v = 1 / (4 * NdotL * NdotV)   => min = 1/4, max = +inf
    // a2 = 1 => v = 1 / (2 * (NdotL + NdotV)) => min = 1/4, max = +inf
    return 0.5 / (lambdaV + lambdaL);
}

// Hammon 2017, "PBR Diffuse Lighting for GGX+Smith Microsurfaces"
float G_SmithGGXCorrelatedFast(float NdotV, float NdotL, float linearRoughness) {
    return 0.5 / mix(2.0 * NdotL * NdotV, NdotL + NdotV, linearRoughness);
}

// Smith Joint GGX Anisotropic Visibility
// Taken from https://cedec.cesa.or.jp/2015/session/ENG/14698.html
float G_SmithGGXCorrelatedAniso(float NdotV, float NdotL, float TdotV, float BdotV, float TdotL, float BdotL, float roughnessT, float roughnessB) {
    float lambdaV = NdotL * length(vec3(roughnessT * TdotV, roughnessB * BdotV, NdotV));
    float lambdaL = NdotV * length(vec3(roughnessT * TdotL, roughnessB * BdotL, NdotL));
    return 0.5 / (lambdaV + lambdaL);
}

//---------------------------------------------------
// Fresnel functions
//---------------------------------------------------

float F_SecondFactorSchlick(float cosTheta) {
    return pow5(1.0 - cosTheta);
}

// Fresnel using Schlick's approximation
vec3 F_Schlick(vec3 F0, float cosTheta) {
    return F0 + (vec3(1.0) - F0) * F_SecondFactorSchlick(cosTheta);
}

float F_SecondFactorSchlickSG(float cosTheta) {
    return exp2((-5.55473 * cosTheta - 6.98316) * cosTheta);
}

// Fresnel using Schlick's approximation with spherical Gaussian approximation
vec3 F_SchlickSG(vec3 F0, float cosTheta) {
    return F0 + (vec3(1.0) - F0) * F_SecondFactorSchlickSG(cosTheta);
}

// Fresnel injected roughness term
vec3 F_SchlickRoughness(vec3 F0, float roughness, float cosTheta) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * F_SecondFactorSchlick(cosTheta);
}

//---------------------------------------------------
// IOR
//---------------------------------------------------

float IorToF0(float transmittedIor, float incidentIor) {
    float r = (transmittedIor - incidentIor) / (transmittedIor + incidentIor);
    return r * r;
}

float IorToF0(float transmittedIor) {
    float r = (transmittedIor - 1.0) / (transmittedIor + 1.0);
    return r * r;
}

float F0ToIor(float f0) {
    float r = sqrt(f0);
    return (1.0 + r) / (1.0 - r);
}

vec3 F0ForAirInterfaceToF0ForClearCoat15(vec3 f0) {
    // Approximation of IorToF0(F0ToIor(f0), 1.5)
    // This assumes that the clear coat layer has an IOR of 1.5
#if defined(GL_ES)
    return saturate(f0 * (f0 * 0.526868 + 0.529324) - 0.0482256);
#else
    return saturate(f0 * (f0 * (0.941892 - 0.263008 * f0) + 0.346479) - 0.0285998);
#endif
}

vec2 GetPrefilteredDFG(float NdotV, float roughness) {
#if 0
    // Zioma's approximation based on Karis
    return vec2(1.0, pow(1.0 - max(roughness, NdotV), 3.0));
#else
    // Karis' approximation based on Lazarov's
    // https://www.unrealengine.com/blog/physically-based-shading-on-mobile
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572,  0.022);
    const vec4 c1 = vec4( 1.0,  0.0425,  1.040, -0.040);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
#endif
}

#endif
