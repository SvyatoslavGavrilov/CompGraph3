//***************************************************************************************
// Atmosphere.hlsl - Dual-mode atmospheric scattering implementation
// Supports Hoffman-Preetham (ground level) and Ray Marching (high altitude) approaches
//***************************************************************************************

cbuffer AtmosphereParams : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float3 CameraPos;
    float padding0;
    float3 SunDirection;
    float padding1;
    float3 PlanetCenter;
    float AtmosphereRadius;
    float PlanetRadius;
    float padding2;
    float3 RayleighScattering;
    float padding3;
    float3 MieScattering;
    float MieG;
    float SunIntensity;
    int AtmosphereMode; // 0 = Hoffman-Preetham, 1 = Ray Marching
    float DensityMultiplier;
    float PollutionLevel;
    float padding4;
};

struct VS_INPUT
{
    float3 Position : POSITION;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
};

// Common functions for both methods
float3 getPrimaryRayDirection(float2 uv)
{
    // Reconstruct view direction from screen UV
    float3 viewRay = normalize(mul(float4(uv, 1.0, 0.0), View).xyz);
    return viewRay;
}

// Hoffman-Preetham Approach (Ground Level)
PS_INPUT VS_HoffmanPreetham(VS_INPUT input)
{
    PS_INPUT output;
    float4x4 viewProj = mul(View, Projection);
    output.Position = mul(float4(input.Position, 1.0), viewProj);
    output.WorldPos = input.Position;
    output.ViewDir = normalize(input.Position - CameraPos);
    return output;
}

float4 PS_HoffmanPreetham(PS_INPUT input) : SV_TARGET
{
    // Apply pollution to scattering coefficients
    float3 rayleigh = RayleighScattering * (1.0 + PollutionLevel * 2.0);
    float3 mie = MieScattering * (1.0 + PollutionLevel * 4.0);
    
    // Hoffman-Preetham calculation
    float3 viewDir = normalize(input.ViewDir);
    float cosTheta = dot(viewDir, normalize(SunDirection));
    
    // Extinction factor (Ft)
    float opticalDepth = exp(-length(input.WorldPos - PlanetCenter) / 8000.0);
    float3 extinction = exp(-(rayleigh + mie) * opticalDepth * DensityMultiplier);
    
    // Sky color calculation
    float3 skyColor = float3(0, 0, 0);
    
    // Rayleigh scattering contribution
    float rayleighPhase = 0.75 * (1.0 + cosTheta * cosTheta);
    skyColor += rayleigh * rayleighPhase * SunIntensity;
    
    // Mie scattering contribution (forward scattering)
    float g2 = MieG * MieG;
    float miePhase = 0.75 * ((1.0 - g2) / pow(1.0 + g2 - 2.0 * MieG * cosTheta, 1.5));
    skyColor += mie * miePhase * SunIntensity;
    
    // Apply extinction
    skyColor *= extinction;
    
    return float4(skyColor, 1.0);
}

// Ray Marching Approach (High Altitude)
PS_INPUT VS_RayMarching(VS_INPUT input)
{
    PS_INPUT output;
    float4x4 viewProj = mul(View, Projection);
    output.Position = mul(float4(input.Position, 1.0), viewProj);
    output.WorldPos = input.Position;
    output.ViewDir = normalize(input.Position - CameraPos);
    return output;
}

// Phase function for Rayleigh scattering
float getRayleighPhase(float cosTheta)
{
    return 0.75 * (1.0 + cosTheta * cosTheta);
}

// Phase function for Mie scattering (Henyey-Greenstein)
float getMiePhase(float cosTheta, float g)
{
    float g2 = g * g;
    return 0.75 * ((1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

// Scale function for optical depth approximation
float scale(float cosTheta)
{
    float x = 1.0 - cosTheta;
    return exp(-0.00287 + x * (0.459 + x * (3.83 + x * (-6.80 + x * 5.25))));
}

float4 PS_RayMarching(PS_INPUT input) : SV_TARGET
{
    // Apply pollution to scattering coefficients
    float3 rayleigh = RayleighScattering * (1.0 + PollutionLevel * 2.0);
    float3 mie = MieScattering * (1.0 + PollutionLevel * 4.0);
    
    float3 viewDir = normalize(input.ViewDir);
    float3 startPos = CameraPos;
    float3 endPos = input.WorldPos;
    
    // Ray marching parameters
    int sampleCount = 32;
    float stepSize = length(endPos - startPos) / sampleCount;
    
    float3 totalScattering = float3(0, 0, 0);
    float opticalDepthR = 0.0;
    float opticalDepthM = 0.0;
    
    float3 currentPos = startPos;
    
    for (int i = 0; i < sampleCount; i++)
    {
        float height = length(currentPos - PlanetCenter);
        float density = exp(-(height - PlanetRadius) / 8000.0) * DensityMultiplier;
        
        // Sample light contribution
        float3 lightDir = normalize(SunDirection);
        float cosTheta = dot(viewDir, lightDir);
        
        // Optical depth calculation
        float sampleDistance = stepSize * density;
        opticalDepthR += sampleDistance * length(rayleigh);
        opticalDepthM += sampleDistance * length(mie);
        
        // Scattering calculation
        float3 tau = rayleigh * opticalDepthR + mie * opticalDepthM;
        float3 attenuation = exp(-tau);
        
        // Phase functions
        float rayleighPhase = getRayleighPhase(cosTheta);
        float miePhase = getMiePhase(cosTheta, MieG);
        
        // Add scattering contribution
        totalScattering += attenuation * (rayleigh * rayleighPhase + mie * miePhase) * density * stepSize;
        
        currentPos += viewDir * stepSize;
    }
    
    // Apply sun intensity
    totalScattering *= SunIntensity;
    
    return float4(totalScattering, 1.0);
}

// Main shader entry point
PS_INPUT VS_Main(VS_INPUT input)
{
    if (AtmosphereMode == 0)
        return VS_HoffmanPreetham(input);
    else
        return VS_RayMarching(input);
}

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    if (AtmosphereMode == 0)
        return PS_HoffmanPreetham(input);
    else
        return PS_RayMarching(input);
}

