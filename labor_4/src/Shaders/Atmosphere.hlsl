//***************************************************************************************
// Atmosphere.hlsl - Dual-mode atmospheric scattering implementation
// Supports Hoffman-Preetham (ground level) and Ray Marching (high altitude) approaches
//***************************************************************************************

cbuffer AtmosphereParams : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float3 CameraPos;
    float CameraAltitudeDisplacement; // Artificial altitude offset for better atmospheric calculations
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
    float SunAngularRadius; // Angular radius of sun disk
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
    // View direction: from camera to point on sky dome
    // For sky dome rendering, this gives us the direction we're looking
    float3 toVertex = input.Position - CameraPos;
    output.ViewDir = normalize(toVertex);
    return output;
}

float4 PS_HoffmanPreetham(PS_INPUT input) : SV_TARGET
{
    // Apply pollution to scattering coefficients
    // Pollution increases all scattering, but we want to preserve blue dominance
    float pollutionFactor = 1.0 + PollutionLevel * 1.5; // Reduced pollution impact
    float3 rayleigh = RayleighScattering * pollutionFactor;
    // Mie scattering (haze) increases more with pollution
    float3 mie = MieScattering * (1.0 + PollutionLevel * 3.0);
    
    // Hoffman-Preetham calculation
    float3 viewDir = normalize(input.ViewDir);
    // SunDirection points FROM sun TO planet, so we negate it to get direction TO sun
    float3 sunDir = normalize(-SunDirection);
    float cosTheta = dot(viewDir, sunDir);
    
    // Calculate view and sun elevation for sky gradient
    float3 up = float3(0, 1, 0);
    float viewElevation = dot(viewDir, up); // -1 = down, 0 = horizon, 1 = up
    float sunElevation = dot(sunDir, up);
    
    // [[Sky-background]] Calculate base sky color gradient (horizon to zenith)
    // Real atmosphere: horizon is lighter blue/white, zenith is deep blue
    // RGB values for realistic sky colors (no green tint)
    float3 horizonColor = float3(0.7, 0.8, 1.0); // Light blue/white at horizon (realistic)
    float3 zenithColor = float3(0.15, 0.25, 0.5);  // Deep blue at zenith (realistic)
    
    // Convert view elevation from [-1, 1] to [0, 1] for interpolation
    // 0 = looking down, 0.5 = horizon, 1 = looking up
    float elevationFactor = saturate((viewElevation + 1.0) * 0.5);
    elevationFactor = pow(elevationFactor, 0.7); // Slight curve for more natural gradient
    
    // Base sky color from gradient
    float3 baseSkyColor = lerp(horizonColor, zenithColor, elevationFactor);
    
    // Extinction factor (Ft) - for sky dome, use simplified calculation
    // Sky dome is at fixed radius, so we use a constant optical depth based on height above ground
    // For sky dome rendering, we want minimal extinction (sky should be bright)
    float heightAboveGround = max(0.0, length(input.WorldPos - PlanetCenter) - PlanetRadius);
    float opticalDepth = exp(-heightAboveGround / 8000.0);
    float3 extinction = exp(-(rayleigh + mie) * opticalDepth * DensityMultiplier * 0.1); // Reduced extinction for sky
    
    // Sky color calculation - start with darker base so sun intensity has more effect
    float3 skyColor = baseSkyColor * 0.3; // Darker base so sun intensity is more visible
    
    // Rayleigh scattering contribution (blue sky)
    // This is what makes the sky blue - blue light scatters more than red/green
    // SunIntensity now has much more influence (multiplier increased)
    float rayleighPhase = 0.75 * (1.0 + cosTheta * cosTheta);
    float3 rayleighContribution = rayleigh * rayleighPhase * SunIntensity * 2.0; // Increased from 0.6 to 2.0
    skyColor += rayleighContribution;
    
    // Mie scattering contribution (forward scattering - hazy glow)
    // Mie scattering is more uniform (white), adds haze but shouldn't dominate
    float g2 = MieG * MieG;
    float miePhase = 0.75 * ((1.0 - g2) / pow(1.0 + g2 - 2.0 * MieG * cosTheta, 1.5));
    float3 mieContribution = mie * miePhase * SunIntensity * 1.0; // Increased from 0.2 to 1.0
    skyColor += mieContribution;
    
    // [[Sun-disk]] Add visible sun disk
    // Use larger sun angular radius for better visibility
    // Default to ~2 degrees (0.035 radians) instead of 0.27 degrees
    float sunAngularRadius = max(SunAngularRadius, 0.035); // ~2 degrees for better visibility
    float sunCosAngle = cos(sunAngularRadius);
    
    // Check if view direction is close to sun direction
    // Use a smoother falloff for the sun disk
    float sunProximity = saturate((cosTheta - sunCosAngle) / (1.0 - sunCosAngle + 0.01));
    if (sunProximity > 0.0)
    {
        // Calculate sun disk intensity (smooth falloff)
        float sunDiskIntensity = smoothstep(0.0, 1.0, sunProximity);
        
        // Sun color (bright white/yellow) - SunIntensity has direct influence
        float3 sunColor = float3(1.0, 0.95, 0.8) * SunIntensity * 5.0; // Increased multiplier from 2.0 to 5.0
        
        // Blend sun disk with sky color - more aggressive blending
        skyColor = lerp(skyColor, sunColor, sunDiskIntensity * 0.9);
    }
    
    // Sunset/sunrise enhancement - when sun is near horizon
    if (sunElevation < 0.3 && sunElevation > -0.1)
    {
        float sunsetFactor = 1.0 - saturate((sunElevation + 0.1) / 0.4);
        float3 sunsetColor = float3(1.0, 0.6, 0.3) * sunsetFactor;
        
        // Add sunset color near sun direction
        float sunProximity = max(0.0, cosTheta);
        skyColor += sunsetColor * sunProximity * sunsetFactor * 0.5;
    }
    
    // Apply extinction (but ensure minimum brightness)
    skyColor *= extinction;
    
    // Ensure minimum brightness so sky is never completely black
    float3 minSkyColor = baseSkyColor * 0.1; // Minimum 10% of base sky color
    skyColor = max(skyColor, minSkyColor);
    
    return float4(skyColor, 1.0);
}

