# Atmospheric Scattering Implementation Plan for labor_3

## [[labor_3App.cpp]] Atmospheric Integration

### Overview
This plan adds a complete atmospheric scattering system to the DirectX 12 rendering pipeline in `labor_3App.cpp`. The implementation follows the Hoffman-Preetham approach for ground-level rendering with ray marching support for high-altitude views, adapted from OpenGL/GLM to DirectX 12/DirectX Math.

### Prerequisites
- DirectX 12 support
- DirectX Math library (XMFLOAT3, XMMATRIX, etc.) - already included
- ImGui for real-time parameter adjustment - already included
- Existing rendering pipeline with camera controls - already present in labor_3App

### Key Adaptations from L3inst.md
- **OpenGL → DirectX 12**: All rendering calls converted to D3D12 API
- **GLM → DirectX Math**: All vector/matrix operations use XMFLOAT3, XMMATRIX, etc.
- **GLSL → HLSL**: All shaders converted to HLSL format
- **Single File**: All code additions go into `labor_3App.cpp` (no new header files)

---

## Step 1: Add Atmospheric Constants and Parameters

**Location**: Add after existing includes (around line 20) but before the `RenderItem` struct

```cpp
// ============================================================================
// [[Atmospheric Scattering System]]
// ============================================================================

// Atmospheric scattering constants
const float ATMOSPHERE_PI = 3.14159265359f;
const float ATMOSPHERE_DEG_TO_RAD = ATMOSPHERE_PI / 180.0f;

// [[Atmospheric Parameters Structure]]
// Stores all parameters for atmospheric scattering calculations
struct AtmosphereParameters {
    // Earth-like atmosphere defaults (scaled down for typical game world)
    float planetRadius = 6371.0f;        // Earth radius in kilometers (scaled for rendering)
    float atmosphereRadius = 6471.0f;    // Atmosphere radius (100km above surface, scaled)
    
    // Rayleigh scattering coefficients (clear atmosphere)
    // RGB values in m^-1, wavelength-dependent (blue scatters more)
    XMFLOAT3 rayleighCoeff = XMFLOAT3(5.5e-6f, 13.5e-6f, 33.1e-6f);
    
    // Mie scattering coefficients (hazy atmosphere)
    // Gray value for haze (less wavelength-dependent)
    XMFLOAT3 mieCoeff = XMFLOAT3(2.0e-5f, 2.0e-5f, 2.0e-5f);
    
    // Optical properties
    float rayleighScaleHeight = 8.0f;    // Height where Rayleigh density falls to 1/e (km)
    float mieScaleHeight = 1.2f;         // Height where Mie density falls to 1/e (km)
    float mieAnisotropy = 0.76f;         // Forward scattering bias (0.76 for Earth)
    
    // Sun properties
    XMFLOAT3 sunDirection = XMFLOAT3(0.0f, -0.5f, -1.0f); // Default sun position
    float sunIntensity = 20.0f;          // Sun brightness multiplier
    
    // Runtime adjustment parameters
    float atmosphereDensity = 1.0f;      // Global density multiplier (1.0 = Earth normal)
    float pollutionLevel = 0.0f;        // 0.0 = clean, 1.0 = heavily polluted
    float humidityLevel = 0.3f;          // 0.0 = dry, 1.0 = very humid
    
    // Recalculate coefficients based on pollution/humidity
    void UpdateCoefficients() {
        // More pollution increases Mie scattering (haze)
        float pollutionFactor = 1.0f + pollutionLevel * 4.0f;
        // More humidity increases both scattering types but affects Mie more
        float humidityFactor = 1.0f + humidityLevel * 2.0f;
        
        // Base Earth coefficients adjusted by density and pollution
        XMVECTOR baseRayleigh = XMLoadFloat3(&XMFLOAT3(5.5e-6f, 13.5e-6f, 33.1e-6f));
        XMVECTOR densityVec = XMVectorReplicate(atmosphereDensity * humidityFactor);
        XMVECTOR rayleighVec = XMVectorMultiply(baseRayleigh, densityVec);
        XMStoreFloat3(&rayleighCoeff, rayleighVec);
        
        XMVECTOR baseMie = XMLoadFloat3(&XMFLOAT3(2.0e-5f, 2.0e-5f, 2.0e-5f));
        XMVECTOR mieFactor = XMVectorReplicate(atmosphereDensity * pollutionFactor * humidityFactor);
        XMVECTOR mieVec = XMVectorMultiply(baseMie, mieFactor);
        XMStoreFloat3(&mieCoeff, mieVec);
        
        // Adjust scale heights based on conditions
        rayleighScaleHeight = 8.0f * (1.0f - pollutionLevel * 0.3f);
        mieScaleHeight = 1.2f * (1.0f + pollutionLevel * 0.5f);
    }
};

// Global atmosphere instance
AtmosphereParameters gAtmosphere;
```

