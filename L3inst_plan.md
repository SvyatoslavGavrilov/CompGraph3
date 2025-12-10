# Atmospheric Scattering Implementation Plan for Labor3X

## [[Labor3X.cpp]] Atmospheric Integration

### Overview
This plan adds a complete atmospheric scattering system to the DirectX 12 rendering pipeline. The implementation adapts the Hoffman-Preetham approach for ground-level rendering with support for high-altitude views, based on the lecture material from [[Light Volume Scattering]].

**Important Note**: All main changes will be made inside `Labor3X.cpp` without creating additional header files. A refactoring guide is provided at the end for future extraction into separate files.

### Prerequisites
- DirectX 12 support
- DirectXMath library (already included via MathHelper.h)
- ImGui for real-time parameter adjustment (already included)
- Existing rendering pipeline with camera controls (already implemented)

---

## Step 1: Add Atmospheric Constants and Parameters

**Location**: Add after existing includes but before the `RenderItem` struct definition (around line 20)

```cpp
// Atmospheric scattering constants
const float ATMOSPHERE_PI = 3.14159265359f;
const float ATMOSPHERE_DEG_TO_RAD = ATMOSPHERE_PI / 180.0f;

// [[Atmospheric Parameters]]
struct AtmosphereParameters {
    // Earth-like atmosphere defaults
    float planetRadius = 6371000.0f;    // Earth radius in meters
    float atmosphereRadius = 6471000.0f; // Atmosphere radius (100km above surface)
    
    // Rayleigh scattering coefficients (clear atmosphere)
    // RGB in m^-1 - stored as XMFLOAT3 for DirectX compatibility
    DirectX::XMFLOAT3 rayleighCoeff = DirectX::XMFLOAT3(5.5e-6f, 13.5e-6f, 33.1e-6f);
    
    // Mie scattering coefficients (hazy atmosphere)
    DirectX::XMFLOAT3 mieCoeff = DirectX::XMFLOAT3(2.0e-5f, 2.0e-5f, 2.0e-5f); // Gray value for haze
    
    // Optical properties
    float rayleighScaleHeight = 8000.0f;  // Height where Rayleigh density falls to 1/e
    float mieScaleHeight = 1200.0f;       // Height where Mie density falls to 1/e
    float mieAnisotropy = 0.76f;          // Forward scattering bias (0.76 for Earth)
    
    // Sun properties
    DirectX::XMFLOAT3 sunDirection = DirectX::XMFLOAT3(0.0f, -0.5f, -1.0f); // Default sun position
    float sunIntensity = 20.0f;           // Sun brightness
    
    // Runtime adjustment parameters
    float atmosphereDensity = 1.0f;      // Global density multiplier (1.0 = Earth normal)
    float pollutionLevel = 0.0f;          // 0.0 = clean, 1.0 = heavily polluted
    float humidityLevel = 0.3f;           // 0.0 = dry, 1.0 = very humid
    
    // Recalculate coefficients based on pollution/humidity
    void updateCoefficients() {
        // More pollution increases Mie scattering (haze)
        float pollutionFactor = 1.0f + pollutionLevel * 4.0f;
        // More humidity increases both scattering types but affects Mie more
        float humidityFactor = 1.0f + humidityLevel * 2.0f;
        
        // Base Earth coefficients adjusted by density and pollution
        DirectX::XMVECTOR baseRayleigh = DirectX::XMVectorSet(5.5e-6f, 13.5e-6f, 33.1e-6f, 0.0f);
        DirectX::XMVECTOR baseMie = DirectX::XMVectorSet(2.0e-5f, 2.0e-5f, 2.0e-5f, 0.0f);
        
        DirectX::XMVECTOR densityVec = DirectX::XMVectorReplicate(atmosphereDensity * humidityFactor);
        DirectX::XMVECTOR pollutionVec = DirectX::XMVectorReplicate(pollutionFactor * humidityFactor);
        
        DirectX::XMVECTOR rayleighVec = DirectX::XMVectorMultiply(baseRayleigh, densityVec);
        DirectX::XMVECTOR mieVec = DirectX::XMVectorMultiply(baseMie, DirectX::XMVectorMultiply(densityVec, pollutionVec));
        
        DirectX::XMStoreFloat3(&rayleighCoeff, rayleighVec);
        DirectX::XMStoreFloat3(&mieCoeff, mieVec);
        
        // Adjust scale heights based on conditions
        rayleighScaleHeight = 8000.0f * (1.0f - pollutionLevel * 0.3f);
        mieScaleHeight = 1200.0f * (1.0f + pollutionLevel * 0.5f);
    }
};

// Global atmosphere instance
AtmosphereParameters gAtmosphere;
```

**Commentary**: 
- Uses DirectXMath (XMFLOAT3, XMVECTOR) instead of GLM for compatibility with DirectX 12
- All atmospheric parameters are stored in a single struct for easy management
- The `updateCoefficients()` method recalculates scattering coefficients based on environmental conditions

---

## Step 2: Add Atmospheric Constant Buffer Structures

