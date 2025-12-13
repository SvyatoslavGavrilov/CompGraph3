//***************************************************************************************
// Terrain shader with GPU-based tessellation and heightmap sampling
//***************************************************************************************

// Constant data that varies per object (world matrix)
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
};

// Constant data that varies per frame
cbuffer cbPass : register(b1)
{
    float gTotalTime;
    float heightScale;
    float terrainSize;
    uint heightmapWidth;
    uint heightmapHeight;
    float tileSize;
    float3 cameraPosition;
    float padding;
};

// Tessellation parameters
cbuffer cbTessellation : register(b2)
{
    float minTessellationFactor;
    float maxTessellationFactor;
    float tessellationDistance;
    float padding2;
};

// Atmosphere parameters for terrain extinction and fog
cbuffer cbAtmosphere : register(b3)
{
    float3 sunDirection;
    float atmosphereRadius;
    float planetRadius;
    float pollutionLevel;
    float densityMultiplier;
    int atmosphereMode; // 0 = Hoffman-Preetham, 1 = Ray Marching
    float SunIntensity; // Sun intensity for terrain lighting
    // Exponential Height Fog parameters
    float FogHeight;
    float FogDensity;
    float FogHeightFalloff;
    float MinFogOpacity;
    float3 FogColor;
    float paddingFog0;
    int EnableFog;
    float paddingFog1[3];
};

// Textures
Texture2D heightmapTexture : register(t0);
Texture2D terrainTexture : register(t1);
SamplerState gSampler : register(s0);

// Vertex shader input/output for patches
struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float3 PosL : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct HullOut
{
    float3 PosL : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct DomainOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : WORLDPOS;
    float3 NormalW : NORMAL;
    float2 TexCoord : TEXCOORD;
};

// [[Terrain-shader-pipeline]] Vertex Shader - First programmable stage in the pipeline
// Purpose: Process control points of terrain patches and calculate texture coordinates
// Input: Control point position in local space (X, Z from quadtree node bounds, Y = 0)
// Output: Position and texture coordinates for hull shader
// 
// Why calculate UV here?
// - UV coordinates are needed by both hull shader (for passing through) and domain shader (for height sampling)
// - Calculating UV in vertex shader is more efficient than calculating in domain shader
// - UV coordinates are automatically interpolated by the tessellator
VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // [[Terrain-shader-pipeline]] Pass through position (no transformation yet)
    // Position is in local space relative to the terrain patch
    // Y coordinate is 0 - height will be added in domain shader by sampling heightmap
    // The position will be transformed to world/clip space in the domain shader
    vout.PosL = vin.PosL;
    
    // [[Terrain-shader-pipeline]] Calculate texture coordinates from world position
    // Terrain is centered at origin, so positions range from [-terrainSize/2, terrainSize/2]
    // We need to convert this to [0, 1] range for texture sampling
    // 
    // Step 1: Normalize position by terrain size
    // If terrainSize = 100, positions range from [-50, 50]
    // Dividing by 100 gives [-0.5, 0.5]
    float2 uv = float2(vin.PosL.x, vin.PosL.z) / terrainSize;
    
    // [[Terrain-shader-pipeline]] Step 2: Convert from [-0.5, 0.5] to [0, 1]
    // Multiplying by 0.5 gives [-0.25, 0.25]
    // Adding 0.5 gives [0.25, 0.75]
    // Actually: uv = uv * 0.5 + 0.5 converts [-0.5, 0.5] to [0, 1]
    // This ensures correct texture mapping across the entire terrain
    uv = uv * 0.5f + 0.5f;  // Convert from [-0.5, 0.5] to [0, 1]
    vout.TexCoord = uv;
    
    return vout;
}