**Notes**:
- All values are scaled down from real-world Earth values to be suitable for typical game worlds
- Uses DirectX Math (XMFLOAT3, XMVECTOR) instead of GLM
- The `UpdateCoefficients()` method uses SIMD operations for efficiency

---

## Step 2: Add Atmospheric Shader Structures and Resources

**Location**: Add in the `Labor3App` class private section (around line 140, after existing member variables)

```cpp
    // ========================================================================
    // [[Atmospheric Scattering Resources]]
    // ========================================================================
    
    // Skydome geometry
    std::unique_ptr<MeshGeometry> mSkydomeGeo = nullptr;
    UINT mSkydomeIndexCount = 0;
    
    // Atmospheric shader programs
    ComPtr<ID3D12RootSignature> mAtmosphereRootSignature = nullptr;
    ComPtr<ID3D12PipelineState> mSkydomePSO = nullptr;
    
    // Atmospheric constant buffer
    struct AtmosphereConstants {
        XMFLOAT3 sunDirection;
        float planetRadius;
        XMFLOAT3 rayleighCoeff;
        float atmosphereRadius;
        XMFLOAT3 mieCoeff;
        float rayleighScaleHeight;
        float mieScaleHeight;
        float mieAnisotropy;
        float sunIntensity;
        XMFLOAT3 cameraPosition;
        float cameraHeight;
        XMFLOAT4X4 viewMatrix;
        XMFLOAT4X4 projectionMatrix;
    };
    std::unique_ptr<UploadBuffer<AtmosphereConstants>> mAtmosphereCB = nullptr;
    
    // Skydome render item
    std::unique_ptr<RenderItem> mSkydomeRitem = nullptr;
```

**Notes**:
- Reuses existing `MeshGeometry` and `RenderItem` structures
- Uses `UploadBuffer` template for constant buffer management (already in project)
- Atmospheric constants are packed into a single constant buffer for efficiency

---

## Step 3: Add Skydome Mesh Generation Function

**Location**: Add as a private method in `Labor3App` class (around line 110, with other Build methods)

```cpp
    // [[Skydome Generation]]
    // Creates a hemisphere mesh for rendering the sky
    void BuildSkydomeGeometry();
```

**Implementation**: Add after the class definition, before `WinMain` (around line 200)