**Location**: Add after `PassConstants` struct in `Labor3XFrameResource.h` (or add inline in Labor3X.cpp before the class definition)

```cpp
// Atmospheric scattering constant buffer
struct AtmosphereConstants
{
    DirectX::XMFLOAT3 sunDirection;
    float planetRadius;
    DirectX::XMFLOAT3 rayleighCoeff;
    float atmosphereRadius;
    DirectX::XMFLOAT3 mieCoeff;
    float rayleighScaleHeight;
    float mieScaleHeight;
    float mieAnisotropy;
    float sunIntensity;
    float cameraHeight;
    float padding[2]; // Align to 16-byte boundary
};
```

**Commentary**: 
- Constant buffer structure for passing atmospheric parameters to shaders
- Uses proper alignment for DirectX 12 constant buffers (16-byte alignment)
- Camera height is included for altitude-dependent calculations

---

## Step 3: Add Atmospheric Shader Files

**Location**: Create new shader files in `Shaders/` directory

### File: `Shaders/Atmosphere.hlsl`

```hlsl
// Atmospheric Scattering Shader
// Based on Hoffman-Preetham approach

cbuffer AtmosphereCB : register(b2)
{
    float3 sunDirection;
    float planetRadius;
    float3 rayleighCoeff;
    float atmosphereRadius;
    float3 mieCoeff;
    float rayleighScaleHeight;
    float mieScaleHeight;
    float mieAnisotropy;
    float sunIntensity;
    float cameraHeight;
    float2 padding;
};

// [[Rayleigh Phase Function]]
// Describes angular distribution of Rayleigh scattered light
// Isotropic scattering (equal in all directions)
float rayleighPhaseFunction(float cosTheta)
{
    return (3.0 / (16.0 * 3.14159265359)) * (1.0 + cosTheta * cosTheta);
}

// [[Mie Phase Function]]
// Describes angular distribution of Mie scattered light
// Forward-scattering dominant (g > 0)
float miePhaseFunction(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 / (4.0 * 3.14159265359)) * ((1.0 - g2) * (1.0 + cosTheta * cosTheta)) / 
           (pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5) * (2.0 + g2));
}

// [[Optical Depth Calculation]]
// Calculates the optical depth along a ray through the atmosphere
float calculateOpticalDepth(float3 startPoint, float3 direction, float rayLength, 
                           float scaleHeight, float planetRadius)
{
    const int steps = 16; // Number of samples for integration
    float opticalDepth = 0.0;
    float stepSize = rayLength / float(steps);
    
    for (int i = 0; i < steps; i++)
    {
        float t = (float(i) + 0.5) * stepSize;
        float3 samplePoint = startPoint + direction * t;
        float height = length(samplePoint) - planetRadius;
        
        // Atmospheric density decays exponentially with height
        float density = exp(-height / scaleHeight);
        opticalDepth += density * stepSize;
    }
    
    return opticalDepth;
}

struct VertexIn
{
    float3 PosL : POSITION;
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 ViewDir : TEXCOORD0;
    float3 RayleighColor : TEXCOORD1;
    float3 MieColor : TEXCOORD2;
    float OpticalDepth : TEXCOORD3;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // Transform to world space (assuming identity world matrix for skydome)
    vout.PosW = vin.PosL;
    
    // Calculate view direction
    float3 cameraPos = float3(0.0, cameraHeight, 0.0);
    vout.ViewDir = normalize(vout.PosW - cameraPos);
    
    // Calculate optical depth from camera to vertex
    float cameraToVertexDistance = length(vout.PosW - cameraPos);
    float rayleighDepth = calculateOpticalDepth(cameraPos, vout.ViewDir, 
                                              cameraToVertexDistance, rayleighScaleHeight, 
                                              planetRadius);
    float mieDepth = calculateOpticalDepth(cameraPos, vout.ViewDir, 
                                          cameraToVertexDistance, mieScaleHeight, 
                                          planetRadius);
    
    vout.OpticalDepth = rayleighDepth;
    
    // Calculate extinction (light loss due to scattering and absorption)
    float3 extinction = exp(-(rayleighCoeff * rayleighDepth + mieCoeff * mieDepth));
    
    // Calculate in-scattering from the sun
    float3 sunDir = normalize(sunDirection);
    float cosTheta = dot(vout.ViewDir, sunDir);
    float rayleighPhase = rayleighPhaseFunction(cosTheta);
    float miePhase = miePhaseFunction(cosTheta, mieAnisotropy);
    
    // Optical depth to sun
    float sunRayLength = sqrt(atmosphereRadius * atmosphereRadius - 
                            planetRadius * planetRadius);
    
    float sunRayleighDepth = calculateOpticalDepth(vout.PosW, sunDir, 
                                                  sunRayLength, rayleighScaleHeight, 
                                                  planetRadius);
    float sunMieDepth = calculateOpticalDepth(vout.PosW, sunDir, 
                                             sunRayLength, mieScaleHeight, 
                                             planetRadius);
    
    float3 sunExtinction = exp(-(rayleighCoeff * sunRayleighDepth + 
                             mieCoeff * sunMieDepth));
    
    // Combine phase functions with extinction and sun intensity
    vout.RayleighColor = rayleighPhase * rayleighCoeff * sunExtinction * sunIntensity;
    vout.MieColor = miePhase * mieCoeff * sunExtinction * sunIntensity;
    
    // Transform to clip space (will be set in main app with proper matrices)
    vout.PosH = float4(vin.PosL, 1.0);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Calculate final color with both Rayleigh and Mie scattering
    float3 rayleighContribution = pin.RayleighColor;
    float3 mieContribution = pin.MieColor;
    
    // Combine contributions
    float3 finalColor = rayleighContribution + mieContribution;
    
    // [[Atmospheric Extinction]]
    // Apply the Beer-Lambert-Bouguer law for light attenuation
    float extinctionFactor = exp(-pin.OpticalDepth * (length(rayleighCoeff) + length(mieCoeff)));
    finalColor *= extinctionFactor;
    
    // Add some ambient light to avoid completely black areas
    finalColor += float3(0.05, 0.07, 0.1) * (1.0 - extinctionFactor);
    
    // Gamma correction
    finalColor = pow(finalColor, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));
    
    // Clamp to valid color range
    finalColor = saturate(finalColor);
    
    return float4(finalColor, 1.0);
}
```