// Constant Hull Shader - calculates tessellation factors
struct PatchTess
{
    float EdgeTess[4] : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

// [[Terrain-shader-pipeline]] Constant Hull Shader Function
// This function runs ONCE per patch (not per control point)
// Purpose: Calculate tessellation factors based on camera distance
// 
// How tessellation works:
// - Tessellation factor determines how many segments to create along each edge
// - Factor of N creates N segments (N+1 vertices) along the edge
// - Higher factors = more detail = more triangles
// - Factors are clamped to [1, 64] range (hardware limitation)
//
// Distance-based tessellation:
// - Closer patches get higher tessellation (more detail)
// - Far patches get lower tessellation (less detail)
// - This matches the CPU-side LOD selection algorithm
PatchTess ConstantHS(InputPatch<VertexOut, 4> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
    
    // [[Terrain-shader-pipeline]] STEP 1: Calculate patch center
    // Average the 4 control points to find the center of the patch
    // This gives us the world-space center for distance calculation
    // The center is used to determine how far the patch is from the camera
    float3 patchCenter = (patch[0].PosL + patch[1].PosL + patch[2].PosL + patch[3].PosL) * 0.25f;
    
    // [[Terrain-shader-pipeline]] STEP 2: Calculate distance from camera (2D distance)
    // Using 2D distance (X, Z only) matches the CPU-side LOD calculation
    // This ensures consistent behavior between CPU LOD and GPU tessellation
    // 2D distance is sufficient because terrain is mostly flat (Y variation is height, not distance)
    // NOTE: Camera coordinate system has X/Z swapped relative to terrain coordinate system
    // Camera's forward/back (Z) maps to terrain's left/right (X), and vice versa
    float2 cameraPos2D = float2(cameraPosition.z, cameraPosition.x);  // Swap axes: camera Z->terrain X, camera X->terrain Z
    float2 patchCenter2D = float2(patchCenter.x, patchCenter.z);
    float distance = length(cameraPos2D - patchCenter2D);
    
    // [[Terrain-shader-pipeline]] STEP 3: Calculate tessellation factor based on distance
    // Linear interpolation between max and min factors:
    // - When distance = 0: tessFactor = maxTessellationFactor (64) - maximum detail
    // - When distance = tessellationDistance: tessFactor = minTessellationFactor (1) - minimum detail
    // - When distance > tessellationDistance: tessFactor = minTessellationFactor (1) - clamped
    //
    // saturate() clamps the normalized distance to [0, 1] range
    // lerp() linearly interpolates: lerp(max, min, t) = max * (1-t) + min * t
    float normalizedDistance = saturate(distance / tessellationDistance);
    float tessFactor = lerp(maxTessellationFactor, minTessellationFactor, normalizedDistance);
    
    // [[Terrain-shader-pipeline]] STEP 4: Clamp to valid range
    // Hardware tessellation requires factors in [1, 64] range
    // Values outside this range are invalid and will cause errors
    tessFactor = clamp(tessFactor, minTessellationFactor, maxTessellationFactor);
    
    // [[Terrain-shader-pipeline]] STEP 5: Set tessellation factors for all edges and inside
    // For simplicity, all edges and inside use the same factor
    // More sophisticated implementations could use different factors per edge
    // This would allow adaptive detail based on edge visibility or importance
    // Edge order: [0]=top, [1]=right, [2]=bottom, [3]=left
    pt.EdgeTess[0] = tessFactor;  // Top edge
    pt.EdgeTess[1] = tessFactor;  // Right edge
    pt.EdgeTess[2] = tessFactor;  // Bottom edge
    pt.EdgeTess[3] = tessFactor;  // Left edge
    
    // [[Terrain-shader-pipeline]] Inside tessellation factors
    // For quad patches, there are 2 inside factors: U and V
    // These control tessellation in the two parametric directions
    // U = horizontal direction, V = vertical direction
    pt.InsideTess[0] = tessFactor; // U direction (horizontal)
    pt.InsideTess[1] = tessFactor; // V direction (vertical)
    
    return pt;
}

// Hull Shader - passes through control points
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]  // Counter-clockwise for correct front-facing (fixes inverted normals)
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
HullOut HS(InputPatch<VertexOut, 4> p, uint i : SV_OutputControlPointID, uint patchId : SV_PrimitiveID)
{
    HullOut hout;
    hout.PosL = p[i].PosL;
    hout.TexCoord = p[i].TexCoord;
    return hout;
}