```cpp
// ============================================================================
// [[Skydome Geometry Generation]]
// ============================================================================
void Labor3App::BuildSkydomeGeometry()
{
    /* 
    Creates a hemisphere mesh for rendering the sky.
    Uses icosahedron subdivision for even vertex distribution.
    The skydome represents the upper hemisphere of the atmosphere.
    */
    
    const int subdivisions = 3; // Quality level (3 = good balance)
    const float radius = gAtmosphere.atmosphereRadius + 1.0f; // Slightly larger than atmosphere
    
    // Icosahedron vertices (12 vertices)
    std::vector<XMFLOAT3> vertices = {
        XMFLOAT3(-0.525731f, 0.000000f, 0.850651f), XMFLOAT3(0.525731f, 0.000000f, 0.850651f),
        XMFLOAT3(-0.525731f, 0.000000f, -0.850651f), XMFLOAT3(0.525731f, 0.000000f, -0.850651f),
        XMFLOAT3(0.000000f, 0.850651f, 0.525731f), XMFLOAT3(0.000000f, 0.850651f, -0.525731f),
        XMFLOAT3(0.000000f, -0.850651f, 0.525731f), XMFLOAT3(0.000000f, -0.850651f, -0.525731f),
        XMFLOAT3(0.850651f, 0.525731f, 0.000000f), XMFLOAT3(-0.850651f, 0.525731f, 0.000000f),
        XMFLOAT3(0.850651f, -0.525731f, 0.000000f), XMFLOAT3(-0.850651f, -0.525731f, 0.000000f)
    };
    
    // Icosahedron triangles (20 faces)
    std::vector<UINT> triangles = {
        0, 4, 1,  0, 9, 4,  9, 5, 4,  4, 5, 8,  4, 8, 1,
        8, 10, 1, 8, 3, 10, 5, 3, 8,  5, 2, 3,  2, 7, 3,
        7, 10, 3, 7, 6, 10, 7, 11, 6, 11, 0, 6, 0, 1, 6,
        6, 1, 10, 9, 0, 11, 9, 11, 2, 9, 2, 5,  7, 2, 11
    };
    
    // Normalize vertices to unit sphere
    for (auto& v : vertices) {
        XMVECTOR vec = XMLoadFloat3(&v);
        vec = XMVector3Normalize(vec);
        XMStoreFloat3(&v, vec);
    }
    
    // Subdivide triangles
    for (int i = 0; i < subdivisions; i++) {
        std::vector<UINT> newTriangles;
        size_t originalCount = triangles.size();
        
        for (size_t j = 0; j < originalCount; j += 3) {
            UINT i0 = triangles[j];
            UINT i1 = triangles[j + 1];
            UINT i2 = triangles[j + 2];
            
            XMFLOAT3 v0 = vertices[i0];
            XMFLOAT3 v1 = vertices[i1];
            XMFLOAT3 v2 = vertices[i2];
            
            // Calculate midpoints
            XMVECTOR mid01 = XMVector3Normalize(XMVectorAdd(XMLoadFloat3(&v0), XMLoadFloat3(&v1)));
            XMVECTOR mid12 = XMVector3Normalize(XMVectorAdd(XMLoadFloat3(&v1), XMLoadFloat3(&v2)));
            XMVECTOR mid20 = XMVector3Normalize(XMVectorAdd(XMLoadFloat3(&v2), XMLoadFloat3(&v0)));
            
            XMFLOAT3 mid01f, mid12f, mid20f;
            XMStoreFloat3(&mid01f, mid01);
            XMStoreFloat3(&mid12f, mid12);
            XMStoreFloat3(&mid20f, mid20);
            
            // Add new vertices
            size_t idx01 = vertices.size(); vertices.push_back(mid01f);
            size_t idx12 = vertices.size(); vertices.push_back(mid12f);
            size_t idx20 = vertices.size(); vertices.push_back(mid20f);
            
            // Create 4 new triangles
            newTriangles.push_back(i0); newTriangles.push_back(idx01); newTriangles.push_back(idx20);
            newTriangles.push_back(i1); newTriangles.push_back(idx12); newTriangles.push_back(idx01);
            newTriangles.push_back(i2); newTriangles.push_back(idx20); newTriangles.push_back(idx12);
            newTriangles.push_back(idx01); newTriangles.push_back(idx12); newTriangles.push_back(idx20);
        }
        
        triangles = newTriangles;
    }
    
    // Convert to hemisphere (only keep vertices with y >= 0)
    std::vector<XMFLOAT3> hemisphereVertices;
    std::vector<UINT> hemisphereIndices;
    std::unordered_map<size_t, size_t> vertexMap;
    
    for (size_t j = 0; j < triangles.size(); j += 3) {
        UINT i0 = triangles[j];
        UINT i1 = triangles[j + 1];
        UINT i2 = triangles[j + 2];
        
        XMFLOAT3 v0 = vertices[i0];
        XMFLOAT3 v1 = vertices[i1];
        XMFLOAT3 v2 = vertices[i2];
        
        // Skip triangles completely below horizon
        if (v0.y < 0.0f && v1.y < 0.0f && v2.y < 0.0f) continue;
        
        // Get or create vertex indices
        auto getVertexIndex = [&](size_t srcIdx, const XMFLOAT3& pos) -> UINT {
            if (vertexMap.find(srcIdx) == vertexMap.end()) {
                XMFLOAT3 newPos = XMFLOAT3(pos.x, std::max(0.0f, pos.y), pos.z);
                hemisphereVertices.push_back(newPos);
                vertexMap[srcIdx] = hemisphereVertices.size() - 1;
            }
            return (UINT)vertexMap[srcIdx];
        };
        
        hemisphereIndices.push_back(getVertexIndex(i0, v0));
        hemisphereIndices.push_back(getVertexIndex(i1, v1));
        hemisphereIndices.push_back(getVertexIndex(i2, v2));
    }
    
    // Scale vertices to skydome radius
    for (auto& v : hemisphereVertices) {
        XMVECTOR vec = XMLoadFloat3(&v);
        vec = XMVectorScale(vec, radius);
        XMStoreFloat3(&v, vec);
    }
    
    // Convert to Vertex format (position only for skydome)
    const UINT vbByteSize = (UINT)hemisphereVertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)hemisphereIndices.size() * sizeof(std::uint16_t);
    
    std::vector<Vertex> skydomeVertices(hemisphereVertices.size());
    for (size_t i = 0; i < hemisphereVertices.size(); i++) {
        skydomeVertices[i].Pos = hemisphereVertices[i];
        skydomeVertices[i].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f); // Not used for skydome
        skydomeVertices[i].TexC = XMFLOAT2(0.0f, 0.0f); // Not used for skydome
    }
    
    std::vector<std::uint16_t> skydomeIndices(hemisphereIndices.size());
    for (size_t i = 0; i < hemisphereIndices.size(); i++) {
        skydomeIndices[i] = (std::uint16_t)hemisphereIndices[i];
    }
    
    // Create mesh geometry
    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "skydomeGeo";
    
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), skydomeVertices.data(), vbByteSize);
    
    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), skydomeIndices.data(), ibByteSize);
    
    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), skydomeVertices.data(), vbByteSize, geo->VertexBufferUploader);
    
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), skydomeIndices.data(), ibByteSize, geo->IndexBufferUploader);
    
    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;
    
    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)skydomeIndices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    
    geo->DrawArgs["skydome"] = submesh;
    
    mGeometries[geo->Name] = std::move(geo);
    mSkydomeIndexCount = (UINT)skydomeIndices.size();
}
```