**Commentary**: 
- DirectX 12 HLSL shader implementing atmospheric scattering
- Uses constant buffer (register b2) for atmospheric parameters
- Implements Rayleigh and Mie phase functions
- Calculates optical depth using numerical integration

---

## Step 4: Add Skydome Geometry Generation Function

**Location**: Add as a private member function in `Labor3XApp` class, before `BuildCubeGeometry()` (around line 290)

```cpp
void Labor3XApp::BuildSkydomeGeometry()
{
    /* 
    [[Skydome Geometry]]
    Creates a hemisphere mesh for rendering the sky.
    Uses icosahedron subdivision for even vertex distribution.
    */
    
    const int subdivisions = 3; // Quality level (3 = good balance)
    const float radius = gAtmosphere.atmosphereRadius + 1000.0f; // Slightly larger than atmosphere
    
    // Icosahedron vertices (12 vertices)
    std::vector<DirectX::XMFLOAT3> vertices = {
        DirectX::XMFLOAT3(-0.525731f, 0.000000f, 0.850651f), 
        DirectX::XMFLOAT3(0.525731f, 0.000000f, 0.850651f),
        DirectX::XMFLOAT3(-0.525731f, 0.000000f, -0.850651f), 
        DirectX::XMFLOAT3(0.525731f, 0.000000f, -0.850651f),
        DirectX::XMFLOAT3(0.000000f, 0.850651f, 0.525731f), 
        DirectX::XMFLOAT3(0.000000f, 0.850651f, -0.525731f),
        DirectX::XMFLOAT3(0.000000f, -0.850651f, 0.525731f), 
        DirectX::XMFLOAT3(0.000000f, -0.850651f, -0.525731f),
        DirectX::XMFLOAT3(0.850651f, 0.525731f, 0.000000f), 
        DirectX::XMFLOAT3(-0.850651f, 0.525731f, 0.000000f),
        DirectX::XMFLOAT3(0.850651f, -0.525731f, 0.000000f), 
        DirectX::XMFLOAT3(-0.850651f, -0.525731f, 0.000000f)
    };
    
    // Icosahedron triangles (20 faces)
    struct Triangle { uint32_t v0, v1, v2; };
    std::vector<Triangle> triangles = {
        {0, 4, 1}, {0, 9, 4}, {9, 5, 4}, {4, 5, 8}, {4, 8, 1},
        {8, 10, 1}, {8, 3, 10}, {5, 3, 8}, {5, 2, 3}, {2, 7, 3},
        {7, 10, 3}, {7, 6, 10}, {7, 11, 6}, {11, 0, 6}, {0, 1, 6},
        {6, 1, 10}, {9, 0, 11}, {9, 11, 2}, {9, 2, 5}, {7, 2, 11}
    };
    
    // Normalize vertices to unit sphere
    for (auto& v : vertices) {
        DirectX::XMVECTOR vec = DirectX::XMLoadFloat3(&v);
        vec = DirectX::XMVector3Normalize(vec);
        DirectX::XMStoreFloat3(&v, vec);
    }
    
    // Subdivide triangles
    for (int i = 0; i < subdivisions; i++) {
        std::vector<Triangle> newTriangles;
        size_t originalCount = triangles.size();
        
        for (size_t j = 0; j < originalCount; j++) {
            Triangle tri = triangles[j];
            DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&vertices[tri.v0]);
            DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&vertices[tri.v1]);
            DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&vertices[tri.v2]);
            
            // Calculate midpoints
            DirectX::XMVECTOR mid01 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(v0, v1));
            DirectX::XMVECTOR mid12 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(v1, v2));
            DirectX::XMVECTOR mid20 = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(v2, v0));
            
            // Add new vertices
            DirectX::XMFLOAT3 mid01f, mid12f, mid20f;
            DirectX::XMStoreFloat3(&mid01f, mid01);
            DirectX::XMStoreFloat3(&mid12f, mid12);
            DirectX::XMStoreFloat3(&mid20f, mid20);
            
            size_t idx01 = vertices.size(); vertices.push_back(mid01f);
            size_t idx12 = vertices.size(); vertices.push_back(mid12f);
            size_t idx20 = vertices.size(); vertices.push_back(mid20f);
            
            // Create 4 new triangles
            newTriangles.push_back({tri.v0, (uint32_t)idx01, (uint32_t)idx20});
            newTriangles.push_back({tri.v1, (uint32_t)idx12, (uint32_t)idx01});
            newTriangles.push_back({tri.v2, (uint32_t)idx20, (uint32_t)idx12});
            newTriangles.push_back({(uint32_t)idx01, (uint32_t)idx12, (uint32_t)idx20});
        }
        
        triangles = newTriangles;
    }
    
    // Convert to hemisphere (only keep vertices with y >= 0)
    std::vector<DirectX::XMFLOAT3> hemisphereVertices;
    std::vector<std::uint16_t> hemisphereIndices;
    
    std::unordered_map<size_t, size_t> vertexMap;
    
    for (const auto& tri : triangles) {
        DirectX::XMFLOAT3 v0 = vertices[tri.v0];
        DirectX::XMFLOAT3 v1 = vertices[tri.v1];
        DirectX::XMFLOAT3 v2 = vertices[tri.v2];
        
        // Skip triangles completely below horizon
        if (v0.y < 0.0f && v1.y < 0.0f && v2.y < 0.0f) continue;
        
        // Clip triangles that cross horizon - simple approach: only include vertices above horizon
        if (v0.y >= 0.0f || v1.y >= 0.0f || v2.y >= 0.0f) {
            auto getVertexIndex = [&](size_t srcIdx, const DirectX::XMFLOAT3& pos) -> uint16_t {
                if (vertexMap.find(srcIdx) == vertexMap.end()) {
                    DirectX::XMFLOAT3 clippedPos = DirectX::XMFLOAT3(pos.x, std::max(0.0f, pos.y), pos.z);
                    hemisphereVertices.push_back(clippedPos);
                    vertexMap[srcIdx] = hemisphereVertices.size() - 1;
                }
                return (uint16_t)vertexMap[srcIdx];
            };
            
            hemisphereIndices.push_back(getVertexIndex(tri.v0, v0));
            hemisphereIndices.push_back(getVertexIndex(tri.v1, v1));
            hemisphereIndices.push_back(getVertexIndex(tri.v2, v2));
        }
    }
    
    // Scale vertices to skydome radius
    for (auto& v : hemisphereVertices) {
        v.x *= radius;
        v.y *= radius;
        v.z *= radius;
    }
    
    // Convert to Vertex format (Pos + Color)
    // For skydome, we use white color as base (shader will calculate atmospheric colors)
    std::vector<Vertex> skydomeVertices(hemisphereVertices.size());
    for (size_t i = 0; i < hemisphereVertices.size(); ++i) {
        skydomeVertices[i] = Vertex(hemisphereVertices[i], DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    
    const UINT vbByteSize = (UINT)skydomeVertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)hemisphereIndices.size() * sizeof(std::uint16_t);
    
    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "skydomeGeo";
    
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), skydomeVertices.data(), vbByteSize);
    
    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), hemisphereIndices.data(), ibByteSize);
    
    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), skydomeVertices.data(), vbByteSize, geo->VertexBufferUploader);
    
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), hemisphereIndices.data(), ibByteSize, geo->IndexBufferUploader);
    
    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;
    
    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)hemisphereIndices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    
    geo->DrawArgs["skydome"] = submesh;
    
    mGeometries[geo->Name] = std::move(geo);
}
```