// [[Terrain-shader-pipeline]] Domain Shader - Evaluates final vertex positions
// This function runs ONCE per tessellated vertex (not per control point)
// Purpose: Evaluate the final position of each vertex generated by the tessellator
//
// How it works:
// 1. The tessellator generates new vertices based on tessellation factors
// 2. For each generated vertex, it provides UV coordinates [0,1]x[0,1] within the patch
// 3. The domain shader uses these UV coordinates to:
//    - Interpolate control point positions (bilinear interpolation)
//    - Interpolate texture coordinates
//    - Sample height from heightmap
//    - Transform to world and clip space
//
// Why sample height here?
// - The tessellator creates vertices that don't exist in the original control points
// - Each tessellated vertex needs height sampled at its exact position
// - Sampling in domain shader ensures every vertex has correct height
[domain("quad")]  // Quad domain (matches hull shader)
DomainOut DS(PatchTess patchTess, float2 uv : SV_DomainLocation, const OutputPatch<HullOut, 4> quad)
{
    DomainOut dout;
    
    // [[Terrain-shader-pipeline]] STEP 1: Bilinear interpolation of control points
    // The tessellator provides uv coordinates [0,1]x[0,1] within the patch
    // We use bilinear interpolation to find the position at these coordinates
    //
    // Control point order: [0]=topLeft, [1]=bottomLeft, [2]=bottomRight, [3]=topRight
    // This matches the index order from CreateTerrainTile in labor_4.cpp
    //
    // First interpolation (Y direction - vertical):
    // uv.y ranges from 0 (top) to 1 (bottom)
    // leftEdge: interpolate between top-left and bottom-left
    // rightEdge: interpolate between top-right and bottom-right
    float3 leftEdge = lerp(quad[0].PosL, quad[1].PosL, uv.y);   // topLeft to bottomLeft
    float3 rightEdge = lerp(quad[3].PosL, quad[2].PosL, uv.y); // topRight to bottomRight
    
    // Second interpolation (X direction - horizontal):
    // uv.x ranges from 0 (left) to 1 (right)
    // posL: interpolate between left edge and right edge
    // This gives us the final interpolated position
    float3 posL = lerp(leftEdge, rightEdge, uv.x);  // left to right
    
    // [[Terrain-shader-pipeline]] STEP 2: Interpolate texture coordinates
    // Texture coordinates must be interpolated the same way as positions
    // This ensures correct UV mapping for heightmap sampling
    // The interpolated UV coordinates will be used to sample the heightmap
    float2 tLeft = lerp(quad[0].TexCoord, quad[1].TexCoord, uv.y);
    float2 tRight = lerp(quad[3].TexCoord, quad[2].TexCoord, uv.y);
    float2 texCoord = lerp(tLeft, tRight, uv.x);
    
    // [[Terrain-shader-pipeline]] STEP 3: Sample height from heightmap texture
    // The heightmap is a grayscale texture where:
    // - Black (0.0) = lowest elevation
    // - White (1.0) = highest elevation
    // SampleLevel with mip level 0 ensures we get the full-resolution heightmap
    // .r channel contains the height value (grayscale = same value in R, G, B)
    float heightValue = heightmapTexture.SampleLevel(gSampler, texCoord, 0).r;
    
    // [[Terrain-shader-pipeline]] STEP 4: Scale height value
    // heightValue is in [0, 1] range (from texture)
    // heightScale (100.0f) determines the maximum height in world units
    // Result: height is in [0, 100] world units
    float height = heightValue * heightScale;
    
    // [[Terrain-shader-pipeline]] STEP 5: Apply height to position
    // Control points have Y = 0 (flat)
    // We replace Y with the sampled height to create terrain elevation
    // This is why we sample height in domain shader - each tessellated vertex gets correct height
    posL.y = height;
    
    // [[Terrain-shader-pipeline]] STEP 6: Calculate normal from heightmap gradient
    // Sample neighboring heights to calculate normal using central differences
    // This gives us the terrain slope for proper directional lighting
    float texelSize = 1.0 / (float)heightmapWidth;
    
    // Sample heights at neighboring texels
    float heightL = heightmapTexture.SampleLevel(gSampler, texCoord + float2(-texelSize, 0), 0).r * heightScale;
    float heightR = heightmapTexture.SampleLevel(gSampler, texCoord + float2(texelSize, 0), 0).r * heightScale;
    float heightD = heightmapTexture.SampleLevel(gSampler, texCoord + float2(0, -texelSize), 0).r * heightScale;
    float heightU = heightmapTexture.SampleLevel(gSampler, texCoord + float2(0, texelSize), 0).r * heightScale;
    
    // Calculate gradient (slope) in X and Z directions
    // World space distance between samples: terrain spans terrainSize, texture spans 1.0
    float worldTexelSize = terrainSize / (float)heightmapWidth;
    float dx = (heightR - heightL) / (2.0 * worldTexelSize);
    float dz = (heightU - heightD) / (2.0 * worldTexelSize);
    
    // Create normal vector from gradient
    // Normal = (-gradient_x, 1.0, -gradient_z) normalized
    // This gives us a normal pointing in the direction of steepest ascent
    float3 normalL = normalize(float3(-dx, 1.0, -dz));
    
    // [[Terrain-shader-pipeline]] STEP 7: Transform to world space
    // gWorld is identity matrix for terrain (terrain is already in world space)
    // This transformation is included for consistency with other objects
    // The world position is stored for pixel shader (lighting calculations)
    float4 posW = mul(float4(posL, 1.0f), gWorld);
    dout.PosW = posW.xyz;
    
    // Transform normal to world space (assuming no scaling in gWorld)
    float3x3 worldNormalMatrix = (float3x3)gWorld;
    dout.NormalW = mul(normalL, worldNormalMatrix);
    dout.NormalW = normalize(dout.NormalW);
    
    // [[Terrain-shader-pipeline]] STEP 8: Transform to homogeneous clip space
    // gViewProj combines view and projection matrices
    // This transforms from world space to clip space for rasterization
    // Clip space coordinates are used by the GPU for clipping and perspective division
    dout.PosH = mul(posW, gViewProj);
    
    // [[Terrain-shader-pipeline]] STEP 9: Store texture coordinates
    // These will be used by the pixel shader for texture sampling
    // The coordinates are interpolated across the triangle during rasterization
    dout.TexCoord = texCoord;
    
    return dout;
}