**Notes**:
- Uses DirectX Math for all vector operations
- Converts to existing `Vertex` format for compatibility
- Creates `MeshGeometry` using existing patterns from the codebase

---

## Step 4: Create HLSL Shader Files

**Location**: Create new files in `labor_3/src/Shaders/`

### File: `Shaders/Skydome.hlsl`

```hlsl
// ============================================================================
// Skydome Atmospheric Scattering Shader
// ============================================================================

cbuffer AtmosphereConstants : register(b0)
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
    float3 cameraPosition;
    float cameraHeight;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 WorldPos : POSITION0;
    float3 SunDirection : POSITION1;
    float CameraHeight : TEXCOORD0;
};

// [[Rayleigh Phase Function]]
// Describes angular distribution of Rayleigh scattered light
// Isotropic scattering (equal in all directions)
float RayleighPhaseFunction(float cosTheta)
{
    return (3.0 / (16.0 * 3.14159265359)) * (1.0 + cosTheta * cosTheta);
}

// [[Mie Phase Function]]
// Describes angular distribution of Mie scattered light
// Forward-scattering dominant (g > 0)
float MiePhaseFunction(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 / (4.0 * 3.14159265359)) * ((1.0 - g2) * (1.0 + cosTheta * cosTheta)) / 
           (pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5) * (2.0 + g2));
}

// [[Optical Depth Function]]
// Calculates the optical depth along a ray through the atmosphere
float OpticalDepth(float3 pos, float3 dir, float scaleHeight)
{
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, pos);
    float c = dot(pos, pos) - atmosphereRadius * atmosphereRadius;
    float det = b * b - 4.0 * a * c;
    
    if (det < 0.0) return 0.0;
    
    float t = (-b - sqrt(det)) / (2.0 * a);
    if (t < 0.0) t = (-b + sqrt(det)) / (2.0 * a);
    
    return exp(-(length(pos + dir * t) - planetRadius) / scaleHeight) * t;
}

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // Transform to world space (skydome is already in world space)
    vout.WorldPos = vin.PosL;
    vout.SunDirection = sunDirection;
    vout.CameraHeight = cameraPosition.y;
    
    // Transform to clip space
    float4 posW = float4(vin.PosL, 1.0f);
    float4 posV = mul(posW, viewMatrix);
    vout.PosH = mul(posV, projectionMatrix);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Camera position in atmosphere coordinates
    float3 cameraPos = float3(0.0, pin.CameraHeight, 0.0);
    
    // View direction from camera to fragment
    float3 viewDir = normalize(pin.WorldPos - cameraPos);
    
    // Calculate optical depths
    float rayleighDepth = OpticalDepth(cameraPos, viewDir, rayleighScaleHeight);
    float mieDepth = OpticalDepth(cameraPos, viewDir, mieScaleHeight);
    
    // Calculate extinction
    float3 extinction = exp(-(rayleighCoeff * rayleighDepth + mieCoeff * mieDepth));
    
    // Calculate in-scattering
    float3 sunDir = normalize(sunDirection);
    float cosTheta = dot(viewDir, sunDir);
    
    float rayleighPhase = RayleighPhaseFunction(cosTheta);
    float miePhase = MiePhaseFunction(cosTheta, mieAnisotropy);
    
    // Optical depth to sun
    float sunRayleighDepth = OpticalDepth(pin.WorldPos, sunDir, rayleighScaleHeight);
    float sunMieDepth = OpticalDepth(pin.WorldPos, sunDir, mieScaleHeight);
    
    float3 sunExtinction = exp(-(rayleighCoeff * sunRayleighDepth + mieCoeff * sunMieDepth));
    
    // Calculate final colors
    float3 rayleighColor = rayleighPhase * rayleighCoeff * sunExtinction * sunIntensity;
    float3 mieColor = miePhase * mieCoeff * sunExtinction * sunIntensity;
    
    float3 finalColor = (rayleighColor + mieColor) * (1.0 - extinction);
    
    // Add some ambient light
    finalColor += float3(0.05, 0.07, 0.1) * extinction;
    
    // Gamma correction
    finalColor = pow(finalColor, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));
    
    return float4(saturate(finalColor), 1.0);
}
```