**Commentary**: 
- Generates a hemisphere mesh using icosahedron subdivision
- Uses DirectXMath for vector operations
- Creates geometry compatible with existing MeshGeometry structure
- Vertices are scaled to atmosphere radius

---

## Step 5: Add Atmospheric Shader Compilation and Root Signature Updates

**Location**: Modify `BuildShadersAndInputLayout()` function (around line 278)

**Find**:
```cpp
void Labor3XApp::BuildShadersAndInputLayout()
{
    mShaders["pyramidVS"] = d3dUtil::CompileShader(L"Shaders\\Pyramid.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["pyramidPS"] = d3dUtil::CompileShader(L"Shaders\\Pyramid.hlsl", nullptr, "PS", "ps_5_1");
    // ... rest of function
}
```

**Replace with**:
```cpp
void Labor3XApp::BuildShadersAndInputLayout()
{
    mShaders["pyramidVS"] = d3dUtil::CompileShader(L"Shaders\\Pyramid.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["pyramidPS"] = d3dUtil::CompileShader(L"Shaders\\Pyramid.hlsl", nullptr, "PS", "ps_5_1");
    
    // Atmospheric scattering shaders
    mShaders["atmosphereVS"] = d3dUtil::CompileShader(L"Shaders\\Atmosphere.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["atmospherePS"] = d3dUtil::CompileShader(L"Shaders\\Atmosphere.hlsl", nullptr, "PS", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}
```

