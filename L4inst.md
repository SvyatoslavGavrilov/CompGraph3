# Comprehensive Homework Implementation Guide: Soft Ray Traced Shadows

## Assignment Overview
Implement soft ray-traced shadows in your DirectX 12 application using hardware-accelerated ray tracing (DXR). You may choose between two techniques described in the presentation or research an alternative approach. This assignment demonstrates practical application of ray tracing concepts for realistic shadow effects.

## Prerequisites & Setup Requirements

### Hardware Requirements
- GPU supporting DirectX Raytracing (DXR) Tier 1.0 or higher (NVIDIA RTX series, AMD RDNA2+, or Intel Arc)
- Windows 10/11 with latest graphics drivers
- DirectX 12 Ultimate compatible system

### Software Requirements
- Visual Studio 2022 (or newer)
- Windows SDK (10.0.20348.0 or newer)
- DirectX 12 development environment
- Git for source control
- Cursor AI IDE (for AI-assisted development)

### Recommended Reference Repositories
```bash
# Clone these repositories for reference implementations
git clone https://github.com/microsoft/DirectX-Graphics-Samples
git clone https://github.com/NVIDIAGameWorks/DxrTutorials
git clone https://github.com/acmarrs/IntroToDXR
```

## Technical Approach Options

### Option 1: Cone Sampling Approach
**Concept:** Cast multiple shadow rays with randomized directions within a cone from the surface point toward the light source. The cone angle determines shadow softness.

**Advantages:**
- More physically accurate
- No post-processing artifacts
- Direct control over softness parameter

**Disadvantages:**
- Higher computational cost (multiple rays per pixel)
- Requires denoising for clean results

### Option 2: Post-Processing Filter Approach
**Concept:** Generate hard ray-traced shadows first, then apply a variable-radius blur filter based on calculated penumbra size.

**Advantages:**
- Lower ray count (better performance)
- Reuses existing denoising/filtering techniques
- Easier to integrate with existing rendering pipelines

**Disadvantages:**
- Less physically accurate
- Potential filtering artifacts at geometry edges
- Requires careful depth-aware filtering

## Implementation Guide (Option 1: Cone Sampling)

### Step 1: Project Setup & Dependencies
1. Create a new DirectX 12 project in Cursor AI
2. Add DXR support by initializing the device with ray tracing capabilities:
```cpp
// Check for ray tracing support
D3D12_FEATURE_DATA_D3D12_OPTIONS5 raytracingCaps = {};
device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_DATA_D3D12_OPTIONS5, 
                           &raytracingCaps, sizeof(raytracingCaps));
if (raytracingCaps.RaytracingTier < D3D12_RAYTRACING_TIER_1_0) {
    throw std::runtime_error("Device doesn't support ray tracing");
}
```

3. Set up descriptor heaps and command lists with ray tracing support:
```cpp
// Create ray tracing command list
ID3D12GraphicsCommandList4* raytracingCommandList;
device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, 
                         commandAllocator, nullptr, 
                         IID_PPV_ARGS(&raytracingCommandList));
```

### Step 2: Acceleration Structure Setup
1. Create Bottom-Level Acceleration Structures (BLAS) for your geometry:
```cpp
// Describe geometry for BLAS
D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
geometryDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress();
geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
geometryDesc.Triangles.VertexCount = vertexCount;
geometryDesc.Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress();
geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
geometryDesc.Triangles.IndexCount = indexCount;
geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
```

2. Create Top-Level Acceleration Structure (TLAS) with instances:
```cpp
// Create instance description
D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
instanceDesc.InstanceID = 0;
instanceDesc.InstanceContributionToHitGroupIndex = 0;
instanceDesc.InstanceMask = 0xFF;
instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1.0f;
instanceDesc.AccelerationStructure = blasResource->GetGPUVirtualAddress();
instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
```