**Notes**:
- Uses HLSL syntax instead of GLSL
- Constant buffer uses `register(b0)` for binding
- Vertex and pixel shaders follow DirectX 12 conventions

---

## Step 5: Add Shader Compilation and Root Signature

**Location**: Add methods to `Labor3App` class (around line 110)

```cpp
    // [[Atmospheric Shader Setup]]
    void BuildAtmosphereRootSignature();
    void BuildAtmosphereShaders();
    void BuildSkydomePSO();
```

**Implementation**: Add after other Build methods (around line 1400)

```cpp
// ============================================================================
// [[Atmospheric Root Signature]]
// ============================================================================
void Labor3App::BuildAtmosphereRootSignature()
{
    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];
    
    // Create a single descriptor table of CBVs
    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable, D3D12_SHADER_VISIBILITY_ALL);
    
    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    
    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    
    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);
    
    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mAtmosphereRootSignature)));
}

// ============================================================================
// [[Atmospheric Shader Compilation]]
// ============================================================================
void Labor3App::BuildAtmosphereShaders()
{
    const D3D_SHADER_MACRO defines[] =
    {
        NULL, NULL
    };
    
    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "ALPHA_TEST", "1",
        NULL, NULL
    };
    
    mShaders["skydomeVS"] = d3dUtil::CompileShader(L"Shaders\\Skydome.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["skydomePS"] = d3dUtil::CompileShader(L"Shaders\\Skydome.hlsl", nullptr, "PS", "ps_5_1");
}

// ============================================================================
// [[Skydome Pipeline State Object]]
// ============================================================================
void Labor3App::BuildSkydomePSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skydomePsoDesc;
    
    ZeroMemory(&skydomePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    skydomePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    skydomePsoDesc.pRootSignature = mAtmosphereRootSignature.Get();
    skydomePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skydomeVS"]->GetBufferPointer()),
        mShaders["skydomeVS"]->GetBufferSize()
    };
    skydomePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["skydomePS"]->GetBufferPointer()),
        mShaders["skydomePS"]->GetBufferSize()
    };
    skydomePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    skydomePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // Render both sides
    skydomePsoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    skydomePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    skydomePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    skydomePsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write depth
    skydomePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // Always pass
    skydomePsoDesc.SampleMask = UINT_MAX;
    skydomePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    skydomePsoDesc.NumRenderTargets = 1;
    skydomePsoDesc.RTVFormats[0] = mBackBufferFormat;
    skydomePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    skydomePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    skydomePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skydomePsoDesc, IID_PPV_ARGS(&mSkydomePSO)));
}
```

**Notes**:
- Uses existing `d3dUtil::CompileShader` helper
- Skydome doesn't write to depth buffer (always rendered first)
- Culling disabled to render both sides of hemisphere

---

## Step 6: Initialize Atmospheric System

**Location**: Add calls in `Labor3App::Initialize()` method (around line 250, after other initialization)

```cpp
    // Initialize atmosphere parameters
    gAtmosphere.UpdateCoefficients();
    
    // Build skydome geometry
    BuildSkydomeGeometry();
    
    // Build atmospheric shaders and PSO
    BuildAtmosphereRootSignature();
    BuildAtmosphereShaders();
    BuildSkydomePSO();
    
    // Create atmospheric constant buffer
    mAtmosphereCB = std::make_unique<UploadBuffer<AtmosphereConstants>>(md3dDevice.Get(), 1, true);
    
    // Create skydome render item
    auto skydomeRitem = std::make_unique<RenderItem>();
    skydomeRitem->World = MathHelper::Identity4x4();
    skydomeRitem->ObjCBIndex = 0; // Will be updated per frame
    skydomeRitem->Geo = mGeometries["skydomeGeo"].get();
    skydomeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skydomeRitem->IndexCount = skydomeRitem->Geo->DrawArgs["skydome"].IndexCount;
    skydomeRitem->StartIndexLocation = skydomeRitem->Geo->DrawArgs["skydome"].StartIndexLocation;
    skydomeRitem->BaseVertexLocation = skydomeRitem->Geo->DrawArgs["skydome"].BaseVertexLocation;
    skydomeRitem->Name = "skydome";
    mSkydomeRitem = skydomeRitem.get();
    mAllRitems.push_back(std::move(skydomeRitem));
```