// Atmospheric extinction calculation function
float3 CalculateAtmosphericExtinction(float3 worldPos, float3 viewDir)
{
    if (atmosphereMode == 0)
    {
        // Hoffman-Preetham extinction
        float3 planetCenter = float3(0, -planetRadius, 0);
        float height = length(worldPos - planetCenter);
        float opticalDepth = exp(-(height - planetRadius) / 8000.0) * densityMultiplier;
        float3 rayleigh = float3(0.0058, 0.0135, 0.0331) * (1.0 + pollutionLevel * 2.0);
        float3 mie = float3(0.000399, 0.000399, 0.000399) * (1.0 + pollutionLevel * 4.0);
        return exp(-(rayleigh + mie) * opticalDepth);
    }
    else
    {
        // Simplified extinction for Ray Marching mode
        float3 toCamera = normalize(cameraPosition - worldPos);
        float distanceToCamera = length(cameraPosition - worldPos);
        float extinctionFactor = exp(-distanceToCamera / 10000.0) * (1.0 - pollutionLevel * 0.5);
        return float3(extinctionFactor, extinctionFactor, extinctionFactor);
    }
}

// Calculate sky color based on view direction (for fog inscattering)
// Simplified version matching the atmosphere shader's sky color calculation
float3 CalculateSkyColor(float3 viewDir)
{
    // Normalize sun direction (points FROM sun, so negate to get direction TO sun)
    float3 sunDir = normalize(-sunDirection);
    float cosTheta = dot(viewDir, sunDir);
    
    // Calculate view elevation for sky gradient
    float3 up = float3(0, 1, 0);
    float viewElevation = dot(viewDir, up); // -1 = down, 0 = horizon, 1 = up
    
    // Sky color gradient (horizon to zenith)
    float3 horizonColor = float3(0.7, 0.8, 1.0); // Light blue/white at horizon
    float3 zenithColor = float3(0.15, 0.25, 0.5);  // Deep blue at zenith
    
    // Convert view elevation from [-1, 1] to [0, 1] for interpolation
    float elevationFactor = saturate((viewElevation + 1.0) * 0.5);
    elevationFactor = pow(elevationFactor, 0.7); // Slight curve for more natural gradient
    
    // Base sky color from gradient
    float3 baseSkyColor = lerp(horizonColor, zenithColor, elevationFactor);
    
    // Simplified sky color (without full scattering calculations for performance)
    // Use base gradient with some sun influence
    float sunInfluence = max(0.0, cosTheta) * 0.3; // Sun adds brightness when visible
    float3 skyColor = baseSkyColor * (0.5 + sunInfluence);
    
    // Ensure minimum brightness
    float3 minSkyColor = baseSkyColor * 0.1;
    skyColor = max(skyColor, minSkyColor);
    
    return skyColor;
}