### Step 3: Shader Implementation
1. Create ray generation shader for shadow rays:
```hlsl
// RayGenerationShader.hlsl
[shader("raygeneration")]
void RayGen() {
    uint2 pixelCoord = DispatchRaysIndex().xy;
    uint2 pixelDim = DispatchRaysDimensions().xy;
    
    // Get surface position and normal from G-buffer
    float3 worldPos = gGBufferPosition[pixelCoord].xyz;
    float3 normal = normalize(gGBufferNormal[pixelCoord].xyz);
    
    // Get light direction (assuming directional light)
    float3 lightDir = normalize(-gLightDirection);
    
    // Check if point is in shadow
    float shadow = 0.0f;
    const uint sampleCount = 16; // Number of shadow samples
    
    for (uint i = 0; i < sampleCount; i++) {
        // Generate random direction within cone
        float coneAngle = 0.1f; // Softness parameter (adjustable)
        float3 randomDir = GetRandomConeDirection(lightDir, coneAngle, i);
        
        // Trace shadow ray
        RayDesc ray;
        ray.Origin = worldPos + normal * 0.01f; // Offset to avoid self-intersection
        ray.Direction = randomDir;
        ray.TMin = 0.01f;
        ray.TMax = 1000.0f;
        
        ShadowPayload payload;
        payload.isOccluded = false;
        
        TraceRay(gSceneAccelerationStructure, 
                RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | 
                RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                0xFF, 0, 1, 0, ray, payload);
        
        if (!payload.isOccluded) {
            shadow += 1.0f;
        }
    }
    
    // Calculate average shadow factor
    shadow = shadow / sampleCount;
    
    // Apply shadow to final color
    float4 color = gGBufferAlbedo[pixelCoord];
    color.rgb *= lerp(0.2f, 1.0f, shadow); // Simple shadow application
    
    gOutput[pixelCoord] = color;
}
```

2. Create miss shader for shadow rays:
```hlsl
// MissShader.hlsl
[shader("miss")]
void ShadowMiss(inout ShadowPayload payload) {
    payload.isOccluded = false; // Ray missed geometry, no occlusion
}
```

3. Create any-hit shader for alpha testing (optional):
```hlsl
// AnyHitShader.hlsl
[shader("anyhit")]
void ShadowAnyHit(inout ShadowPayload payload, 
                 const BuiltInTriangleIntersectionAttributes attrib) {
    // Skip transparent objects
    if (IsTransparent(PrimitiveIndex())) {
        IgnoreHit();
    }
}
```

### Step 4: Random Direction Generation
Add this helper function to your HLSL shader:
```hlsl
// Random number generator using Wang hash
uint wang_hash(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

// Generate random direction within cone around baseDir
float3 GetRandomConeDirection(float3 baseDir, float coneAngle, uint sampleIndex) {
    uint2 dispatchIndex = DispatchRaysIndex().xy;
    uint seed = dispatchIndex.x * 1973u + dispatchIndex.y * 9277u + sampleIndex * 26699u;
    
    // Generate random rotation around base direction
    float phi = 2.0f * PI * frac(wang_hash(seed++) * 0.000001f);
    
    // Generate random angle within cone
    float cosThetaMax = cos(coneAngle);
    float cosTheta = cosThetaMax + (1.0f - cosThetaMax) * frac(wang_hash(seed++) * 0.000001f);
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
    
    // Create orthonormal basis
    float3 tangent;
    if (abs(baseDir.y) < 0.999f) {
        tangent = normalize(cross(baseDir, float3(0, 1, 0)));
    } else {
        tangent = normalize(cross(baseDir, float3(1, 0, 0)));
    }
    float3 bitangent = cross(baseDir, tangent);
    
    // Apply rotation and angle
    float3 direction = cosTheta * baseDir + 
                      sinTheta * (cos(phi) * tangent + sin(phi) * bitangent);
    
    return normalize(direction);
}
```

### Step 5: Pipeline Setup
1. Create root signatures for ray tracing:
```cpp
// Global root signature
D3D12_ROOT_SIGNATURE_DESC globalRootSignatureDesc = {};
globalRootSignatureDesc.NumParameters = 3; // Acceleration structure, output texture, G-buffer textures
globalRootSignatureDesc.pParameters = rootParameters;
globalRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

// Create local root signature for hit groups
// (Similar setup but with different parameters)
```