**Notes**:
- Initializes all atmospheric resources during app startup
- Creates render item for skydome using existing patterns

---

## Step 7: Update Atmospheric Constants Per Frame

**Location**: Add method to `Labor3App` class (around line 90)

```cpp
    void UpdateAtmosphereCB(const GameTimer& gt);
```

**Implementation**: Add after `UpdateMainPassCB` (around line 720)

```cpp
// ============================================================================
// [[Update Atmospheric Constants]]
// ============================================================================
void Labor3App::UpdateAtmosphereCB(const GameTimer& gt)
{
    // Update atmosphere coefficients if parameters changed
    gAtmosphere.UpdateCoefficients();
    
    AtmosphereConstants atmConstants;
    
    // Copy sun direction
    atmConstants.sunDirection = gAtmosphere.sunDirection;
    atmConstants.planetRadius = gAtmosphere.planetRadius;
    atmConstants.atmosphereRadius = gAtmosphere.atmosphereRadius;
    atmConstants.rayleighCoeff = gAtmosphere.rayleighCoeff;
    atmConstants.mieCoeff = gAtmosphere.mieCoeff;
    atmConstants.rayleighScaleHeight = gAtmosphere.rayleighScaleHeight;
    atmConstants.mieScaleHeight = gAtmosphere.mieScaleHeight;
    atmConstants.mieAnisotropy = gAtmosphere.mieAnisotropy;
    atmConstants.sunIntensity = gAtmosphere.sunIntensity;
    
    // Get camera position and height
    XMFLOAT3 eyePos = mEyePos;
    atmConstants.cameraPosition = eyePos;
    atmConstants.cameraHeight = eyePos.y;
    
    // Copy view and projection matrices
    atmConstants.viewMatrix = mView;
    atmConstants.projectionMatrix = mProj;
    
    // Upload to GPU
    mAtmosphereCB->CopyData(0, atmConstants);
}
```

**Location**: Call in `Update()` method (around line 320, after `UpdateMainPassCB`)

```cpp
    UpdateAtmosphereCB(gt);
```

---

## Step 8: Render Skydome

**Location**: Add method to `Labor3App` class (around line 108)

```cpp
    void DrawSkydome(ID3D12GraphicsCommandList* cmdList);
```

**Implementation**: Add in `Draw()` or `DeferredDraw()` method (around line 1650, before rendering other objects)

```cpp
// ============================================================================
// [[Render Skydome]]
// ============================================================================
void Labor3App::DrawSkydome(ID3D12GraphicsCommandList* cmdList)
{
    // Set skydome PSO
    cmdList->SetPipelineState(mSkydomePSO.Get());
    cmdList->SetGraphicsRootSignature(mAtmosphereRootSignature.Get());
    
    // Bind constant buffer
    auto atmCB = mAtmosphereCB->Resource();
    cmdList->SetGraphicsRootConstantBufferView(0, atmCB->GetGPUVirtualAddress());
    
    // Draw skydome
    auto skydomeGeo = mGeometries["skydomeGeo"].get();
    cmdList->IASetVertexBuffers(0, 1, &skydomeGeo->VertexBufferView());
    cmdList->IASetIndexBuffer(&skydomeGeo->IndexBufferView());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    cmdList->DrawIndexedInstanced(mSkydomeIndexCount, 1, 0, 0, 0);
}
```

**Location**: Call in `Draw()` method (around line 1650, at the beginning of rendering, before other objects)

```cpp
    // Render skydome first (background)
    DrawSkydome(mCommandList.Get());
```

**Notes**:
- Skydome is rendered first as background
- Uses its own PSO and root signature
- Doesn't interfere with existing rendering pipeline

---

## Step 9: Add ImGui Control Panel

**Location**: Add method to `Labor3App` class (around line 110)

```cpp
    void DrawAtmosphereUI();
```

**Implementation**: Add after other UI code (around line 1800, in `Draw()` method after ImGui::NewFrame())