**Location**: Modify `BuildRootSignature()` function (around line 249)

**Find**:
```cpp
void Labor3XApp::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];

    slotRootParameter[0].InitAsConstantBufferView(0); // Object constants (b0)
    slotRootParameter[1].InitAsConstantBufferView(1); // Pass constants (b1)
    // ... rest of function
}
```

**Replace with**:
```cpp
void Labor3XApp::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[3];

    slotRootParameter[0].InitAsConstantBufferView(0); // Object constants (b0)
    slotRootParameter[1].InitAsConstantBufferView(1); // Pass constants (b1)
    slotRootParameter[2].InitAsConstantBufferView(2); // Atmosphere constants (b2)

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    // ... rest of function remains the same
}
```

**Commentary**: 
- Adds atmospheric shader compilation
- Extends root signature to include atmosphere constant buffer
- Maintains compatibility with existing shaders

---

## Step 6: Add Atmospheric Constant Buffer to Frame Resources

**Location**: Modify `Labor3XFrameResource.h` to add atmosphere constant buffer

**Find** in `Labor3XFrameResource.h`:
```cpp
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
```

**Add after** (add the AtmosphereConstants struct definition first if not already added):
```cpp
    std::unique_ptr<UploadBuffer<AtmosphereConstants>> AtmosphereCB = nullptr;
```

**Location**: Modify `Labor3XFrameResource.cpp` constructor

**Find**:
```cpp
Labor3XFrameResource::Labor3XFrameResource(ID3D12Device* device, UINT passCount, UINT objectCount)
{
    // ... existing code
    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
}
```

**Add**:
```cpp
    AtmosphereCB = std::make_unique<UploadBuffer<AtmosphereConstants>>(device, 1, true);
```

**Location**: Modify `BuildFrameResources()` in `Labor3X.cpp` (around line 380)

**Find**:
```cpp
void Labor3XApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<Labor3XFrameResource>(md3dDevice.Get(),
            1, 1));
    }
}
```

**No changes needed** - the frame resource constructor handles the atmosphere buffer allocation.

**Commentary**: 
- Adds atmosphere constant buffer to frame resources
- One buffer per frame for triple buffering support

---

## Step 7: Add Skydome Render Item

**Location**: Modify `BuildRenderItems()` function (around line 389)

**Find**:
```cpp
void Labor3XApp::BuildRenderItems()
{
    auto cubeRitem = std::make_unique<RenderItem>();
    // ... cube setup
    mAllRitems.push_back(std::move(cubeRitem));
}
```

**Add after cube render item**:
```cpp
    // Skydome render item
    auto skydomeRitem = std::make_unique<RenderItem>();
    skydomeRitem->World = MathHelper::Identity4x4(); // Skydome is always at origin
    skydomeRitem->ObjCBIndex = 1; // Different from cube
    skydomeRitem->Geo = mGeometries["skydomeGeo"].get();
    skydomeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skydomeRitem->IndexCount = skydomeRitem->Geo->DrawArgs["skydome"].IndexCount;
    skydomeRitem->StartIndexLocation = skydomeRitem->Geo->DrawArgs["skydome"].StartIndexLocation;
    skydomeRitem->BaseVertexLocation = skydomeRitem->Geo->DrawArgs["skydome"].BaseVertexLocation;
    mAllRitems.push_back(std::move(skydomeRitem));
```

**Location**: Modify `BuildFrameResources()` to allocate for 2 objects instead of 1

**Find**:
```cpp
        mFrameResources.push_back(std::make_unique<Labor3XFrameResource>(md3dDevice.Get(),
            1, 1));
```

**Replace with**:
```cpp
        mFrameResources.push_back(std::make_unique<Labor3XFrameResource>(md3dDevice.Get(),
            1, 2)); // 1 pass, 2 objects (cube + skydome)
```

**Commentary**: 
- Creates render item for skydome
- Uses separate object constant buffer index

---

## Step 8: Add Atmospheric PSO

**Location**: Modify `BuildPSOs()` function (around line 351)

**Find**:
```cpp
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));
}
```

**Add before the closing brace**:
```cpp
    // Atmospheric scattering PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC atmospherePsoDesc = psoDesc;
    atmospherePsoDesc.VS = 
    { 
        reinterpret_cast<BYTE*>(mShaders["atmosphereVS"]->GetBufferPointer()), 
        mShaders["atmosphereVS"]->GetBufferSize()
    };
    atmospherePsoDesc.PS = 
    { 
        reinterpret_cast<BYTE*>(mShaders["atmospherePS"]->GetBufferPointer()),
        mShaders["atmospherePS"]->GetBufferSize()
    };
    // Disable depth writing for skydome (always behind everything)
    atmospherePsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&atmospherePsoDesc, IID_PPV_ARGS(&mPSOs["atmosphere"])));
}
```