2. Create ray tracing pipeline state object (RTPSO):
```cpp
// Create RTPSO with all shader exports
D3D12_STATE_OBJECT_DESC rtpsoDesc = {};
rtpsoDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
rtpsoDesc.NumSubobjects = subobjectCount;
rtpsoDesc.pSubobjects = subobjects.data();

ID3D12StateObject* raytracingPSO;
device->CreateStateObject(&rtpsoDesc, IID_PPV_ARGS(&raytracingPSO));
```

3. Create shader tables:
```cpp
// Get shader identifiers
void* rayGenShaderId = stateObjectProps->GetShaderIdentifier(L"RayGen");
void* missShaderId = stateObjectProps->GetShaderIdentifier(L"ShadowMiss");
void* hitGroupShaderId = stateObjectProps->GetShaderIdentifier(L"HitGroup");

// Create shader table resource and copy identifiers
const UINT shaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(ShaderConstants);
const UINT shaderTableSize = shaderRecordSize * 3; // 3 entries: RayGen, Miss, HitGroup

// Allocate and map shader table
UINT8* shaderTableData;
shaderTable->Map(0, nullptr, reinterpret_cast<void**>(&shaderTableData));

// Copy shader identifiers and constants
memcpy(shaderTableData, rayGenShaderId, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
// Copy ray generation constants...
```

### Step 6: Dispatch Ray Tracing
```cpp
// Set up dispatch rays description
D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
dispatchDesc.RayGenerationShaderRecord.StartAddress = shaderTable->GetGPUVirtualAddress();
dispatchDesc.RayGenerationShaderRecord.SizeInBytes = shaderRecordSize;
dispatchDesc.MissShaderTable.StartAddress = shaderTable->GetGPUVirtualAddress() + shaderRecordSize;
dispatchDesc.MissShaderTable.SizeInBytes = shaderRecordSize;
dispatchDesc.MissShaderTable.StrideInBytes = shaderRecordSize;
dispatchDesc.HitGroupTable.StartAddress = shaderTable->GetGPUVirtualAddress() + shaderRecordSize * 2;
dispatchDesc.HitGroupTable.SizeInBytes = shaderRecordSize;
dispatchDesc.HitGroupTable.StrideInBytes = shaderRecordSize;
dispatchDesc.Width = screenWidth;
dispatchDesc.Height = screenHeight;
dispatchDesc.Depth = 1;

// Dispatch rays
commandList->SetPipelineState1(raytracingPSO);
commandList->DispatchRays(&dispatchDesc);
```

## Implementation Guide (Option 2: Post-Processing Filter)

### Step 1: Hard Shadow Generation
1. Implement basic ray-traced hard shadows first:
```hlsl
// Simple shadow ray generation
float CastShadowRay(float3 origin, float3 direction) {
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.01f;
    ray.TMax = 1000.0f;
    
    ShadowPayload payload;
    payload.visibility = 1.0f;
    
    TraceRay(gSceneAccelerationStructure, 
            RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | 
            RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
            0xFF, 0, 1, 0, ray, payload);
    
    return payload.visibility;
}
```

### Step 2: Penumbra Size Calculation
Add this function to calculate penumbra size for filtering:
```hlsl
float CalculatePenumbraSize(float3 surfacePos, float3 lightDir, float lightRadius) {
    // Get depth from depth buffer (or G-buffer)
    float depth = gDepthBuffer[DispatchRaysIndex().xy].r;
    float3 viewPos = ReconstructViewPosition(DispatchRaysIndex().xy, depth);
    
    // Calculate distance to light
    float distanceToLight = length(gLightPosition - surfacePos);
    
    // Calculate penumbra size based on light size and distances
    float penumbraSize = lightRadius * (distanceToLight - depth) / distanceToLight;
    
    // Convert to screen space size
    float2 screenSpaceSize = penumbraSize * gScreenToView.xy;
    
    return length(screenSpaceSize);
}
```