```cpp
// ============================================================================
// [[Atmospheric Control Panel]]
// ============================================================================
void Labor3App::DrawAtmosphereUI()
{
    static bool showAtmospherePanel = true;
    
    if (!showAtmospherePanel) return;
    
    ImGui::Begin("Atmosphere Controls", &showAtmospherePanel, ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::Text("Atmospheric Conditions");
    ImGui::Separator();
    
    // Sun controls
    ImGui::Text("Sun Direction");
    static float sunElevation = -30.0f; // Degrees from horizon
    static float sunAzimuth = 180.0f;   // Degrees from north
    
    bool sunChanged = false;
    if (ImGui::SliderFloat("Elevation", &sunElevation, -90.0f, 90.0f, "%.1f°")) {
        sunChanged = true;
    }
    
    if (ImGui::SliderFloat("Azimuth", &sunAzimuth, 0.0f, 360.0f, "%.1f°")) {
        sunChanged = true;
    }
    
    if (sunChanged) {
        // Convert spherical coordinates to Cartesian
        float elevationRad = sunElevation * ATMOSPHERE_DEG_TO_RAD;
        float azimuthRad = sunAzimuth * ATMOSPHERE_DEG_TO_RAD;
        XMVECTOR sunDir = XMVectorSet(
            cosf(elevationRad) * cosf(azimuthRad),
            sinf(elevationRad),
            cosf(elevationRad) * sinf(azimuthRad),
            0.0f
        );
        sunDir = XMVector3Normalize(sunDir);
        XMStoreFloat3(&gAtmosphere.sunDirection, sunDir);
    }
    
    ImGui::SliderFloat("Sun Intensity", &gAtmosphere.sunIntensity, 0.1f, 100.0f, "%.1f");
    
    ImGui::Separator();
    
    // Atmospheric quality controls
    ImGui::Text("Atmospheric Quality");
    
    bool paramsChanged = false;
    if (ImGui::SliderFloat("Density", &gAtmosphere.atmosphereDensity, 0.1f, 5.0f, "%.2f")) {
        paramsChanged = true;
    }
    
    if (ImGui::SliderFloat("Pollution", &gAtmosphere.pollutionLevel, 0.0f, 1.0f, "%.2f")) {
        paramsChanged = true;
    }
    
    if (ImGui::SliderFloat("Humidity", &gAtmosphere.humidityLevel, 0.0f, 1.0f, "%.2f")) {
        paramsChanged = true;
    }
    
    if (paramsChanged) {
        gAtmosphere.UpdateCoefficients();
    }
    
    ImGui::Separator();
    
    // Preset buttons
    ImGui::Text("Presets");
    if (ImGui::Button("Clear Sky")) {
        gAtmosphere.atmosphereDensity = 1.0f;
        gAtmosphere.pollutionLevel = 0.0f;
        gAtmosphere.humidityLevel = 0.2f;
        gAtmosphere.sunIntensity = 20.0f;
        gAtmosphere.UpdateCoefficients();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Hazy Day")) {
        gAtmosphere.atmosphereDensity = 1.2f;
        gAtmosphere.pollutionLevel = 0.4f;
        gAtmosphere.humidityLevel = 0.5f;
        gAtmosphere.sunIntensity = 15.0f;
        gAtmosphere.UpdateCoefficients();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Heavy Pollution")) {
        gAtmosphere.atmosphereDensity = 1.5f;
        gAtmosphere.pollutionLevel = 0.8f;
        gAtmosphere.humidityLevel = 0.6f;
        gAtmosphere.sunIntensity = 10.0f;
        gAtmosphere.UpdateCoefficients();
    }
    
    ImGui::Separator();
    
    // Technical parameters (advanced)
    if (ImGui::TreeNode("Advanced Parameters")) {
        ImGui::SliderFloat("Rayleigh Scale Height", &gAtmosphere.rayleighScaleHeight, 1.0f, 20.0f, "%.1f km");
        ImGui::SliderFloat("Mie Scale Height", &gAtmosphere.mieScaleHeight, 0.5f, 5.0f, "%.1f km");
        ImGui::SliderFloat("Mie Anisotropy", &gAtmosphere.mieAnisotropy, 0.0f, 0.99f, "%.3f");
        
        ImGui::Text("Rayleigh Coefficients (RGB):");
        ImGui::SliderFloat3("##rayleigh", (float*)&gAtmosphere.rayleighCoeff, 1e-6f, 1e-4f, "%.3e");
        
        ImGui::Text("Mie Coefficients (RGB):");
        ImGui::SliderFloat3("##mie", (float*)&gAtmosphere.mieCoeff, 1e-6f, 1e-4f, "%.3e");
        
        ImGui::TreePop();
    }
    
    ImGui::End();
}
```

**Location**: Call in `Draw()` method (around line 1800, after `ImGui::NewFrame()`)

```cpp
    DrawAtmosphereUI();
```

---

## Step 10: Integration Summary

### Files Modified
1. **labor_3/src/labor_3App.cpp** - All atmospheric code added here
2. **labor_3/src/Shaders/Skydome.hlsl** - New shader file

### Files Created
- `Shaders/Skydome.hlsl` - Atmospheric scattering shader

### Key Integration Points