**Commentary**: 
- Creates separate PSO for atmospheric rendering
- Disables depth writing for skydome

---

## Step 9: Add Atmosphere Update Function

**Location**: Add new function after `UpdatePassCB()` (around line 431)

```cpp
void Labor3XApp::UpdateAtmosphereCB(const GameTimer& gt)
{
    auto currAtmosphereCB = mCurrFrameResource->AtmosphereCB.get();
    
    AtmosphereConstants atmConstants;
    
    // Update atmosphere coefficients
    gAtmosphere.updateCoefficients();
    
    // Normalize sun direction
    DirectX::XMVECTOR sunDir = DirectX::XMLoadFloat3(&gAtmosphere.sunDirection);
    sunDir = DirectX::XMVector3Normalize(sunDir);
    DirectX::XMStoreFloat3(&atmConstants.sunDirection, sunDir);
    
    atmConstants.planetRadius = gAtmosphere.planetRadius;
    atmConstants.atmosphereRadius = gAtmosphere.atmosphereRadius;
    atmConstants.rayleighCoeff = gAtmosphere.rayleighCoeff;
    atmConstants.mieCoeff = gAtmosphere.mieCoeff;
    atmConstants.rayleighScaleHeight = gAtmosphere.rayleighScaleHeight;
    atmConstants.mieScaleHeight = gAtmosphere.mieScaleHeight;
    atmConstants.mieAnisotropy = gAtmosphere.mieAnisotropy;
    atmConstants.sunIntensity = gAtmosphere.sunIntensity;
    
    // Calculate camera height above planet surface
    DirectX::XMVECTOR cameraPos = mCamera.GetPositionXM();
    DirectX::XMFLOAT3 cameraPosF;
    DirectX::XMStoreFloat3(&cameraPosF, cameraPos);
    atmConstants.cameraHeight = cameraPosF.y; // Assuming Y is up
    
    currAtmosphereCB->CopyData(0, atmConstants);
}
```

**Location**: Add function declaration to class private section (around line 66)

**Add**:
```cpp
    void UpdateAtmosphereCB(const GameTimer& gt);
```

**Location**: Call in `Update()` function (around line 205)

**Find**:
```cpp
    UpdateObjectCBs(gt);
    UpdatePassCB(gt);
}
```

**Add**:
```cpp
    UpdateObjectCBs(gt);
    UpdatePassCB(gt);
    UpdateAtmosphereCB(gt);
}
```

**Commentary**: 
- Updates atmosphere constant buffer each frame
- Calculates camera height for altitude-dependent effects

---

## Step 10: Modify Draw Function to Render Skydome

**Location**: Modify `Draw()` function (around line 209)

**Find**:
```cpp
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get());
```

**Replace with**:
```cpp
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());
    
    // Set atmosphere constant buffer
    auto atmosphereCB = mCurrFrameResource->AtmosphereCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(2, atmosphereCB->GetGPUVirtualAddress());

    // Draw skydome first (background)
    mCommandList->SetPipelineState(mPSOs["atmosphere"].Get());
    // Find and draw skydome render item
    for (auto& ri : mAllRitems) {
        if (ri->Geo->Name == "skydomeGeo") {
            UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
            auto objectCB = mCurrFrameResource->ObjectCB->Resource();
            D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
            mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);
            
            mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
            mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
            mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);
            mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
            break;
        }
    }
    
    // Draw regular scene objects
    mCommandList->SetPipelineState(mPSOs["opaque"].Get());
    DrawRenderItems(mCommandList.Get());
```

**Commentary**: 
- Renders skydome first using atmosphere PSO
- Then renders regular scene with opaque PSO
- Sets atmosphere constant buffer before rendering

---

## Step 11: Add ImGui Control Panel

**Location**: Add new function after `DrawRenderItems()` (around line 452)

