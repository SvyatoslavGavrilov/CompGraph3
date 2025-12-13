//***************************************************************************************
// Atmosphere.hlsl - Dual-mode atmospheric scattering implementation
// Supports Hoffman-Preetham (ground level) and Ray Marching (high altitude) approaches
//***************************************************************************************
//
// ATMOSPHERE COLORING PROCESS SCHEME
// ===================================
//
// The atmosphere coloring follows this pipeline:
//
// ┌─────────────────────────────────────────────────────────────────────────────┐
// │                    INPUT PREPARATION STAGE                                  │
// ├─────────────────────────────────────────────────────────────────────────────┤
// │ 1. Apply Pollution Effects                                                 │
// │    - Rayleigh scattering: β_r = β_r_base × (1.0 + Pollution × 1.5)        │
// │    - Mie scattering:      β_m = β_m_base × (1.0 + Pollution × 3.0)       │
// │                                                                             │
// │ 2. Calculate Geometric Parameters                                           │
// │    - View direction: normalized direction from camera to sky point         │
// │    - Sun direction: normalized direction TO sun (negate SunDirection)      │
// │    - cos(θ): dot product of view and sun directions                        │
// │    - View elevation: vertical angle (-1 = down, 0 = horizon, 1 = up)       │
// │    - Sun elevation: vertical angle of sun position                         │
// └─────────────────────────────────────────────────────────────────────────────┘
//                              ↓
// ┌─────────────────────────────────────────────────────────────────────────────┐
// │                    BASE SKY GRADIENT STAGE                                  │
// ├─────────────────────────────────────────────────────────────────────────────┤
// │ 3. Calculate Base Sky Color Gradient                                       │
// │    - Horizon color: (0.7, 0.8, 1.0) - Light blue/white                     │
// │    - Zenith color:  (0.15, 0.25, 0.5) - Deep blue                         │
// │    - Interpolation: lerp(horizon, zenith, elevation_factor^0.7)            │
// │    - Result: baseSkyColor (varies from horizon to zenith)                  │
// └─────────────────────────────────────────────────────────────────────────────┘
//                              ↓
// ┌─────────────────────────────────────────────────────────────────────────────┐
// │                    DENSITY COLOR SHIFT STAGE                               │
// ├─────────────────────────────────────────────────────────────────────────────┤
// │ 4. Apply Density-Based Color Shifting                                      │
// │    - Density factor: saturate((DensityMultiplier - 0.1) / 9.9)^0.6        │
// │    - High density colors:                                                  │
// │      * Horizon: (0.9, 0.6, 0.4) - Orange/red                              │
// │      * Zenith:  (0.4, 0.3, 0.25) - Reddish brown                          │
// │    - Blend: lerp(normal_colors, high_density_colors, density_factor)       │
// │    - Physics: High density → more blue scattered/absorbed → redder sky     │
// └─────────────────────────────────────────────────────────────────────────────┘
//                              ↓
// ┌─────────────────────────────────────────────────────────────────────────────┐
// │                    SCATTERING CONTRIBUTION STAGE                            │
// ├─────────────────────────────────────────────────────────────────────────────┤
// │ 5. Rayleigh Scattering (Blue Sky Effect)                                   │
// │    - Phase function: P_R(θ) = 0.75 × (1.0 + cos²(θ))                      │
// │    - Contribution: β_r × P_R(θ) × SunIntensity × 2.0                      │
// │    - Color shift (high density):                                           │
// │      * Red:   +150% enhancement                                            │
// │      * Green: +50% enhancement                                             │
// │      * Blue:  -70% reduction                                               │
// │    - Result: rayleighContribution (blue-tinted scattering)                 │
// │                                                                             │
// │ 6. Mie Scattering (Haze/Forward Scattering)                                │
// │    - Phase function: P_M(θ) = 0.75 × (1-g²) / (1+g²-2g×cos(θ))^1.5       │
// │    - Contribution: β_m × P_M(θ) × SunIntensity × 1.0                       │
// │    - Color shift (high density):                                           │
// │      * Red:   +120% enhancement                                            │
// │      * Green: -30% reduction                                               │
// │      * Blue:  -50% reduction                                               │
// │    - Result: mieContribution (warm haze glow)                              │
// │                                                                             │
// │ 7. Combine Scattering                                                      │
// │    - Initial: skyColor = baseSkyColor × 0.3 (darker base)                 │
// │    - Add: skyColor += rayleighContribution                                 │
// │    - Add: skyColor += mieContribution                                      │
// └─────────────────────────────────────────────────────────────────────────────┘
//                              ↓
// ┌─────────────────────────────────────────────────────────────────────────────┐
// │                    SUN DISK RENDERING STAGE                                 │
// ├─────────────────────────────────────────────────────────────────────────────┤
// │ 8. Render Visible Sun Disk                                                 │
// │    - Angular radius: max(SunAngularRadius, 0.035) ≈ 2 degrees             │
// │    - Proximity check: cos(θ) > cos(sun_angular_radius)                     │
// │    - Intensity: smoothstep falloff for smooth edges                        │
// │    - Sun color: (1.0, 0.95, 0.8) × SunIntensity × 5.0                     │
// │    - Blend: lerp(skyColor, sunColor, intensity × 0.9)                      │
// └─────────────────────────────────────────────────────────────────────────────┘
//                              ↓
// ┌─────────────────────────────────────────────────────────────────────────────┐
// │                    SUNSET/SUNRISE ENHANCEMENT STAGE                        │
// ├─────────────────────────────────────────────────────────────────────────────┤
// │ 9. Add Sunset/Sunrise Colors                                               │
// │    - Condition: sun elevation between -0.1 and 0.3 (near horizon)           │
// │    - Sunset factor: 1.0 - saturate((elevation + 0.1) / 0.4)                │
// │    - Sunset color: (1.0, 0.6, 0.3) × sunset_factor                        │
// │    - Additive: skyColor += sunset_color × sun_proximity × factor × 0.5     │
// └─────────────────────────────────────────────────────────────────────────────┘
//                              ↓
// ┌─────────────────────────────────────────────────────────────────────────────┐
// │                    EXTINCTION & FINALIZATION STAGE                          │
// ├─────────────────────────────────────────────────────────────────────────────┤
// │ 10. Apply Extinction (Light Absorption)                                    │
// │     - Optical depth: exp(-height_above_ground / 8000.0)                    │
// │     - Extinction: exp(-(β_r + β_m) × optical_depth × density × 0.1)      │
// │     - Apply: skyColor ×= extinction                                        │
// │                                                                             │
// │ 11. Minimum Brightness Clamping                                            │
// │     - Minimum: baseSkyColor × 0.1 (10% of base for Hoffman-Preetham)      │
// │     - Minimum: baseSkyColor × 0.15 (15% of base for Ray Marching)         │
// │     - Clamp: skyColor = max(skyColor, minSkyColor)                         │
// │                                                                             │
// │ 12. Final Output                                                           │
// │     - Return: float4(skyColor, 1.0)                                        │
// └─────────────────────────────────────────────────────────────────────────────┘
//
// RAY MARCHING MODE DIFFERENCES:
// ===============================
// - Instead of analytical scattering, performs volumetric ray marching
// - Samples atmosphere in 32 steps along view ray
// - At each sample:
//   * Calculates density based on altitude: exp(-relative_height × 10.0)
//   * Accumulates optical depth for Rayleigh and Mie
//   * Calculates transmittance: exp(-τ)
//   * Adds in-scattered light: transmittance × scattering × density × step_size
// - Same sun disk and sunset effects applied after ray marching
//
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
    // ========================================================================
    // STAGE 1: INPUT PREPARATION
    // ========================================================================
    // Step 1.1: Apply pollution to scattering coefficients
    // Pollution increases all scattering, but we want to preserve blue dominance
    float pollutionFactor = 1.0 + PollutionLevel * 1.5; // Reduced pollution impact
    float3 rayleigh = RayleighScattering * pollutionFactor;
    // Mie scattering (haze) increases more with pollution
    float3 mie = MieScattering * (1.0 + PollutionLevel * 3.0);
    
    // Step 1.2: Calculate geometric parameters
    // Hoffman-Preetham calculation
    float3 viewDir = normalize(input.ViewDir);
    // SunDirection points FROM sun TO planet, so we negate it to get direction TO sun
    float3 sunDir = normalize(-SunDirection);
    float cosTheta = dot(viewDir, sunDir);
    
    // Calculate view and sun elevation for sky gradient
    float3 up = float3(0, 1, 0);
    float viewElevation = dot(viewDir, up); // -1 = down, 0 = horizon, 1 = up
    float sunElevation = dot(sunDir, up);
    
    // ========================================================================
    // STAGE 2: BASE SKY GRADIENT
    // ========================================================================
    // Step 2.1: Define base sky colors (horizon to zenith gradient)
    // [[Sky-background]] Calculate base sky color gradient (horizon to zenith)
    // Real atmosphere: horizon is lighter blue/white, zenith is deep blue
    // RGB values for realistic sky colors (no green tint)
    float3 horizonColor = float3(0.7, 0.8, 1.0); // Light blue/white at horizon (realistic)
    float3 zenithColor = float3(0.15, 0.25, 0.5);  // Deep blue at zenith (realistic)
    
    // ========================================================================
    // STAGE 3: DENSITY COLOR SHIFT
    // ========================================================================
    // Step 3.1: Calculate density factor for color shifting
    // [[Density-color-shift]] High density shifts sky color towards red/orange
    // When density is high (pollution, dust), blue light scatters more and gets absorbed
    // Red/orange light passes through more easily, making sky appear redder
    // Normalize density multiplier: 0.1-10.0 range maps to 0-1 for color shift
    float densityFactor = saturate((DensityMultiplier - 0.1) / 9.9); // Map 0.1-10.0 to 0-1
    densityFactor = pow(densityFactor, 0.6); // Apply curve for more gradual shift
    
    // Step 3.2: Define high-density colors (red/orange shift)
    // Shift colors towards red/orange when density is high
    // Low density: blue sky colors
    // High density: red/orange sky colors (like polluted/dusty atmosphere)
    float3 horizonColorHighDensity = float3(0.9, 0.6, 0.4); // Orange/red at horizon (high density)
    float3 zenithColorHighDensity = float3(0.4, 0.3, 0.25);   // Reddish brown at zenith (high density)
    
    // Step 3.3: Blend normal and high-density colors
    // Blend between normal and high-density colors based on density multiplier
    horizonColor = lerp(horizonColor, horizonColorHighDensity, densityFactor);
    zenithColor = lerp(zenithColor, zenithColorHighDensity, densityFactor);
    
    // Step 3.4: Calculate elevation-based interpolation factor
    // Convert view elevation from [-1, 1] to [0, 1] for interpolation
    // 0 = looking down, 0.5 = horizon, 1 = looking up
    float elevationFactor = saturate((viewElevation + 1.0) * 0.5);
    elevationFactor = pow(elevationFactor, 0.7); // Slight curve for more natural gradient
    
    // Step 3.5: Compute final base sky color from gradient
    // Base sky color from gradient
    float3 baseSkyColor = lerp(horizonColor, zenithColor, elevationFactor);
    
    // ========================================================================
    // STAGE 4: SCATTERING CONTRIBUTION
    // ========================================================================
    // Step 4.1: Pre-calculate extinction (will be applied later)
    // Extinction factor (Ft) - for sky dome, use simplified calculation
    // Sky dome is at fixed radius, so we use a constant optical depth based on height above ground
    // For sky dome rendering, we want minimal extinction (sky should be bright)
    float heightAboveGround = max(0.0, length(input.WorldPos - PlanetCenter) - PlanetRadius);
    float opticalDepth = exp(-heightAboveGround / 8000.0);
    float3 extinction = exp(-(rayleigh + mie) * opticalDepth * DensityMultiplier * 0.1); // Reduced extinction for sky
    
    // Step 4.2: Initialize sky color with darker base
    // Sky color calculation - start with darker base so sun intensity has more effect
    float3 skyColor = baseSkyColor * 0.3; // Darker base so sun intensity is more visible
    
    // Step 4.3: Calculate Rayleigh scattering contribution (blue sky effect)
    // Rayleigh scattering contribution (blue sky)
    // This is what makes the sky blue - blue light scatters more than red/green
    // SunIntensity now has much more influence (multiplier increased)
    float rayleighPhase = 0.75 * (1.0 + cosTheta * cosTheta); // Phase function: P_R(θ) = 0.75 × (1.0 + cos²(θ))
    float3 rayleighContribution = rayleigh * rayleighPhase * SunIntensity * 2.0; // Increased from 0.6 to 2.0
    
    // Step 4.4: Apply density-based color shift to Rayleigh scattering
    // [[Density-color-shift]] Reduce blue scattering and enhance red when density is high
    // High density causes more blue light to be scattered/absorbed, less blue reaches observer
    // Red light passes through more easily, so we reduce blue contribution and enhance red
    float3 rayleighColorShift = float3(1.0, 1.0, 1.0); // Default: all colors equal
    rayleighColorShift.r = 1.0 + densityFactor * 1.5; // Enhance red at high density (+150%)
    rayleighColorShift.g = 1.0 + densityFactor * 0.5; // Slight green enhancement (+50%)
    rayleighColorShift.b = 1.0 - densityFactor * 0.7;  // Reduce blue at high density (-70%)
    rayleighContribution *= rayleighColorShift;
    skyColor += rayleighContribution; // Add Rayleigh contribution to sky color
    
    // Step 4.5: Calculate Mie scattering contribution (haze/forward scattering)
    // Mie scattering contribution (forward scattering - hazy glow)
    // Mie scattering is more uniform (white), adds haze but shouldn't dominate
    float g2 = MieG * MieG;
    float miePhase = 0.75 * ((1.0 - g2) / pow(1.0 + g2 - 2.0 * MieG * cosTheta, 1.5)); // Henyey-Greenstein phase function
    float3 mieContribution = mie * miePhase * SunIntensity * 1.0; // Increased from 0.2 to 1.0
    
    // Step 4.6: Apply density-based color shift to Mie scattering
    // [[Density-color-shift]] Mie scattering also shifts towards red at high density
    // Mie scattering from dust/pollution particles tends to be more red/orange
    float3 mieColorShift = float3(1.0, 0.9, 0.7); // Default: slightly warm
    mieColorShift.r = 1.0 + densityFactor * 1.2; // Enhance red at high density (+120%)
    mieColorShift.g = 1.0 - densityFactor * 0.3; // Reduce green slightly (-30%)
    mieColorShift.b = 1.0 - densityFactor * 0.5; // Reduce blue more (-50%)
    mieContribution *= mieColorShift;
    skyColor += mieContribution; // Add Mie contribution to sky color
    
    // ========================================================================
    // STAGE 5: SUN DISK RENDERING
    // ========================================================================
    // Step 5.1: Calculate sun disk parameters
    // [[Sun-disk]] Add visible sun disk
    // Use larger sun angular radius for better visibility
    // Default to ~2 degrees (0.035 radians) instead of 0.27 degrees
    float sunAngularRadius = max(SunAngularRadius, 0.035); // ~2 degrees for better visibility
    float sunCosAngle = cos(sunAngularRadius);
    
    // Step 5.2: Check if view direction is within sun disk
    // Check if view direction is close to sun direction
    // Use a smoother falloff for the sun disk
    float sunProximity = saturate((cosTheta - sunCosAngle) / (1.0 - sunCosAngle + 0.01));
    if (sunProximity > 0.0)
    {
        // Step 5.3: Calculate sun disk intensity with smooth falloff
        // Calculate sun disk intensity (smooth falloff)
        float sunDiskIntensity = smoothstep(0.0, 1.0, sunProximity);
        
        // Step 5.4: Define sun color and blend with sky
        // Sun color (bright white/yellow) - SunIntensity has direct influence
        float3 sunColor = float3(1.0, 0.95, 0.8) * SunIntensity * 5.0; // Increased multiplier from 2.0 to 5.0
        
        // Blend sun disk with sky color - more aggressive blending
        skyColor = lerp(skyColor, sunColor, sunDiskIntensity * 0.9);
    }
    
    // ========================================================================
    // STAGE 6: SUNSET/SUNRISE ENHANCEMENT
    // ========================================================================
    // Step 6.1: Add sunset/sunrise colors when sun is near horizon
    // Sunset/sunrise enhancement - when sun is near horizon
    if (sunElevation < 0.3 && sunElevation > -0.1)
    {
        float sunsetFactor = 1.0 - saturate((sunElevation + 0.1) / 0.4);
        float3 sunsetColor = float3(1.0, 0.6, 0.3) * sunsetFactor;
        
        // Add sunset color near sun direction
        float sunProximity = max(0.0, cosTheta);
        skyColor += sunsetColor * sunProximity * sunsetFactor * 0.5;
    }
    
    // ========================================================================
    // STAGE 7: EXTINCTION & FINALIZATION
    // ========================================================================
    // Step 7.1: Apply extinction (light absorption through atmosphere)
    // Apply extinction (but ensure minimum brightness)
    skyColor *= extinction;
    
    // Step 7.2: Clamp to minimum brightness
    // Ensure minimum brightness so sky is never completely black
    float3 minSkyColor = baseSkyColor * 0.1; // Minimum 10% of base sky color
    skyColor = max(skyColor, minSkyColor);
    
    // Step 7.3: Apply brightness reduction when sun is above horizon
    // Full brightness (1.0) when sunDir.y <= 0.0 (from y == -1.0 to y == 0.0, i.e., sun below/at horizon)
    // Gradually reduce brightness when sunDir.y > 0.0 (sun above horizon)
    // Brightness multiplier: 1.0 at y == 0.0 (horizon), 0.0 at y == 1.0 (directly overhead)
    if (sunDir.y > 0.0)
    {
        float brightnessMultiplier = 1.0 - sunDir.y; // Linear interpolation from 1.0 (at y=0) to 0.0 (at y=1)
        brightnessMultiplier = max(0.0, brightnessMultiplier); // Clamp to avoid negative values
        skyColor *= brightnessMultiplier;
    }
    // If sunDir.y <= 0.0, brightness remains at 1.0 (full brightness)
    
    // Step 7.4: Return final color
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
    // ========================================================================
    // STAGE 1: INPUT PREPARATION
    // ========================================================================
    // Step 1.1: Apply pollution to scattering coefficients
    // Apply pollution to scattering coefficients
    // Pollution increases all scattering, but we want to preserve blue dominance
    float pollutionFactor = 1.0 + PollutionLevel * 1.5; // Reduced pollution impact
    float3 rayleigh = RayleighScattering * pollutionFactor;
    // Mie scattering (haze) increases more with pollution
    float3 mie = MieScattering * (1.0 + PollutionLevel * 3.0);
    
    // Step 1.2: Calculate geometric parameters
    float3 viewDir = normalize(input.ViewDir);
    // SunDirection points FROM sun TO planet, so we negate it to get direction TO sun
    float3 sunDir = normalize(-SunDirection);
    float3 startPos = CameraPos;
    float3 endPos = input.WorldPos;
    
    // Calculate view and sun elevation for sky gradient
    float3 up = float3(0, 1, 0);
    float viewElevation = dot(viewDir, up);
    float sunElevation = dot(sunDir, up);
    
    // ========================================================================
    // STAGE 2: BASE SKY GRADIENT
    // ========================================================================
    // Step 2.1: Define base sky colors (horizon to zenith gradient)
    // [[Sky-background]] Calculate base sky color gradient (horizon to zenith)
    // Real atmosphere: horizon is lighter blue/white, zenith is deep blue
    float3 horizonColor = float3(0.7, 0.8, 1.0); // Light blue/white at horizon (realistic)
    float3 zenithColor = float3(0.15, 0.25, 0.5);  // Deep blue at zenith (realistic)
    
    // ========================================================================
    // STAGE 3: DENSITY COLOR SHIFT
    // ========================================================================
    // Step 3.1: Calculate density factor for color shifting
    // [[Density-color-shift]] High density shifts sky color towards red/orange
    // When density is high (pollution, dust), blue light scatters more and gets absorbed
    // Red/orange light passes through more easily, making sky appear redder
    // Normalize density multiplier: 0.1-10.0 range maps to 0-1 for color shift
    float densityFactor = saturate((DensityMultiplier - 0.1) / 9.9); // Map 0.1-10.0 to 0-1
    densityFactor = pow(densityFactor, 0.6); // Apply curve for more gradual shift
    
    // Step 3.2: Define high-density colors and blend
    // Shift colors towards red/orange when density is high
    // Low density: blue sky colors
    // High density: red/orange sky colors (like polluted/dusty atmosphere)
    float3 horizonColorHighDensity = float3(0.9, 0.6, 0.4); // Orange/red at horizon (high density)
    float3 zenithColorHighDensity = float3(0.4, 0.3, 0.25);   // Reddish brown at zenith (high density)
    
    // Blend between normal and high-density colors based on density multiplier
    horizonColor = lerp(horizonColor, horizonColorHighDensity, densityFactor);
    zenithColor = lerp(zenithColor, zenithColorHighDensity, densityFactor);
    
    // Step 3.3: Calculate elevation-based interpolation and base sky color
    // Convert view elevation for interpolation
    float elevationFactor = saturate((viewElevation + 1.0) * 0.5);
    elevationFactor = pow(elevationFactor, 0.7); // Slight curve for more natural gradient
    
    // Base sky color from gradient
    float3 baseSkyColor = lerp(horizonColor, zenithColor, elevationFactor);
    
    // ========================================================================
    // STAGE 4: VOLUMETRIC RAY MARCHING
    // ========================================================================
    // Step 4.1: Initialize ray marching parameters
    // Ray marching parameters (volumetric scattering)
    int sampleCount = 32;
    float stepSize = length(endPos - startPos) / sampleCount;
    
    float3 totalScattering = baseSkyColor * 0.6; // Start with brighter base sky color
    float opticalDepthR = 0.0;
    float opticalDepthM = 0.0;
    
    float3 currentPos = startPos;
    float cosTheta = dot(viewDir, sunDir);
    
    // Step 4.2: Ray march through atmosphere (volumetric scattering)
    // [[Volumetric-scattering]] Ray march through atmosphere
    for (int i = 0; i < sampleCount; i++)
    {
        // Step 4.2.1: Calculate atmospheric density at current sample point
        float height = length(currentPos - PlanetCenter);
        
        // Density calculation based on height (atmospheric density decreases with altitude)
        float relativeHeight = (height - PlanetRadius) / (AtmosphereRadius - PlanetRadius);
        float density = exp(-relativeHeight * 10.0) * DensityMultiplier;
        density = max(0.0, density); // Clamp to avoid negative values
        
        // Step 4.2.2: Accumulate optical depth along view ray
        // Calculate optical depth along view ray
        float sampleDistance = stepSize;
        opticalDepthR += sampleDistance * density * length(rayleigh);
        opticalDepthM += sampleDistance * density * length(mie);
        
        // Step 4.2.3: Calculate light path optical depth (simplified)
        // Calculate light scattering from sun to this point
        // sunDir already points TO sun (we negated it above)
        float3 toSun = sunDir;
        float sunDistance = (AtmosphereRadius - height) / max(0.001, dot(viewDir, toSun));
        
        // Sample light path optical depth (simplified)
        float lightOpticalDepth = exp(-relativeHeight * 8.0) * sunDistance * 0.001;
        
        // Step 4.2.4: Calculate transmittance (light absorption)
        // Calculate total optical depth
        float3 tau = (rayleigh * opticalDepthR + mie * opticalDepthM) + lightOpticalDepth;
        float3 transmittance = exp(-tau);
        
        // Step 4.2.5: Calculate phase functions for scattering
        // Phase functions for scattering
        float rayleighPhase = getRayleighPhase(cosTheta);
        float miePhase = getMiePhase(cosTheta, MieG);
        
        // Step 4.2.6: Calculate scattering coefficients and apply density color shift
        // Add in-scattered light contribution
        // SunIntensity now has more direct influence
        float3 scatteringCoeff = (rayleigh * rayleighPhase + mie * miePhase);
        
        // [[Density-color-shift]] Apply color shift for high density
        // High density causes more blue light to be scattered/absorbed
        // Red light passes through more easily, so we reduce blue contribution and enhance red
        float densityFactor = saturate((DensityMultiplier - 0.1) / 9.9); // Map 0.1-10.0 to 0-1
        densityFactor = pow(densityFactor, 0.6); // Apply curve for more gradual shift
        
        float3 colorShift = float3(1.0, 1.0, 1.0); // Default: all colors equal
        colorShift.r = 1.0 + densityFactor * 1.5; // Enhance red at high density
        colorShift.g = 1.0 + densityFactor * 0.5; // Slight green enhancement
        colorShift.b = 1.0 - densityFactor * 0.7; // Reduce blue at high density
        scatteringCoeff *= colorShift;
        
        // Step 4.2.7: Add in-scattered light contribution
        // In-scattered light = transmittance × scattering_coefficient × density × step_size × sun_intensity
        totalScattering += transmittance * scatteringCoeff * density * stepSize * SunIntensity * 2.0; // Increased from 1.0 to 2.0
        
        // Step 4.2.8: Advance to next sample point
        currentPos += viewDir * stepSize;
    }
    
    // ========================================================================
    // STAGE 5: SUN DISK RENDERING
    // ========================================================================
    // Step 5.1-5.4: Same as Hoffman-Preetham mode
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
    
    // ========================================================================
    // STAGE 6: SUNSET/SUNRISE ENHANCEMENT
    // ========================================================================
    // Step 6.1: Same as Hoffman-Preetham mode
    // Sunset/sunrise enhancement - when sun is near horizon
    if (sunElevation < 0.3 && sunElevation > -0.1)
    {
        float sunsetFactor = 1.0 - saturate((sunElevation + 0.1) / 0.4);
        float3 sunsetColor = float3(1.0, 0.6, 0.3) * sunsetFactor;
        
        // Add sunset color near sun direction
        float sunProximity = max(0.0, cosTheta);
        totalScattering += sunsetColor * sunProximity * sunsetFactor * 0.5;
    }
    
    // ========================================================================
    // STAGE 7: FINALIZATION
    // ========================================================================
    // Step 7.1: Clamp to minimum brightness (extinction handled in ray marching)
    // Ensure minimum brightness so sky is never completely black
    float3 minSkyColor = baseSkyColor * 0.15; // Minimum 15% of base sky color
    totalScattering = max(totalScattering, minSkyColor);
    
    // Step 7.2: Apply brightness reduction when sun is above horizon
    // Full brightness (1.0) when sunDir.y <= 0.0 (from y == -1.0 to y == 0.0, i.e., sun below/at horizon)
    // Gradually reduce brightness when sunDir.y > 0.0 (sun above horizon)
    // Brightness multiplier: 1.0 at y == 0.0 (horizon), 0.0 at y == 1.0 (directly overhead)
    if (sunDir.y < 0.0)
    {
        float brightnessMultiplier = 1.0 + sunDir.y - 0.3; // Linear interpolation from 1.0 (at y=0) to 0.0 (at y=1)
        brightnessMultiplier = max(0.0, brightnessMultiplier); // Clamp to avoid negative values
        totalScattering *= brightnessMultiplier;
    }
    // If sunDir.y <= 0.0, brightness remains at 1.0 (full brightness)
    
    // Step 7.3: Return final color
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