// Exponential Height Fog calculation
// Based on standard exponential height fog model
// Applied as post-effect to terrain rendering
float CalculateExponentialHeightFog(float3 rayOrigin, float3 rayDirection, float rayLength, out float3 fogInscattering)
{
    if (EnableFog == 0 || FogDensity <= 0.0)
    {
        fogInscattering = float3(0, 0, 0);
        return 1.0; // No fog, full transmittance
    }
    
    // Get vertical component of ray direction (Y is up)
    float rayDirectionZ = rayDirection.y; // Y component is vertical
    
    // Calculate falloff factor
    // Clamp to prevent numerical issues with exp2
    float falloff = max(-127.0, FogHeightFalloff * rayDirectionZ);
    
    // Calculate line integral for exponential fog
    // Direct calculation
    float lineIntegral = (1.0 - exp2(-falloff)) / falloff;
    
    // Taylor expansion around 0 for numerical stability when falloff is near zero
    float log2 = 0.69314718056; // log(2.0)
    float lineIntegralTaylor = log2 - (0.5 * log2 * log2) * falloff;
    
    // Use Taylor expansion when falloff is very close to zero
    float FLT_EPSILON2 = 0.0001;
    float finalLineIntegral = abs(falloff) > FLT_EPSILON2 ? lineIntegral : lineIntegralTaylor;
    
    // Calculate fog density at ray origin
    float rayOriginHeight = rayOrigin.y - FogHeight; // Height relative to fog reference
    float rayOriginTerms = FogDensity * exp2(-FogHeightFalloff * rayOriginHeight);
    
    // Calculate exponential height line integral
    float exponentialHeightLineIntegral = rayOriginTerms * finalLineIntegral;
    
    // Scale by ray length
    exponentialHeightLineIntegral *= rayLength;
    
    // Calculate fog factor (transmittance)
    float expFogFactor = max(saturate(exp2(-exponentialHeightLineIntegral)), MinFogOpacity);
    
    // Calculate fog inscattering color using sky color
    // Fog should blend with the sky color, not a fixed color
    float3 skyColor = CalculateSkyColor(rayDirection);
    fogInscattering = skyColor * (1.0 - expFogFactor) * 0.3; // Use sky color for fog
    
    return expFogFactor;
}