```cpp
void Labor3XApp::DrawAtmosphereUI()
{
    /* 
    [[Real-time Parameter Control]]
    Creates an ImGui control panel for adjusting atmospheric parameters in real-time.
    This allows artists and developers to see the effects of parameter changes immediately.
    */
    
    // Start ImGui frame (assuming ImGui is already initialized in d3dApp)
    // This should be called from Draw() before Present
    
    ImGui::Begin("Atmosphere Controls");
    
    ImGui::Text("Atmospheric Conditions");
    ImGui::Separator();
    
    // Sun controls
    ImGui::Text("Sun Direction");
    static float sunElevation = -30.0f; // Degrees from horizon
    static float sunAzimuth = 180.0f;   // Degrees from north
    
    if (ImGui::SliderFloat("Elevation", &sunElevation, -90.0f, 90.0f, "%.1f°")) {
        // Convert spherical coordinates to Cartesian
        float elevationRad = sunElevation * ATMOSPHERE_DEG_TO_RAD;
        float azimuthRad = sunAzimuth * ATMOSPHERE_DEG_TO_RAD;
        gAtmosphere.sunDirection.x = cosf(elevationRad) * cosf(azimuthRad);
        gAtmosphere.sunDirection.y = sinf(elevationRad);
        gAtmosphere.sunDirection.z = cosf(elevationRad) * sinf(azimuthRad);
    }
    
    if (ImGui::SliderFloat("Azimuth", &sunAzimuth, 0.0f, 360.0f, "%.1f°")) {
        float elevationRad = sunElevation * ATMOSPHERE_DEG_TO_RAD;
        float azimuthRad = sunAzimuth * ATMOSPHERE_DEG_TO_RAD;
        gAtmosphere.sunDirection.x = cosf(elevationRad) * cosf(azimuthRad);
        gAtmosphere.sunDirection.y = sinf(elevationRad);
        gAtmosphere.sunDirection.z = cosf(elevationRad) * sinf(azimuthRad);
    }
    
    ImGui::SliderFloat("Sun Intensity", &gAtmosphere.sunIntensity, 0.1f, 100.0f, "%.1f");
    
    ImGui::Separator();
    
    // Atmospheric quality controls
    ImGui::Text("Atmospheric Quality");
    
    if (ImGui::SliderFloat("Density", &gAtmosphere.atmosphereDensity, 0.1f, 5.0f, "%.2f")) {
        gAtmosphere.updateCoefficients();
    }
    
    if (ImGui::SliderFloat("Pollution", &gAtmosphere.pollutionLevel, 0.0f, 1.0f, "%.2f")) {
        gAtmosphere.updateCoefficients();
    }
    
    if (ImGui::SliderFloat("Humidity", &gAtmosphere.humidityLevel, 0.0f, 1.0f, "%.2f")) {
        gAtmosphere.updateCoefficients();
    }
    
    ImGui::Separator();
    
    // Preset buttons
    ImGui::Text("Presets");
    if (ImGui::Button("Clear Sky")) {
        gAtmosphere.atmosphereDensity = 1.0f;
        gAtmosphere.pollutionLevel = 0.0f;
        gAtmosphere.humidityLevel = 0.2f;
        gAtmosphere.sunIntensity = 20.0f;
        gAtmosphere.updateCoefficients();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Hazy Day")) {
        gAtmosphere.atmosphereDensity = 1.2f;
        gAtmosphere.pollutionLevel = 0.4f;
        gAtmosphere.humidityLevel = 0.5f;
        gAtmosphere.sunIntensity = 15.0f;
        gAtmosphere.updateCoefficients();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Heavy Pollution")) {
        gAtmosphere.atmosphereDensity = 1.5f;
        gAtmosphere.pollutionLevel = 0.8f;
        gAtmosphere.humidityLevel = 0.6f;
        gAtmosphere.sunIntensity = 10.0f;
        gAtmosphere.updateCoefficients();
    }
    
    ImGui::End();
}
```

**Location**: Add function declaration to class private section (around line 66)

**Add**:
```cpp
    void DrawAtmosphereUI();
```

**Location**: Call in `Draw()` function before Present (around line 240)

**Find**:
```cpp
    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
```

**Add before Present** (assuming ImGui rendering is handled in d3dApp base class):
```cpp
    // Note: ImGui rendering should be handled in d3dApp base class
    // This function prepares the UI data, actual rendering happens elsewhere
    DrawAtmosphereUI();
```

**Commentary**: 
- Creates ImGui panel for real-time atmospheric parameter adjustment
- Provides presets for common atmospheric conditions
- Updates coefficients when sliders change

---

## Step 12: Initialize Atmosphere System

**Location**: Modify `Initialize()` function (around line 126)

**Find**:
```cpp
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildCubeGeometry();
    BuildRenderItems();
    BuildPSOs();
    BuildFrameResources();
```

**Add after BuildFrameResources()**:
```cpp
    // Initialize atmosphere system
    gAtmosphere.updateCoefficients();
    BuildSkydomeGeometry();
```

**Commentary**: 
- Initializes atmosphere parameters
- Builds skydome geometry during initialization

---

## Step 13: Update Object CBs for Skydome

**Location**: Modify `UpdateObjectCBs()` function (around line 403)

**Find**:
```cpp
void Labor3XApp::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    
    // Update cube rotation
    XMMATRIX world = XMMatrixRotationY(mCubeRotation);
    XMMATRIX viewProj = XMLoadFloat4x4(&mView) * XMLoadFloat4x4(&mProj);

    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
    XMStoreFloat4x4(&objConstants.ViewProj, XMMatrixTranspose(viewProj));

    currObjectCB->CopyData(0, objConstants);
}
```

**Replace with**:
```cpp
void Labor3XApp::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    
    // Update cube rotation
    XMMATRIX world = XMMatrixRotationY(mCubeRotation);
    XMMATRIX viewProj = XMLoadFloat4x4(&mView) * XMLoadFloat4x4(&mProj);

    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
    XMStoreFloat4x4(&objConstants.ViewProj, XMMatrixTranspose(viewProj));

    currObjectCB->CopyData(0, objConstants);
    
    // Update skydome (always at origin, follows camera)
    // For skydome, we want it centered at camera position but in world space
    XMMATRIX skydomeWorld = XMMatrixIdentity();
    XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(skydomeWorld));
    currObjectCB->CopyData(1, objConstants);
}
```