### Step 3: Depth-Aware Gaussian Blur
Implement a depth-aware blur shader:
```hlsl
// ScreenSpaceShadowBlur.hlsl
Texture2D<float> gShadowMap : register(t0);
Texture2D<float> gDepthBuffer : register(t1);
RWTexture2D<float> gBlurredShadows : register(u0);

cbuffer Constants : register(b0) {
    float2 gInvScreenSize;
    float gBlurScale;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint2 pixelCoord = dispatchThreadID.xy;
    
    // Get base shadow value and depth
    float centerShadow = gShadowMap[pixelCoord];
    float centerDepth = gDepthBuffer[pixelCoord];
    
    // Calculate adaptive blur radius based on penumbra size
    float penumbraSize = CalculatePenumbraSizeFromShadowMap(pixelCoord);
    int blurRadius = clamp(int(penumbraSize * gBlurScale), 1, 8);
    
    float shadowSum = 0.0f;
    float weightSum = 0.0f;
    
    // Apply Gaussian blur with depth check
    for (int y = -blurRadius; y <= blurRadius; y++) {
        for (int x = -blurRadius; x <= blurRadius; x++) {
            uint2 sampleCoord = pixelCoord + uint2(x, y);
            
            // Check if sample is within bounds
            if (sampleCoord.x >= 0 && sampleCoord.x < gScreenSize.x &&
                sampleCoord.y >= 0 && sampleCoord.y < gScreenSize.y) {
                
                float sampleShadow = gShadowMap[sampleCoord];
                float sampleDepth = gDepthBuffer[sampleCoord];
                
                // Depth discontinuity check - don't blur across edges
                float depthDiff = abs(sampleDepth - centerDepth);
                float depthThreshold = 0.05f; // Adjust based on scene scale
                
                if (depthDiff < depthThreshold) {
                    // Gaussian weight
                    float2 offset = float2(x, y);
                    float weight = exp(-dot(offset, offset) / (2.0f * blurRadius));
                    
                    shadowSum += sampleShadow * weight;
                    weightSum += weight;
                }
            }
        }
    }
    
    // Normalize and write result
    gBlurredShadows[pixelCoord] = (weightSum > 0) ? shadowSum / weightSum : centerShadow;
}
```

## Testing & Validation

### Test Scenes
1. **Simple Test Scene:** Create a scene with basic geometry (sphere, plane, cube) and a single directional light
2. **Complex Test Scene:** Add multiple objects with varying scales and orientations
3. **Performance Test Scene:** Create a scene with high polygon count to test optimization

### Validation Metrics
1. **Visual Quality Check:**
   - Soft shadows should show gradual transitions from light to dark
   - No harsh edges or artifacts at shadow boundaries
   - Correct penumbra size relative to light size and distances

2. **Performance Metrics:**
   - Frame time should remain stable (target < 16ms for 60 FPS)
   - Ray count per pixel should be optimized
   - Memory usage should be reasonable

3. **Edge Case Testing:**
   - Test with objects very close to light source
   - Test with objects at extreme distances
   - Test with transparent/translucent objects
   - Test with complex geometry and small details

## Common Pitfalls & Troubleshooting

### Issue 1: Self-Intersection Artifacts
**Symptoms:** Black speckles or incorrect shadows on object surfaces
**Solution:**
```cpp
// Increase ray offset (TMin) to avoid self-intersection
ray.TMin = 0.05f; // Adjust based on scene scale
// Or use normal offset technique
ray.Origin = worldPos + normal * 0.01f;
```

### Issue 2: Noisy Shadows in Cone Sampling
**Symptoms:** Grainy or speckled shadow appearance
**Solutions:**
1. Increase sample count (performance trade-off)
2. Implement temporal accumulation:
```hlsl
// In ray generation shader
float3 jitter = GetHaltonSequence(frameIndex % 16) * 2.0f - 1.0f;
jitter *= gInvScreenSize * 0.5f;
ray.Origin += jitter;
```
3. Apply spatial denoising filter after ray tracing