// Ray Marching Approach (High Altitude)
PS_INPUT VS_RayMarching(VS_INPUT input)
{
    PS_INPUT output;
    float4x4 viewProj = mul(View, Projection);
    output.Position = mul(float4(input.Position, 1.0), viewProj);
    output.WorldPos = input.Position;
    // View direction: from camera to point on sky dome
    float3 toVertex = input.Position - CameraPos;
    output.ViewDir = normalize(toVertex);
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
    // Pollution increases all scattering, but we want to preserve blue dominance
    float pollutionFactor = 1.0 + PollutionLevel * 1.5; // Reduced pollution impact
    float3 rayleigh = RayleighScattering * pollutionFactor;
    // Mie scattering (haze) increases more with pollution
    float3 mie = MieScattering * (1.0 + PollutionLevel * 3.0);
    
    float3 viewDir = normalize(input.ViewDir);
    // SunDirection points FROM sun TO planet, so we negate it to get direction TO sun
    float3 sunDir = normalize(-SunDirection);
    float3 startPos = CameraPos;
    float3 endPos = input.WorldPos;
    
    // Calculate view and sun elevation for sky gradient
    float3 up = float3(0, 1, 0);
    float viewElevation = dot(viewDir, up);
    float sunElevation = dot(sunDir, up);
    
    // [[Sky-background]] Calculate base sky color gradient (horizon to zenith)
    // Real atmosphere: horizon is lighter blue/white, zenith is deep blue
    float3 horizonColor = float3(0.7, 0.8, 1.0); // Light blue/white at horizon (realistic)
    float3 zenithColor = float3(0.15, 0.25, 0.5);  // Deep blue at zenith (realistic)
    
    // Convert view elevation for interpolation
    float elevationFactor = saturate((viewElevation + 1.0) * 0.5);
    elevationFactor = pow(elevationFactor, 0.7); // Slight curve for more natural gradient
    
    // Base sky color from gradient
    float3 baseSkyColor = lerp(horizonColor, zenithColor, elevationFactor);
    
    // Ray marching parameters (volumetric scattering)
    int sampleCount = 32;
    float stepSize = length(endPos - startPos) / sampleCount;
    
    float3 totalScattering = baseSkyColor * 0.6; // Start with brighter base sky color
    float opticalDepthR = 0.0;
    float opticalDepthM = 0.0;
    
    float3 currentPos = startPos;
    float cosTheta = dot(viewDir, sunDir);
    
    // [[Volumetric-scattering]] Ray march through atmosphere
    for (int i = 0; i < sampleCount; i++)
    {
        float height = length(currentPos - PlanetCenter);
        
        // Density calculation based on height (atmospheric density decreases with altitude)
        float relativeHeight = (height - PlanetRadius) / (AtmosphereRadius - PlanetRadius);
        float density = exp(-relativeHeight * 10.0) * DensityMultiplier;
        density = max(0.0, density); // Clamp to avoid negative values
        
        // Calculate optical depth along view ray
        float sampleDistance = stepSize;
        opticalDepthR += sampleDistance * density * length(rayleigh);
        opticalDepthM += sampleDistance * density * length(mie);
        
        // Calculate light scattering from sun to this point
        // sunDir already points TO sun (we negated it above)
        float3 toSun = sunDir;
        float sunDistance = (AtmosphereRadius - height) / max(0.001, dot(viewDir, toSun));
        
        // Sample light path optical depth (simplified)
        float lightOpticalDepth = exp(-relativeHeight * 8.0) * sunDistance * 0.001;
        
        // Calculate total optical depth
        float3 tau = (rayleigh * opticalDepthR + mie * opticalDepthM) + lightOpticalDepth;
        float3 transmittance = exp(-tau);
        
        // Phase functions for scattering
        float rayleighPhase = getRayleighPhase(cosTheta);
        float miePhase = getMiePhase(cosTheta, MieG);
        
        // Add in-scattered light contribution
        // SunIntensity now has more direct influence
        float3 scatteringCoeff = (rayleigh * rayleighPhase + mie * miePhase);
        totalScattering += transmittance * scatteringCoeff * density * stepSize * SunIntensity * 2.0; // Increased from 1.0 to 2.0
        
        currentPos += viewDir * stepSize;
    }
    
    // [[Sun-disk]] Add visible sun disk for ray marching mode
    // Use larger sun angular radius for better visibility
    float sunAngularRadius = max(SunAngularRadius, 0.035); // ~2 degrees for better visibility
    float sunCosAngle = cos(sunAngularRadius);
    
    // Use smoother falloff for sun disk
    float sunProximity = saturate((cosTheta - sunCosAngle) / (1.0 - sunCosAngle + 0.01));
    if (sunProximity > 0.0)
    {
        float sunDiskIntensity = smoothstep(0.0, 1.0, sunProximity);
        float3 sunColor = float3(1.0, 0.95, 0.8) * SunIntensity * 5.0; // Increased multiplier from 2.0 to 5.0
        totalScattering = lerp(totalScattering, sunColor, sunDiskIntensity * 0.9);
    }
    
    // Sunset/sunrise enhancement - when sun is near horizon
    if (sunElevation < 0.3 && sunElevation > -0.1)
    {
        float sunsetFactor = 1.0 - saturate((sunElevation + 0.1) / 0.4);
        float3 sunsetColor = float3(1.0, 0.6, 0.3) * sunsetFactor;
        
        // Add sunset color near sun direction
        float sunProximity = max(0.0, cosTheta);
        totalScattering += sunsetColor * sunProximity * sunsetFactor * 0.5;
    }
    
    // Ensure minimum brightness so sky is never completely black
    float3 minSkyColor = baseSkyColor * 0.15; // Minimum 15% of base sky color
    totalScattering = max(totalScattering, minSkyColor);
    
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