// [[Terrain-shader-pipeline]] Pixel Shader - Final stage in the pipeline
// This function runs ONCE per pixel (fragment)
// Purpose: Determine the final color that will be written to the render target
//
// What happens:
// 1. Sample terrain texture using interpolated UV coordinates
// 2. Apply simple height-based lighting (depth cue)
// 3. Return final color
//
// The pixel shader receives interpolated values from the domain shader:
// - Position in world space (for lighting)
// - Texture coordinates (for texture sampling)
float4 PS(DomainOut pin) : SV_Target
{
    // [[Terrain-shader-pipeline]] STEP 1: Sample terrain texture
    // The terrain texture provides the base color for the terrain
    // Texture coordinates were calculated in vertex shader and interpolated
    // gSampler uses linear filtering and wrap addressing
    // Linear filtering provides smooth color transitions
    float4 texColor = terrainTexture.Sample(gSampler, pin.TexCoord);
    
    // [[Directional-lighting]] STEP 2: Calculate directional lighting from sun
    // Use normal-based lighting with sun direction for realistic terrain shading
    // sunDirection points FROM sun TO planet center
    // For lighting, we need direction FROM sun TO surface
    // Since sun is far away, direction from sun to planet ≈ direction from sun to surface
    float3 normal = normalize(pin.NormalW);
    // sunDirection already points FROM sun, so use it directly for light direction
    float3 lightDir = normalize(-sunDirection); // Direction FROM sun (light source direction)
    
    // Calculate N dot L (lambertian diffuse lighting)
    // Positive NdotL means surface faces the light
    float NdotL = max(dot(normal, lightDir), 0.0);
    
    // Ambient lighting to prevent complete darkness
    // Ambient is scaled by sun intensity to maintain realistic lighting balance
    // When sun is brighter, ambient should also increase slightly (indirect lighting)
    float ambientBase = 0.2; // Base ambient level
    float ambientScale = 0.1 + saturate(SunIntensity / 200.0) * 0.2; // Scale ambient with sun (0.1 to 0.3 range)
    float ambient = ambientBase + ambientScale;
    ambient = min(ambient, 0.4); // Cap ambient to prevent over-brightening
    
    // Combine ambient and directional lighting
    // SunIntensity directly scales the directional lighting contribution
    // Normalize sun intensity: 0 = no directional light, 200 = full directional light
    // Use a curve that makes lower values more noticeable
    float sunIntensityFactor = saturate(SunIntensity / 100.0); // Normalize to [0, 2] range, then clamp
    sunIntensityFactor = pow(sunIntensityFactor, 0.7); // Apply power curve for more natural response
    
    // Directional light contribution scales with sun intensity
    // When SunIntensity = 0, directional light = 0 (only ambient)
    // When SunIntensity = 100, directional light = NdotL (full effect)
    // When SunIntensity = 200, directional light = NdotL * 1.5 (brighter)
    float directionalLight = NdotL * (0.5 + sunIntensityFactor * 1.0);
    
    // Combine ambient and directional lighting
    float lightingFactor = ambient + (1.0 - ambient) * directionalLight;
    
    // Apply lighting to texture color
    texColor.rgb *= lightingFactor;
    
    // [[Atmosphere-integration]] Apply atmospheric extinction
    // Atmospheric extinction makes distant objects fade to sky color
    // This creates realistic atmospheric perspective
    float3 viewDir = normalize(cameraPosition - pin.PosW);
    float3 extinction = CalculateAtmosphericExtinction(pin.PosW, viewDir);
    texColor.rgb *= extinction;
    
    // [[Exponential-Height-Fog]] Apply exponential height fog as post-effect
    float3 fogInscattering;
    float3 rayOrigin = cameraPosition;
    float rayLength = length(pin.PosW - rayOrigin);
    float fogFactor = CalculateExponentialHeightFog(rayOrigin, viewDir, rayLength, fogInscattering);
    
    // Blend fog with terrain color: FinalColor = TerrainColor * FogTransmittance + FogInscattering
    texColor.rgb = texColor.rgb * fogFactor + fogInscattering;
    
    return texColor;
}