### Issue 3: Incorrect Penumbra Size in Filter Approach
**Symptoms:** Shadows too soft or too hard regardless of light size
**Solution:**
```hlsl
// Recalculate penumbra size with proper scaling
float CalculatePenumbraSize(float3 surfacePos, float lightRadius) {
    float distanceToLight = length(gLightPosition - surfacePos);
    float distanceToOccluder = GetDistanceToNearestOccluder(surfacePos);
    
    // Proper penumbra calculation
    float penumbraRatio = lightRadius / (distanceToLight - distanceToOccluder);
    return penumbraRatio * distanceToOccluder;
}
```

### Issue 4: Performance Bottlenecks
**Symptoms:** Low frame rates, stuttering
**Optimization Strategies:**
1. **Spatial Subsampling:** Calculate shadows at half resolution
2. **Adaptive Sampling:** Use fewer samples in flat shadow regions
3. **Culling:** Skip shadow rays for fragments already in deep shadow
4. **Level of Detail:** Reduce ray count based on distance from camera

## Advanced Features (Optional)

### 1. Colored Shadows from Area Lights
```hlsl
float3 CastColoredShadowRay(float3 origin, float3 direction, float3 lightColor) {
    // Similar to regular shadow ray but accumulates color absorption
    ColoredShadowPayload payload;
    payload.attenuation = float3(1.0f, 1.0f, 1.0f);
    
    TraceRay(gSceneAccelerationStructure, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    
    return payload.attenuation * lightColor;
}
```

### 2. Contact Hardening Shadows
```hlsl
// Adjust blur radius based on distance to occluder
float CalculateContactHardeningRadius(float3 surfacePos, float3 lightDir, float penumbraBase) {
    float distanceToOccluder = GetDistanceToNearestOccluder(surfacePos, lightDir);
    // Closer objects create harder shadows
    return penumbraBase * (1.0f - exp(-distanceToOccluder * 0.1f));
}
```

### 3. Hybrid Approach (Best of Both Worlds)
```hlsl
// Use cone sampling for nearby objects, filter approach for distant objects
float CalculateShadow(float3 surfacePos, float3 lightDir) {
    float distanceToCamera = length(surfacePos - gCameraPosition);
    
    if (distanceToCamera < 10.0f) {
        // Use cone sampling for close objects (better quality)
        return CalculateConeShadow(surfacePos, lightDir);
    } else {
        // Use filtered approach for distant objects (better performance)
        float hardShadow = CastShadowRay(surfacePos, lightDir);
        float penumbraSize = CalculatePenumbraSize(surfacePos, lightDir);
        return ApplyFilter(hardShadow, penumbraSize);
    }
}
```

## Submission Requirements

1. **Source Code:** Complete implementation with proper comments
2. **Documentation:** Brief explanation of your approach and any challenges faced
3. **Screenshots:** Before/after comparison showing soft shadow effect
4. **Performance Report:** Frame times with/without shadows, ray count per pixel
5. **Video Demo:** Short screen recording showing shadows in motion

## Cursor AI Integration Tips

1. **Use Cursor AI for Code Generation:**
   - Prompt: "Generate DirectX 12 ray tracing acceleration structure creation code"
   - Prompt: "Create HLSL shader for cone sampling shadow rays"

2. **Debugging Assistance:**
   - Prompt: "Why am I getting self-intersection artifacts in my ray traced shadows?"
   - Prompt: "How to optimize ray traced shadow performance in DX12?"

3. **Code Review:**
   - Select your code and ask: "Review this ray tracing shader for performance issues"

4. **Learning Resources:**
   - Prompt: "Explain DXR shader tables and their memory layout"
   - Prompt: "How does the Surface Area Heuristic work for BVH construction?"

This comprehensive guide provides everything needed to complete the soft ray-traced shadows homework assignment. Choose the approach that best fits your project requirements and skill level, and don't hesitate to leverage Cursor AI for implementation assistance and debugging.