**Commentary**: 
- Updates both cube and skydome object constant buffers
- Skydome uses identity world matrix (centered at origin)

---

## Step 14: Fix Texture Paths

**Location**: Check `Models/sponza.mtl` file

The texture paths in the .mtl file use relative paths like `textures/lion.dds`. These should work correctly if:
1. The model loader resolves paths relative to the .mtl file location
2. Textures are in `Textures/textures/` subdirectory

**No code changes needed** if the texture loading code already handles relative paths correctly. If textures fail to load, check the model loading code in `model.cpp` to ensure it resolves paths relative to the .mtl file.

**Commentary**: 
- Texture paths in .mtl files are relative and should resolve correctly
- Textures are located in `Textures/textures/` subdirectory

---

## [[Refactoring Guide]] Future Improvements

### File Structure Plan
Once the single-file implementation is working, refactor into separate files:

```
labor_3_x/src/
├── rendering/
│   ├── AtmosphereSystem.cpp/hpp          // Main atmosphere system
│   ├── SkydomeRenderer.cpp/hpp           // Skydome mesh and rendering
│   ├── AtmosphericScattering.cpp/hpp     // Scattering calculations
│   └── AtmosphereUI.cpp/hpp              // ImGui control panel
├── shaders/
│   ├── Atmosphere.hlsl                   // Atmospheric scattering shader
│   └── (existing shaders)
└── Labor3X.cpp                           // Main application (simplified)
```

### Key Refactoring Steps

1. **Extract Atmosphere System**: 
   - Move `AtmosphereParameters` struct to `AtmosphereSystem.h`
   - Move `gAtmosphere` instance to `AtmosphereSystem.cpp`
   - Create `AtmosphereSystem` class with initialization and update methods

2. **Separate Rendering Logic**: 
   - Create `SkydomeRenderer` class for skydome geometry generation and rendering
   - Move `BuildSkydomeGeometry()` to `SkydomeRenderer::BuildGeometry()`
   - Move skydome rendering code to `SkydomeRenderer::Render()`

3. **Externalize Shaders**: 
   - Shader is already in external file `Shaders/Atmosphere.hlsl`
   - No changes needed

4. **Create UI Component**: 
   - Extract `DrawAtmosphereUI()` to `AtmosphereUI.cpp/hpp`
   - Create `AtmosphereUIController` class

5. **Add Configuration**: 
   - Create JSON/XML configuration for preset atmospheres
   - Add save/load functionality for atmospheric presets

### Performance Optimizations
- Implement texture lookups for precomputed scattering integrals
- Add level-of-detail for skydome based on distance
- Implement temporal anti-aliasing for atmospheric effects
- Add compute shader support for high-altitude rendering
- Cache optical depth calculations

### Advanced Features to Add
- Dynamic weather transitions
- Volumetric clouds integration
- Multiple scattering approximation
- Night sky with stars and moon
- Atmospheric shadow casting
- Time-of-day system with automatic sun position

---

## [[Implementation Notes]] Key Concepts

### [[Rayleigh vs Mie Scattering]]
- **Rayleigh scattering**: Dominant for clear skies, wavelength-dependent (blue scatters more)
- **Mie scattering**: Dominant for hazy conditions, less wavelength-dependent, forward-scattering dominant
- The ratio of these coefficients determines sky color and haze appearance

### [[Hoffman-Preetham Approach]]
The implementation follows the Hoffman-Preetham method:
- Precomputes expensive integrals at vertices
- Uses linear interpolation in fragment shader
- Assumes constant atmospheric density (good for ground-level views)
- Efficient for real-time rendering

### [[DirectX 12 Adaptations]]
- Uses DirectXMath instead of GLM
- Constant buffers instead of OpenGL uniforms
- HLSL shaders instead of GLSL
- Pipeline State Objects (PSOs) for shader state
- Root signatures for resource binding

### [[Real-time Parameter Adjustment]]
The ImGui panel allows real-time adjustment of:
- **Pollution level**: Increases Mie scattering, creates haze
- **Humidity**: Affects both scattering types, creates more diffuse lighting
- **Atmospheric density**: Global multiplier for all scattering effects
- **Sun position**: Changes lighting direction and sky colors

### [[Optical Depth Calculation]]
The optical depth calculation is crucial for realistic atmospheric effects:
- Uses numerical integration along view rays
- Accounts for exponential density decay with height
- Determines both extinction and in-scattering effects

---

## Summary

This implementation plan provides a complete atmospheric scattering system for the Labor3X DirectX 12 application. All changes are made within `Labor3X.cpp` and related files without creating additional headers, as requested. The plan includes:

1. Atmospheric parameter structures and constants
2. HLSL shader implementation
3. Skydome geometry generation
4. Integration with existing rendering pipeline
5. ImGui control panel for real-time adjustment
6. Proper DirectX 12 resource management

The implementation can be extended with more advanced techniques like ray marching for high-altitude views or volumetric fog as described in the lecture materials.