1. **Initialization** (`Initialize()` method):
   - Call `gAtmosphere.UpdateCoefficients()`
   - Call `BuildSkydomeGeometry()`
   - Call `BuildAtmosphereRootSignature()`
   - Call `BuildAtmosphereShaders()`
   - Call `BuildSkydomePSO()`
   - Create `mAtmosphereCB` constant buffer
   - Create skydome render item

2. **Update Loop** (`Update()` method):
   - Call `UpdateAtmosphereCB(gt)` after `UpdateMainPassCB(gt)`

3. **Render Loop** (`Draw()` method):
   - Call `DrawSkydome(mCommandList.Get())` at the beginning, before other objects
   - Call `DrawAtmosphereUI()` in ImGui section

### Testing Checklist
- [ ] Skydome renders correctly as background
- [ ] Sun direction changes affect sky color
- [ ] Pollution/humidity sliders work
- [ ] Preset buttons apply correct values
- [ ] No performance degradation
- [ ] No visual artifacts or flickering

---

## [[Refactoring Guide]] Future Improvements

### File Structure Plan
Once the single-file implementation is working, refactor into separate files:

```
labor_3/src/
├── AtmosphereSystem.h/cpp          // Main atmosphere system class
├── SkydomeRenderer.h/cpp           // Skydome mesh and rendering
├── AtmosphericScattering.h/cpp    // Scattering calculations
└── AtmosphereUI.h/cpp              // ImGui control panel
```

### Key Refactoring Steps
1. **Extract Atmosphere System**: Move `AtmosphereParameters` and related code to `AtmosphereSystem.h/cpp`
2. **Separate Rendering Logic**: Create `SkydomeRenderer` class for skydome-specific code
3. **Externalize Shaders**: Shaders already in separate files (good!)
4. **Create UI Component**: Extract ImGui controls to `AtmosphereUI.h/cpp`
5. **Add Configuration**: Create JSON/XML configuration for preset atmospheres

### Performance Optimizations
- Implement texture lookups for precomputed scattering integrals
- Add level-of-detail for skydome based on distance
- Implement temporal anti-aliasing for atmospheric effects
- Add compute shader support for high-altitude rendering

### Advanced Features to Add
- Dynamic weather transitions
- Volumetric clouds integration
- Multiple scattering approximation
- Night sky with stars and moon
- Atmospheric shadow casting

---

## [[Implementation Notes]] Key Concepts

### [[Rayleigh vs Mie Scattering]]
- **Rayleigh scattering**: Dominant for clear skies, wavelength-dependent (blue scatters more)
- **Mie scattering**: Dominant for hazy conditions, less wavelength-dependent, forward-scattering dominant
- The ratio of these coefficients determines sky color and haze appearance

### [[Hoffman-Preetham Approach]]
The implementation follows the Hoffman-Preetham method:
- Precomputes expensive integrals at vertices
- Uses linear interpolation in pixel shader
- Assumes constant atmospheric density (good for ground-level views)
- Efficient for real-time rendering

### [[DirectX 12 Adaptations]]
- **Constant Buffers**: Uses `UploadBuffer<AtmosphereConstants>` for GPU data
- **Root Signatures**: Separate root signature for atmospheric rendering
- **Pipeline State**: Dedicated PSO for skydome rendering
- **HLSL Shaders**: Converted from GLSL with proper DirectX 12 bindings

### [[Real-time Parameter Adjustment]]
The ImGui panel allows real-time adjustment of:
- **Pollution level**: Increases Mie scattering, creates haze
- **Humidity**: Affects both scattering types, creates more diffuse lighting
- **Atmospheric density**: Global multiplier for all scattering effects
- **Sun position**: Changes lighting direction and sky colors

### [[Optical Depth Calculation]]
The optical depth calculation is crucial for realistic atmospheric effects:
- Uses analytical integration along view rays
- Accounts for exponential density decay with height
- Determines both extinction and in-scattering effects

---

## Troubleshooting

### Common Issues

1. **Skydome not visible**: Check that `DrawSkydome()` is called before other rendering, and depth test is configured correctly
2. **Shader compilation errors**: Verify HLSL file path and syntax
3. **Constant buffer not updating**: Ensure `UpdateAtmosphereCB()` is called in `Update()` method
4. **Performance issues**: Reduce skydome subdivision level or optimize shader

### Debug Tips
- Use RenderDoc or PIX to inspect shader outputs
- Add debug colors to shader to verify calculations
- Check constant buffer values in GPU debugger
- Verify skydome geometry is correct (check vertex/index counts)

---

This implementation provides a solid foundation that can be extended with more advanced techniques like ray marching for high-altitude views or volumetric fog as described in the lecture materials.


