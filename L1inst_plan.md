# Terrain Renderer Implementation Plan

> **Status**: Planning Phase  
> **Target File**: `baseline/src/Baseline.cpp`  
> **Approach**: Single-file implementation (all code in Baseline.cpp)  
> **Refactoring Guide**: See [[L1inst_refactor_guide.md]]

## Overview

This plan transforms the baseline cube renderer into a terrain renderer with:
- **Quad-tree LOD system** for terrain rendering
- **Frustum culling** for performance
- **Height map generation** using Perlin noise
- **Water surface** with animated waves

All implementations will be added directly to `Baseline.cpp` without creating additional header files.

---

## Table of Contents

- [[#Phase 1: Core Data Structures]]
- [[#Phase 2: Frustum Culling System]]
- [[#Phase 3: Height Map Generator]]
- [[#Phase 4: Terrain Tile System]]
- [[#Phase 5: Quad-Tree LOD System]]
- [[#Phase 6: Water Renderer]]
- [[#Phase 7: Integration into BaselineApp]]
- [[#Phase 8: Shader Creation]]
- [[#Phase 9: Testing and Optimization]]

---

## Phase 1: Core Data Structures

### Location: Top of Baseline.cpp (after includes, before class definition)

### Step 1.1: Add Required Includes

**Location**: After line 11 (after `#include "BaselineFrameResource.h"`)

```cpp
// ============================================================================
// TERRAIN RENDERER ADDITIONS - Includes
// ============================================================================
#include <DirectXCollision.h>  // For BoundingBox
#include <random>              // For height map generation
#include <cmath>               // For math functions
```

**Note**: `DirectXCollision.h` may already be included via `d3dUtil.h`, but we include it explicitly for clarity.

---

### Step 1.2: Define Terrain Vertex Structure

**Location**: After `RenderItem` struct definition (around line 36)

```cpp
// ============================================================================
// TERRAIN RENDERER - Vertex Structures
// ============================================================================

// Vertex structure for terrain tiles
struct TerrainVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexCoord;
    
    TerrainVertex() {}
    TerrainVertex(const DirectX::XMFLOAT3& p, const DirectX::XMFLOAT3& n, const DirectX::XMFLOAT2& t)
        : Position(p), Normal(n), TexCoord(t) {}
};

// Vertex structure for water surface
struct WaterVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT2 TexCoord;
    
    WaterVertex() {}
    WaterVertex(const DirectX::XMFLOAT3& p, const DirectX::XMFLOAT2& t)
        : Position(p), TexCoord(t) {}
};
```

---

### Step 1.3: Extend PassConstants Structure

**Location**: In `BaselineFrameResource.h` (we'll note this but keep changes minimal)

**Note**: We need to extend `PassConstants` to include view/proj matrices and eye position. However, since we're keeping everything in Baseline.cpp, we'll handle this by:

1. **Option A**: Modify `BaselineFrameResource.h` minimally (recommended)
2. **Option B**: Create a separate terrain pass CB in Baseline.cpp

**For Option A** (modify BaselineFrameResource.h):

```cpp
// In BaselineFrameResource.h, modify PassConstants:
struct PassConstants
{
    DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;
    DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
};
```

**For Option B** (keep in Baseline.cpp only):

We'll create a separate `TerrainPassConstants` structure in Baseline.cpp and manage it separately.

**Decision**: Use Option A for consistency, but document Option B as alternative.

---

## Phase 2: Frustum Culling System

### Location: Inside BaselineApp class (private section)

### Step 2.1: Define Frustum Class (Nested in BaselineApp)

**Location**: In `BaselineApp` class, private section (around line 68, before existing members)

```cpp
private:
    // ============================================================================
    // TERRAIN RENDERER - Frustum Culling System
    // ============================================================================
    
    // Nested Frustum class for culling
    class Frustum
    {
    public:
        Frustum() = default;
        ~Frustum() = default;

        void Update(const DirectX::XMMATRIX& viewProj);
        
        // Check intersection with sphere
        bool Intersects(const DirectX::XMFLOAT3& center, float radius) const;
        
        // Check intersection with AABB
        bool Intersects(const DirectX::BoundingBox& boundingBox) const;
        
        // Check if point is inside frustum
        bool ContainsPoint(const DirectX::XMFLOAT3& point) const;

    private:
        struct Plane
        {
            DirectX::XMVECTOR normal;
            float distance;
        };
        
        Plane mPlanes[6]; // Near, Far, Left, Right, Top, Bottom
    };
    
    // Frustum instance
    Frustum mFrustum;
```

---

### Step 2.2: Implement Frustum Methods

**Location**: After `BaselineApp` class definition, before `WinMain` (around line 92)

```cpp
// ============================================================================
// TERRAIN RENDERER - Frustum Implementation
// ============================================================================

void BaselineApp::Frustum::Update(const DirectX::XMMATRIX& viewProj)
{
    DirectX::XMFLOAT4X4 viewProjMat;
    DirectX::XMStoreFloat4x4(&viewProjMat, viewProj);
    
    // Extract planes from view-projection matrix
    // Left plane: column 4 + column 1
    mPlanes[0].normal = DirectX::XMVectorSet(
        viewProjMat._14 + viewProjMat._11,
        viewProjMat._24 + viewProjMat._21,
        viewProjMat._34 + viewProjMat._31,
        0.0f);
    mPlanes[0].distance = viewProjMat._44 + viewProjMat._41;
    
    // Right plane: column 4 - column 1
    mPlanes[1].normal = DirectX::XMVectorSet(
        viewProjMat._14 - viewProjMat._11,
        viewProjMat._24 - viewProjMat._21,
        viewProjMat._34 - viewProjMat._31,
        0.0f);
    mPlanes[1].distance = viewProjMat._44 - viewProjMat._41;
    
    // Bottom plane: column 4 + column 2
    mPlanes[2].normal = DirectX::XMVectorSet(
        viewProjMat._14 + viewProjMat._12,
        viewProjMat._24 + viewProjMat._22,
        viewProjMat._34 + viewProjMat._32,
        0.0f);
    mPlanes[2].distance = viewProjMat._44 + viewProjMat._42;
    
    // Top plane: column 4 - column 2
    mPlanes[3].normal = DirectX::XMVectorSet(
        viewProjMat._14 - viewProjMat._12,
        viewProjMat._24 - viewProjMat._22,
        viewProjMat._34 - viewProjMat._32,
        0.0f);
    mPlanes[3].distance = viewProjMat._44 - viewProjMat._42;
    
    // Near plane: column 3
    mPlanes[4].normal = DirectX::XMVectorSet(
        viewProjMat._13,
        viewProjMat._23,
        viewProjMat._33,
        0.0f);
    mPlanes[4].distance = viewProjMat._43;
    
    // Far plane: column 4 - column 3
    mPlanes[5].normal = DirectX::XMVectorSet(
        viewProjMat._14 - viewProjMat._13,
        viewProjMat._24 - viewProjMat._23,
        viewProjMat._34 - viewProjMat._33,
        0.0f);
    mPlanes[5].distance = viewProjMat._44 - viewProjMat._43;
    
    // Normalize all planes
    for (int i = 0; i < 6; ++i)
    {
        float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(mPlanes[i].normal));
        if (length > 0.0f)
        {
            mPlanes[i].normal = DirectX::XMVectorScale(mPlanes[i].normal, 1.0f / length);
            mPlanes[i].distance /= length;
        }
    }
}

bool BaselineApp::Frustum::Intersects(const DirectX::XMFLOAT3& center, float radius) const
{
    DirectX::XMVECTOR centerVec = DirectX::XMLoadFloat3(&center);
    
    for (int i = 0; i < 6; ++i)
    {
        float distance = DirectX::XMVectorGetX(
            DirectX::XMVector3Dot(mPlanes[i].normal, centerVec)) + mPlanes[i].distance;
        
        // If sphere is completely outside any plane, it's outside frustum
        if (distance < -radius)
            return false;
    }
    
    return true;
}

bool BaselineApp::Frustum::Intersects(const DirectX::BoundingBox& boundingBox) const
{
    for (int i = 0; i < 6; ++i)
    {
        // Check if all 8 corners of AABB are on negative side of plane
        bool allOutside = true;
        
        for (int x = 0; x < 2; ++x)
        {
            for (int y = 0; y < 2; ++y)
            {
                for (int z = 0; z < 2; ++z)
                {
                    DirectX::XMFLOAT3 point;
                    point.x = (x == 0) ? 
                        (boundingBox.Center.x - boundingBox.Extents.x) : 
                        (boundingBox.Center.x + boundingBox.Extents.x);
                    point.y = (y == 0) ? 
                        (boundingBox.Center.y - boundingBox.Extents.y) : 
                        (boundingBox.Center.y + boundingBox.Extents.y);
                    point.z = (z == 0) ? 
                        (boundingBox.Center.z - boundingBox.Extents.z) : 
                        (boundingBox.Center.z + boundingBox.Extents.z);
                    
                    DirectX::XMVECTOR pointVec = DirectX::XMLoadFloat3(&point);
                    float distance = DirectX::XMVectorGetX(
                        DirectX::XMVector3Dot(mPlanes[i].normal, pointVec)) + mPlanes[i].distance;
                    
                    if (distance >= 0.0f)
                    {
                        allOutside = false;
                        goto next_plane;
                    }
                }
            }
        }
        
        next_plane:
        if (allOutside)
            return false;
    }
    
    return true;
}

bool BaselineApp::Frustum::ContainsPoint(const DirectX::XMFLOAT3& point) const
{
    DirectX::XMVECTOR pointVec = DirectX::XMLoadFloat3(&point);
    
    for (int i = 0; i < 6; ++i)
    {
        float distance = DirectX::XMVectorGetX(
            DirectX::XMVector3Dot(mPlanes[i].normal, pointVec)) + mPlanes[i].distance;
        
        if (distance < 0.0f)
            return false;
    }
    
    return true;
}
```

---

## Phase 3: Height Map Generator

### Step 3.1: Define HeightMapGenerator Class (Nested in BaselineApp)

**Location**: In `BaselineApp` class, private section (after Frustum class)

```cpp
    // ============================================================================
    // TERRAIN RENDERER - Height Map Generator
    // ============================================================================
    
    class HeightMapGenerator
    {
    public:
        HeightMapGenerator(int width, int height, float scale = 1.0f);
        ~HeightMapGenerator() = default;
        
        void GenerateHeightMap(std::vector<float>& heights, const DirectX::XMFLOAT2& offset = {0.0f, 0.0f});
        
        // Configuration
        void SetOctaves(int octaves) { mOctaves = octaves; }
        void SetFrequency(float frequency) { mFrequency = frequency; }
        void SetAmplitude(float amplitude) { mAmplitude = amplitude; }
        void SetSeed(unsigned int seed) { mSeed = seed; }
        
        int GetWidth() const { return mWidth; }
        int GetHeight() const { return mHeight; }
        float GetScale() const { return mScale; }

    private:
        float PerlinNoise2D(float x, float y);
        float Interpolate(float a, float b, float x);
        float Fade(float t);
        
        int mWidth;
        int mHeight;
        float mScale;
        int mOctaves;
        float mFrequency;
        float mAmplitude;
        unsigned int mSeed;
    };
```

---

### Step 3.2: Implement HeightMapGenerator Methods

**Location**: After Frustum implementation

```cpp
// ============================================================================
// TERRAIN RENDERER - Height Map Generator Implementation
// ============================================================================

BaselineApp::HeightMapGenerator::HeightMapGenerator(int width, int height, float scale)
    : mWidth(width), mHeight(height), mScale(scale),
      mOctaves(4), mFrequency(0.01f), mAmplitude(10.0f), mSeed(12345)
{
}

void BaselineApp::HeightMapGenerator::GenerateHeightMap(
    std::vector<float>& heights, 
    const DirectX::XMFLOAT2& offset)
{
    heights.resize(mWidth * mHeight);
    
    for (int y = 0; y < mHeight; ++y)
    {
        for (int x = 0; x < mWidth; ++x)
        {
            float worldX = (x + offset.x) * mScale;
            float worldY = (y + offset.y) * mScale;
            
            float height = 0.0f;
            float amplitude = mAmplitude;
            float frequency = mFrequency;
            
            // Sum octaves for fractal noise
            for (int o = 0; o < mOctaves; ++o)
            {
                height += PerlinNoise2D(worldX * frequency, worldY * frequency) * amplitude;
                amplitude *= 0.5f;
                frequency *= 2.0f;
            }
            
            // Normalize height (ensure non-negative)
            height = std::max(0.0f, height);
            heights[y * mWidth + x] = height;
        }
    }
}

float BaselineApp::HeightMapGenerator::PerlinNoise2D(float x, float y)
{
    // Simple hash function for pseudo-random values
    auto hash = [](float x, float y, unsigned int seed) -> float {
        int ix = static_cast<int>(std::floor(x));
        int iy = static_cast<int>(std::floor(y));
        
        // Combine coordinates with seed
        size_t h = std::hash<int>()(ix * 12345 + iy * 67890 + static_cast<int>(seed));
        
        // Normalize to [0, 1]
        return static_cast<float>(h % 10000) / 10000.0f;
    };
    
    // Integer coordinates
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    
    // Fractional parts
    float sx = x - x0;
    float sy = y - y0;
    
    // Gradient values for 4 corners
    float n0 = hash(x0, y0, mSeed);
    float n1 = hash(x1, y0, mSeed);
    float n2 = hash(x0, y1, mSeed);
    float n3 = hash(x1, y1, mSeed);
    
    // Interpolate
    float i1 = Interpolate(n0, n1, sx);
    float i2 = Interpolate(n2, n3, sx);
    float result = Interpolate(i1, i2, sy);
    
    // Normalize to [-1, 1]
    return result * 2.0f - 1.0f;
}

float BaselineApp::HeightMapGenerator::Interpolate(float a, float b, float x)
{
    float f = Fade(x);
    return a * (1.0f - f) + b * f;
}

float BaselineApp::HeightMapGenerator::Fade(float t)
{
    // Smoothstep function: 6t^5 - 15t^4 + 10t^3
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}
```

---

## Phase 4: Terrain Tile System

### Step 4.1: Define TerrainTile Class (Nested in BaselineApp)

**Location**: In `BaselineApp` class, private section (after HeightMapGenerator)

```cpp
    // ============================================================================
    // TERRAIN RENDERER - Terrain Tile
    // ============================================================================
    
    class TerrainTile
    {
    public:
        TerrainTile();
        ~TerrainTile() = default;
        
        void Initialize(
            ID3D12Device* device, 
            ID3D12GraphicsCommandList* cmdList,
            const std::vector<float>& heights, 
            int resolution,
            float worldSize, 
            const DirectX::XMFLOAT3& center);
        
        void Render(
            ID3D12GraphicsCommandList* cmdList, 
            ID3D12Resource* objectCB,
            UINT objCBByteSize, 
            int lodLevel, 
            UINT passCBIndex);
        
        D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const;
        D3D12_INDEX_BUFFER_VIEW IndexBufferView() const;
        
        const DirectX::BoundingBox& GetBoundingBox() const { return mBoundingBox; }
        UINT GetIndexCount() const { return mIndexCount; }

    private:
        void CalculateNormals(
            std::vector<TerrainVertex>& vertices, 
            const std::vector<float>& heights, 
            int resolution);
        void CreateMesh(
            const std::vector<float>& heights, 
            int resolution, 
            float worldSize, 
            const DirectX::XMFLOAT3& center);
        
        ComPtr<ID3D12Resource> mVertexBuffer;
        ComPtr<ID3D12Resource> mVertexBufferUpload;
        ComPtr<ID3D12Resource> mIndexBuffer;
        ComPtr<ID3D12Resource> mIndexBufferUpload;
        
        UINT mVertexByteStride;
        UINT mVertexBufferByteSize;
        UINT mIndexBufferByteSize;
        UINT mIndexCount;
        
        DirectX::BoundingBox mBoundingBox;
    };
```

---

### Step 4.2: Implement TerrainTile Methods

**Location**: After HeightMapGenerator implementation

```cpp
// ============================================================================
// TERRAIN RENDERER - Terrain Tile Implementation
// ============================================================================

BaselineApp::TerrainTile::TerrainTile()
    : mVertexByteStride(0), mVertexBufferByteSize(0), 
      mIndexBufferByteSize(0), mIndexCount(0)
{
}

void BaselineApp::TerrainTile::Initialize(
    ID3D12Device* device, 
    ID3D12GraphicsCommandList* cmdList,
    const std::vector<float>& heights, 
    int resolution,
    float worldSize, 
    const DirectX::XMFLOAT3& center)
{
    CreateMesh(heights, resolution, worldSize, center);
}

void BaselineApp::TerrainTile::Render(
    ID3D12GraphicsCommandList* cmdList, 
    ID3D12Resource* objectCB,
    UINT objCBByteSize, 
    int lodLevel, 
    UINT passCBIndex)
{
    // Set vertex and index buffers
    cmdList->IASetVertexBuffers(0, 1, &VertexBufferView());
    cmdList->IASetIndexBuffer(&IndexBufferView());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    // Set object constant buffer
    D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + lodLevel * objCBByteSize;
    cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
    
    // Draw
    cmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
}

D3D12_VERTEX_BUFFER_VIEW BaselineApp::TerrainTile::VertexBufferView() const
{
    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    vbv.StrideInBytes = mVertexByteStride;
    vbv.SizeInBytes = mVertexBufferByteSize;
    return vbv;
}

D3D12_INDEX_BUFFER_VIEW BaselineApp::TerrainTile::IndexBufferView() const
{
    D3D12_INDEX_BUFFER_VIEW ibv;
    ibv.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    ibv.SizeInBytes = mIndexBufferByteSize;
    ibv.Format = DXGI_FORMAT_R32_UINT; // 32-bit indices for large tiles
    return ibv;
}

void BaselineApp::TerrainTile::CalculateNormals(
    std::vector<TerrainVertex>& vertices, 
    const std::vector<float>& heights, 
    int resolution)
{
    int width = resolution;
    int height = resolution;
    
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int index = y * width + x;
            DirectX::XMFLOAT3 normal(0.0f, 1.0f, 0.0f);
            
            // Calculate normal using neighboring vertices
            if (x > 0 && x < width - 1 && y > 0 && y < height - 1)
            {
                float leftHeight = heights[y * width + (x - 1)];
                float rightHeight = heights[y * width + (x + 1)];
                float bottomHeight = heights[(y - 1) * width + x];
                float topHeight = heights[(y + 1) * width + x];
                
                // Vectors for normal calculation
                DirectX::XMFLOAT3 tangent(
                    2.0f, // X difference
                    rightHeight - leftHeight,
                    0.0f
                );
                
                DirectX::XMFLOAT3 bitangent(
                    0.0f,
                    topHeight - bottomHeight,
                    2.0f // Z difference
                );
                
                // Cross product for normal
                normal.x = tangent.y * bitangent.z - tangent.z * bitangent.y;
                normal.y = tangent.z * bitangent.x - tangent.x * bitangent.z;
                normal.z = tangent.x * bitangent.y - tangent.y * bitangent.x;
                
                // Normalize
                float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                if (length > 0.0f)
                {
                    normal.x /= length;
                    normal.y /= length;
                    normal.z /= length;
                }
            }
            
            vertices[index].Normal = normal;
        }
    }
}

void BaselineApp::TerrainTile::CreateMesh(
    const std::vector<float>& heights, 
    int resolution, 
    float worldSize, 
    const DirectX::XMFLOAT3& center)
{
    int width = resolution;
    int height = resolution;
    float halfWorldSize = worldSize * 0.5f;
    float cellSize = worldSize / (width - 1);
    
    // Create vertices
    std::vector<TerrainVertex> vertices(width * height);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int index = y * width + x;
            float worldX = center.x - halfWorldSize + x * cellSize;
            float worldZ = center.z - halfWorldSize + y * cellSize;
            float heightValue = heights[index];
            
            vertices[index].Position = {worldX, heightValue, worldZ};
            vertices[index].TexCoord = {
                static_cast<float>(x) / (width - 1), 
                static_cast<float>(y) / (height - 1)
            };
        }
    }
    
    // Calculate normals
    CalculateNormals(vertices, heights, resolution);
    
    // Create indices (triangles)
    std::vector<uint32_t> indices;
    indices.reserve((width - 1) * (height - 1) * 6);
    
    for (int y = 0; y < height - 1; ++y)
    {
        for (int x = 0; x < width - 1; ++x)
        {
            uint32_t topLeft = y * width + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (y + 1) * width + x;
            uint32_t bottomRight = bottomLeft + 1;
            
            // First triangle
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            // Second triangle
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
    
    mIndexCount = static_cast<UINT>(indices.size());
    
    // Setup vertex buffer
    mVertexByteStride = sizeof(TerrainVertex);
    mVertexBufferByteSize = static_cast<UINT>(vertices.size()) * mVertexByteStride;
    
    // Setup index buffer
    mIndexBufferByteSize = static_cast<UINT>(indices.size()) * sizeof(uint32_t);
    
    // Calculate bounding box
    DirectX::XMFLOAT3 minCorner(FLT_MAX, FLT_MAX, FLT_MAX);
    DirectX::XMFLOAT3 maxCorner(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    
    for (const auto& vertex : vertices)
    {
        minCorner.x = std::min(minCorner.x, vertex.Position.x);
        minCorner.y = std::min(minCorner.y, vertex.Position.y);
        minCorner.z = std::min(minCorner.z, vertex.Position.z);
        
        maxCorner.x = std::max(maxCorner.x, vertex.Position.x);
        maxCorner.y = std::max(maxCorner.y, vertex.Position.y);
        maxCorner.z = std::max(maxCorner.z, vertex.Position.z);
    }
    
    DirectX::BoundingBox::CreateFromPoints(
        mBoundingBox, 
        DirectX::XMLoadFloat3(&minCorner), 
        DirectX::XMLoadFloat3(&maxCorner));
    
    // Create and fill buffers
    // Vertex buffer
    ThrowIfFailed(D3DCreateBlob(mVertexBufferByteSize, &mVertexBufferUpload));
    CopyMemory(mVertexBufferUpload->GetBufferPointer(), vertices.data(), mVertexBufferByteSize);
    
    mVertexBuffer = d3dUtil::CreateDefaultBuffer(
        device, cmdList,
        vertices.data(), mVertexBufferByteSize, mVertexBufferUpload);
    
    // Index buffer
    ThrowIfFailed(D3DCreateBlob(mIndexBufferByteSize, &mIndexBufferUpload));
    CopyMemory(mIndexBufferUpload->GetBufferPointer(), indices.data(), mIndexBufferByteSize);
    
    mIndexBuffer = d3dUtil::CreateDefaultBuffer(
        device, cmdList,
        indices.data(), mIndexBufferByteSize, mIndexBufferUpload);
}
```

---

## Phase 5: Quad-Tree LOD System

### Step 5.1: Define QuadTreeNode Class (Nested in BaselineApp)

**Location**: In `BaselineApp` class, private section (after TerrainTile)

```cpp
    // ============================================================================
    // TERRAIN RENDERER - Quad Tree Node
    // ============================================================================
    
    class QuadTreeNode
    {
    public:
        QuadTreeNode(
            BaselineApp* app,
            const DirectX::XMFLOAT3& center, 
            float halfSize, 
            int depth);
        ~QuadTreeNode() = default;
        
        void Update(const DirectX::XMFLOAT3& cameraPosition, const Frustum& frustum);
        void Render(
            ID3D12GraphicsCommandList* cmdList, 
            ID3D12Resource* objectCB, 
            UINT objCBByteSize, 
            UINT passCBIndex);
        
        void Subdivide();
        void Merge();
        
        const DirectX::XMFLOAT3& GetCenter() const { return mCenter; }
        float GetHalfSize() const { return mHalfSize; }
        int GetDepth() const { return mDepth; }
        bool IsLeaf() const { return mChildren.empty(); }
        bool NeedsSubdivision(const DirectX::XMFLOAT3& cameraPosition) const;
        bool ShouldMerge(const DirectX::XMFLOAT3& cameraPosition) const;
        
    private:
        void BuildTileGeometry();
        
        BaselineApp* mApp;
        DirectX::XMFLOAT3 mCenter;
        float mHalfSize;
        int mDepth;
        
        std::vector<std::unique_ptr<QuadTreeNode>> mChildren;
        std::unique_ptr<TerrainTile> mTile;
        
        float mMaxScreenError = 5.0f;
        bool mIsVisible = false;
        DirectX::BoundingBox mBoundingBox;
    };
```

---

### Step 5.2: Define QuadTree Class (Nested in BaselineApp)

**Location**: In `BaselineApp` class, private section (after QuadTreeNode)

```cpp
    // ============================================================================
    // TERRAIN RENDERER - Quad Tree
    // ============================================================================
    
    class QuadTree
    {
    public:
        QuadTree(
            BaselineApp* app,
            float terrainSize, 
            int maxDepth, 
            int tileResolution);
        ~QuadTree() = default;
        
        void Update(const DirectX::XMFLOAT3& cameraPosition, const Frustum& frustum);
        void Render(
            ID3D12GraphicsCommandList* cmdList, 
            ID3D12Resource* objectCB, 
            UINT objCBByteSize, 
            UINT passCBIndex);
        
        HeightMapGenerator* GetHeightMapGenerator() const { return mHeightMapGenerator.get(); }
        int GetMaxDepth() const { return mMaxDepth; }
        float GetLODDistanceFactor() const { return mLODDistanceFactor; }
        int GetTileResolution() const { return mTileResolution; }
        
        void SetLODDistanceFactor(float factor) { mLODDistanceFactor = factor; }

    private:
        BaselineApp* mApp;
        std::unique_ptr<QuadTreeNode> mRootNode;
        std::unique_ptr<HeightMapGenerator> mHeightMapGenerator;
        
        float mTerrainSize;
        int mMaxDepth;
        int mTileResolution;
        float mLODDistanceFactor;
    };
```

---

### Step 5.3: Add QuadTree Member to BaselineApp

**Location**: In `BaselineApp` class, private members section (around line 90)

```cpp
    // ============================================================================
    // TERRAIN RENDERER - Member Variables
    // ============================================================================
    
    // Terrain system
    std::unique_ptr<QuadTree> mTerrainQuadTree;
    std::unique_ptr<HeightMapGenerator> mHeightMapGenerator; // Optional: direct access
    
    // Terrain parameters
    float mTerrainSize = 1000.0f;        // Size of terrain in world units
    int mMaxQuadTreeDepth = 6;          // Maximum depth of quad tree
    float mLODDistanceFactor = 1.0f;    // LOD distance multiplier
    int mTileResolution = 65;           // Vertices per tile (65x65)
```

---

### Step 5.4: Implement QuadTreeNode Methods

**Location**: After TerrainTile implementation

```cpp
// ============================================================================
// TERRAIN RENDERER - Quad Tree Node Implementation
// ============================================================================

BaselineApp::QuadTreeNode::QuadTreeNode(
    BaselineApp* app,
    const DirectX::XMFLOAT3& center, 
    float halfSize, 
    int depth)
    : mApp(app), mCenter(center), mHalfSize(halfSize), mDepth(depth)
{
    // Calculate bounding box for node
    DirectX::XMVECTOR minCorner = DirectX::XMVectorSet(
        center.x - halfSize, 0.0f, center.z - halfSize, 1.0f);
    DirectX::XMVECTOR maxCorner = DirectX::XMVectorSet(
        center.x + halfSize, 100.0f, center.z + halfSize, 1.0f); // 100.0f = max height
    
    DirectX::BoundingBox::CreateFromPoints(mBoundingBox, minCorner, maxCorner);
    
    // Create tile geometry
    BuildTileGeometry();
}

void BaselineApp::QuadTreeNode::Update(
    const DirectX::XMFLOAT3& cameraPosition, 
    const Frustum& frustum)
{
    // Frustum culling
    mIsVisible = frustum.Intersects(mBoundingBox);
    
    if (!mIsVisible)
        return;
    
    // Check if subdivision is needed
    if (mDepth < mApp->mMaxQuadTreeDepth && NeedsSubdivision(cameraPosition))
    {
        if (mChildren.empty())
        {
            Subdivide();
        }
        
        // Recursively update children
        for (auto& child : mChildren)
        {
            child->Update(cameraPosition, frustum);
        }
    }
    else
    {
        // Check if merge is needed
        if (!mChildren.empty() && ShouldMerge(cameraPosition))
        {
            Merge();
        }
    }
}

void BaselineApp::QuadTreeNode::Render(
    ID3D12GraphicsCommandList* cmdList, 
    ID3D12Resource* objectCB, 
    UINT objCBByteSize, 
    UINT passCBIndex)
{
    if (!mIsVisible)
        return;
    
    if (!mChildren.empty())
    {
        // Render children instead of self
        for (auto& child : mChildren)
        {
            child->Render(cmdList, objectCB, objCBByteSize, passCBIndex);
        }
    }
    else if (mTile)
    {
        // Render current tile
        mTile->Render(cmdList, objectCB, objCBByteSize, mDepth, passCBIndex);
    }
}

void BaselineApp::QuadTreeNode::Subdivide()
{
    if (!mChildren.empty())
        return;
    
    float childHalfSize = mHalfSize * 0.5f;
    int childDepth = mDepth + 1;
    
    // Create 4 child nodes
    mChildren.reserve(4);
    
    // Northwest child
    DirectX::XMFLOAT3 nwCenter(
        mCenter.x - childHalfSize,
        mCenter.y,
        mCenter.z - childHalfSize
    );
    mChildren.push_back(std::make_unique<QuadTreeNode>(
        mApp, nwCenter, childHalfSize, childDepth));
    
    // Northeast child
    DirectX::XMFLOAT3 neCenter(
        mCenter.x + childHalfSize,
        mCenter.y,
        mCenter.z - childHalfSize
    );
    mChildren.push_back(std::make_unique<QuadTreeNode>(
        mApp, neCenter, childHalfSize, childDepth));
    
    // Southwest child
    DirectX::XMFLOAT3 swCenter(
        mCenter.x - childHalfSize,
        mCenter.y,
        mCenter.z + childHalfSize
    );
    mChildren.push_back(std::make_unique<QuadTreeNode>(
        mApp, swCenter, childHalfSize, childDepth));
    
    // Southeast child
    DirectX::XMFLOAT3 seCenter(
        mCenter.x + childHalfSize,
        mCenter.y,
        mCenter.z + childHalfSize
    );
    mChildren.push_back(std::make_unique<QuadTreeNode>(
        mApp, seCenter, childHalfSize, childDepth));
}

void BaselineApp::QuadTreeNode::Merge()
{
    mChildren.clear();
}

bool BaselineApp::QuadTreeNode::NeedsSubdivision(const DirectX::XMFLOAT3& cameraPosition) const
{
    if (mDepth >= mApp->mMaxQuadTreeDepth)
        return false;
    
    // Distance from camera to node center
    float dx = cameraPosition.x - mCenter.x;
    float dz = cameraPosition.z - mCenter.z;
    float distance = sqrtf(dx * dx + dz * dz);
    
    // Calculate LOD distance
    float lodDistance = mApp->mLODDistanceFactor * mHalfSize * (1 << mDepth);
    
    return distance < lodDistance;
}

bool BaselineApp::QuadTreeNode::ShouldMerge(const DirectX::XMFLOAT3& cameraPosition) const
{
    // Distance from camera to node center
    float dx = cameraPosition.x - mCenter.x;
    float dz = cameraPosition.z - mCenter.z;
    float distance = sqrtf(dx * dx + dz * dz);
    
    // Merge distance (slightly larger than subdivision distance)
    float mergeDistance = mApp->mLODDistanceFactor * mHalfSize * (1 << (mDepth - 1)) * 1.5f;
    
    return distance > mergeDistance;
}

void BaselineApp::QuadTreeNode::BuildTileGeometry()
{
    // Generate geometry for current tile
    mTile = std::make_unique<TerrainTile>();
    
    // Determine tile size in world coordinates
    int tileSize = mApp->mTileResolution;
    float worldSize = mHalfSize * 2.0f;
    
    // Generate heights for tile
    std::vector<float> heights;
    DirectX::XMFLOAT2 offset(
        mCenter.x - mHalfSize,
        mCenter.z - mHalfSize
    );
    
    mApp->mTerrainQuadTree->GetHeightMapGenerator()->GenerateHeightMap(heights, offset);
    
    // Create tile geometry
    mTile->Initialize(
        mApp->md3dDevice.Get(),
        mApp->mCommandList.Get(),
        heights,
        tileSize,
        worldSize,
        mCenter
    );
}
```

---

### Step 5.5: Implement QuadTree Methods

**Location**: After QuadTreeNode implementation

```cpp
// ============================================================================
// TERRAIN RENDERER - Quad Tree Implementation
// ============================================================================

BaselineApp::QuadTree::QuadTree(
    BaselineApp* app,
    float terrainSize, 
    int maxDepth, 
    int tileResolution)
    : mApp(app),
      mTerrainSize(terrainSize), 
      mMaxDepth(maxDepth), 
      mTileResolution(tileResolution),
      mLODDistanceFactor(1.0f)
{
    // Create height map generator
    mHeightMapGenerator = std::make_unique<HeightMapGenerator>(
        tileResolution, tileResolution, terrainSize / tileResolution);
    mHeightMapGenerator->SetOctaves(6);
    mHeightMapGenerator->SetFrequency(0.005f);
    mHeightMapGenerator->SetAmplitude(50.0f);
    mHeightMapGenerator->SetSeed(42);
    
    // Create root node
    DirectX::XMFLOAT3 center(0.0f, 0.0f, 0.0f);
    float halfSize = terrainSize * 0.5f;
    mRootNode = std::make_unique<QuadTreeNode>(app, center, halfSize, 0);
}

void BaselineApp::QuadTree::Update(
    const DirectX::XMFLOAT3& cameraPosition, 
    const Frustum& frustum)
{
    mRootNode->Update(cameraPosition, frustum);
}

void BaselineApp::QuadTree::Render(
    ID3D12GraphicsCommandList* cmdList, 
    ID3D12Resource* objectCB, 
    UINT objCBByteSize, 
    UINT passCBIndex)
{
    mRootNode->Render(cmdList, objectCB, objCBByteSize, passCBIndex);
}
```

---

## Phase 6: Water Renderer

### Step 6.1: Define WaterRenderer Class (Nested in BaselineApp)

**Location**: In `BaselineApp` class, private section (after QuadTree)

```cpp
    // ============================================================================
    // TERRAIN RENDERER - Water Renderer
    // ============================================================================
    
    class WaterRenderer
    {
    public:
        WaterRenderer(
            BaselineApp* app,
            float gridSize, 
            float gridSpacing);
        ~WaterRenderer() = default;
        
        void Update(const DirectX::XMFLOAT3& cameraPosition, float deltaTime);
        void Render(
            ID3D12GraphicsCommandList* cmdList, 
            ID3D12Resource* passCB, 
            UINT passCBByteSize);
        
        void SetWaveSpeed(float speed) { mWaveSpeed = speed; }
        void SetWaveHeight(float height) { mWaveHeight = height; }
        void SetWaterColor(const DirectX::XMFLOAT4& color) { mWaterColor = color; }
        
        float GetWaveSpeed() const { return mWaveSpeed; }
        float GetWaveHeight() const { return mWaveHeight; }
        
    private:
        void BuildWaterGeometry();
        void BuildWaterPSO();
        void UpdateWaveBuffer(float time);
        
        struct WaveConstants
        {
            float Time;
            float WaveSpeed;
            float WaveHeight;
            float Padding;
        };
        
        BaselineApp* mApp;
        
        ComPtr<ID3D12RootSignature> mRootSignature;
        ComPtr<ID3D12PipelineState> mPSO;
        ComPtr<ID3DBlob> mWaterVS;
        ComPtr<ID3DBlob> mWaterPS;
        
        ComPtr<ID3D12Resource> mVertexBuffer;
        ComPtr<ID3D12Resource> mVertexBufferUpload;
        ComPtr<ID3D12Resource> mIndexBuffer;
        ComPtr<ID3D12Resource> mIndexBufferUpload;
        ComPtr<ID3D12Resource> mWaveConstantBuffer;
        ComPtr<ID3D12Resource> mWaveConstantBufferUpload;
        
        D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
        D3D12_INDEX_BUFFER_VIEW mIndexBufferView;
        
        UINT mVertexCount;
        UINT mIndexCount;
        UINT mVBByteStride;
        
        float mGridSize;
        float mGridSpacing;
        float mTime = 0.0f;
        float mWaveSpeed = 1.0f;
        float mWaveHeight = 0.5f;
        DirectX::XMFLOAT4 mWaterColor = {0.0f, 0.3f, 0.8f, 1.0f};
    };
```

---

### Step 6.2: Add WaterRenderer Member to BaselineApp

**Location**: In `BaselineApp` class, private members section (after QuadTree member)

```cpp
    // Water system
    std::unique_ptr<WaterRenderer> mWaterRenderer;
```

---

### Step 6.3: Implement WaterRenderer Methods

**Location**: After QuadTree implementation

```cpp
// ============================================================================
// TERRAIN RENDERER - Water Renderer Implementation
// ============================================================================

BaselineApp::WaterRenderer::WaterRenderer(
    BaselineApp* app,
    float gridSize, 
    float gridSpacing)
    : mApp(app), mGridSize(gridSize), mGridSpacing(gridSpacing)
{
    BuildWaterGeometry();
    BuildWaterPSO();
}

void BaselineApp::WaterRenderer::Update(const DirectX::XMFLOAT3& cameraPosition, float deltaTime)
{
    mTime += deltaTime;
}

void BaselineApp::WaterRenderer::Render(
    ID3D12GraphicsCommandList* cmdList, 
    ID3D12Resource* passCB, 
    UINT passCBByteSize)
{
    // Set PSO and root signature
    cmdList->SetPipelineState(mPSO.Get());
    cmdList->SetGraphicsRootSignature(mRootSignature.Get());
    
    // Set pass constant buffer
    cmdList->SetGraphicsRootConstantBufferView(0, passCB->GetGPUVirtualAddress());
    
    // Update wave constants
    WaveConstants waveConstants;
    waveConstants.Time = mTime;
    waveConstants.WaveSpeed = mWaveSpeed;
    waveConstants.WaveHeight = mWaveHeight;
    waveConstants.Padding = 0.0f;
    
    void* mappedData = nullptr;
    ThrowIfFailed(mWaveConstantBuffer->Map(0, nullptr, &mappedData));
    memcpy(mappedData, &waveConstants, sizeof(WaveConstants));
    mWaveConstantBuffer->Unmap(0, nullptr);
    
    // Set wave constant buffer
    cmdList->SetGraphicsRootConstantBufferView(1, mWaveConstantBuffer->GetGPUVirtualAddress());
    
    // Set vertex and index buffers
    cmdList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    cmdList->IASetIndexBuffer(&mIndexBufferView);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    // Draw
    cmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
}

void BaselineApp::WaterRenderer::BuildWaterGeometry()
{
    int vertexCountPerSide = static_cast<int>(mGridSize / mGridSpacing) + 1;
    mVertexCount = vertexCountPerSide * vertexCountPerSide;
    mIndexCount = (vertexCountPerSide - 1) * (vertexCountPerSide - 1) * 6;
    mVBByteStride = sizeof(WaterVertex);
    
    std::vector<WaterVertex> vertices(mVertexCount);
    std::vector<uint16_t> indices(mIndexCount);
    
    float halfGridSize = mGridSize * 0.5f;
    
    // Create vertices
    for (int z = 0; z < vertexCountPerSide; ++z)
    {
        for (int x = 0; x < vertexCountPerSide; ++x)
        {
            int index = z * vertexCountPerSide + x;
            float worldX = -halfGridSize + x * mGridSpacing;
            float worldZ = -halfGridSize + z * mGridSpacing;
            
            vertices[index].Position = {worldX, 0.0f, worldZ};
            vertices[index].TexCoord = {
                static_cast<float>(x) / (vertexCountPerSide - 1),
                static_cast<float>(z) / (vertexCountPerSide - 1)
            };
        }
    }
    
    // Create indices
    int idx = 0;
    for (int z = 0; z < vertexCountPerSide - 1; ++z)
    {
        for (int x = 0; x < vertexCountPerSide - 1; ++x)
        {
            uint16_t topLeft = z * vertexCountPerSide + x;
            uint16_t topRight = topLeft + 1;
            uint16_t bottomLeft = (z + 1) * vertexCountPerSide + x;
            uint16_t bottomRight = bottomLeft + 1;
            
            // First triangle
            indices[idx++] = topLeft;
            indices[idx++] = bottomLeft;
            indices[idx++] = topRight;
            
            // Second triangle
            indices[idx++] = topRight;
            indices[idx++] = bottomLeft;
            indices[idx++] = bottomRight;
        }
    }
    
    // Create buffers
    const UINT vbByteSize = mVertexCount * mVBByteStride;
    const UINT ibByteSize = mIndexCount * sizeof(uint16_t);
    
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mVertexBufferUpload));
    CopyMemory(mVertexBufferUpload->GetBufferPointer(), vertices.data(), vbByteSize);
    
    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mIndexBufferUpload));
    CopyMemory(mIndexBufferUpload->GetBufferPointer(), indices.data(), ibByteSize);
    
    mVertexBuffer = d3dUtil::CreateDefaultBuffer(
        mApp->md3dDevice.Get(), mApp->mCommandList.Get(),
        vertices.data(), vbByteSize, mVertexBufferUpload);
    
    mIndexBuffer = d3dUtil::CreateDefaultBuffer(
        mApp->md3dDevice.Get(), mApp->mCommandList.Get(),
        indices.data(), ibByteSize, mIndexBufferUpload);
    
    // Setup views
    mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    mVertexBufferView.StrideInBytes = mVBByteStride;
    mVertexBufferView.SizeInBytes = vbByteSize;
    
    mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    mIndexBufferView.SizeInBytes = ibByteSize;
    mIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    
    // Create wave constant buffer
    const UINT waveCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(WaveConstants));
    mWaveConstantBuffer = d3dUtil::CreateDefaultBuffer(
        mApp->md3dDevice.Get(), mApp->mCommandList.Get(),
        waveCBByteSize, mWaveConstantBufferUpload);
}

void BaselineApp::WaterRenderer::BuildWaterPSO()
{
    // Compile shaders
    mWaterVS = d3dUtil::CompileShader(L"Shaders\\Water.hlsl", nullptr, "VS", "vs_5_1");
    mWaterPS = d3dUtil::CompileShader(L"Shaders\\Water.hlsl", nullptr, "PS", "ps_5_1");
    
    // Create root signature
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    slotRootParameter[0].InitAsConstantBufferView(0); // Pass constants (b0)
    slotRootParameter[1].InitAsConstantBufferView(1); // Wave constants (b1)
    
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    
    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);
    
    ThrowIfFailed(mApp->md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature)));
    
    // Input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    
    // PSO description
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = { 
        reinterpret_cast<BYTE*>(mWaterVS->GetBufferPointer()), 
        mWaterVS->GetBufferSize() 
    };
    psoDesc.PS = { 
        reinterpret_cast<BYTE*>(mWaterPS->GetBufferPointer()),
        mWaterPS->GetBufferSize() 
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // Don't cull water polygons
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write to depth
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mApp->mBackBufferFormat;
    psoDesc.SampleDesc.Count = mApp->m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = mApp->m4xMsaaState ? (mApp->m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mApp->mDepthStencilFormat;
    
    ThrowIfFailed(mApp->md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}
```

---

## Phase 7: Integration into BaselineApp

### Step 7.1: Modify Initialize() Method

**Location**: In `BaselineApp::Initialize()` (around line 126)

**Replace the existing Initialize() method**:

```cpp
bool BaselineApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildRootSignature();
    BuildShadersAndInputLayout();
    
    // ============================================================================
    // TERRAIN RENDERER - Build terrain and water systems
    // ============================================================================
    BuildTerrain();
    BuildWater();
    
    // Original cube geometry (can be removed later)
    BuildCubeGeometry();
    BuildRenderItems();
    BuildPSOs();
    BuildFrameResources();

    // Initialize camera
    mCamera.SetPosition(0.0f, 50.0f, -100.0f); // Higher position for terrain view
    mCamera.LookAt(
        XMVectorSet(0.0f, 50.0f, -100.0f, 0.0f), 
        XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), 
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    mCamera.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 2000.0f); // Extended far plane
    mCamera.UpdateViewMatrix();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();
    return true;
}
```

---

### Step 7.2: Add BuildTerrain() Method

**Location**: After `BuildRenderItems()` method (around line 401)

```cpp
// ============================================================================
// TERRAIN RENDERER - Build Methods
// ============================================================================

void BaselineApp::BuildTerrain()
{
    // Initialize quad tree
    mTerrainQuadTree = std::make_unique<QuadTree>(
        this,
        mTerrainSize,
        mMaxQuadTreeDepth,
        mTileResolution
    );
    
    // Set LOD parameters
    mTerrainQuadTree->SetLODDistanceFactor(mLODDistanceFactor);
}
```

---

### Step 7.3: Add BuildWater() Method

**Location**: After `BuildTerrain()` method

```cpp
void BaselineApp::BuildWater()
{
    // Initialize water renderer
    float waterGridSize = mTerrainSize * 2.0f; // Larger than terrain
    float waterGridSpacing = 5.0f; // Grid spacing for water
    
    mWaterRenderer = std::make_unique<WaterRenderer>(
        this,
        waterGridSize,
        waterGridSpacing
    );
    
    // Configure water parameters
    mWaterRenderer->SetWaveSpeed(0.8f);
    mWaterRenderer->SetWaveHeight(0.3f);
    mWaterRenderer->SetWaterColor({0.0f, 0.4f, 0.7f, 1.0f});
}
```

---

### Step 7.4: Modify BuildShadersAndInputLayout() Method

**Location**: In `BuildShadersAndInputLayout()` (around line 278)

**Replace the method**:

```cpp
void BaselineApp::BuildShadersAndInputLayout()
{
    // Original pyramid shader (for cube)
    mShaders["pyramidVS"] = d3dUtil::CompileShader(L"Shaders\\Pyramid.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["pyramidPS"] = d3dUtil::CompileShader(L"Shaders\\Pyramid.hlsl", nullptr, "PS", "ps_5_1");

    // ============================================================================
    // TERRAIN RENDERER - Terrain shaders
    // ============================================================================
    mShaders["terrainVS"] = d3dUtil::CompileShader(L"Shaders\\Terrain.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["terrainPS"] = d3dUtil::CompileShader(L"Shaders\\Terrain.hlsl", nullptr, "PS", "ps_5_1");

    // Input layout for terrain
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}
```

---

### Step 7.5: Modify BuildPSOs() Method

**Location**: In `BuildPSOs()` (around line 351)

**Replace the method**:

```cpp
void BaselineApp::BuildPSOs()
{
    // ============================================================================
    // TERRAIN RENDERER - Terrain PSO
    // ============================================================================
    D3D12_GRAPHICS_PIPELINE_STATE_DESC terrainPsoDesc;
    ZeroMemory(&terrainPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    terrainPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    terrainPsoDesc.pRootSignature = mRootSignature.Get();
    terrainPsoDesc.VS = 
    { 
        reinterpret_cast<BYTE*>(mShaders["terrainVS"]->GetBufferPointer()), 
        mShaders["terrainVS"]->GetBufferSize()
    };
    terrainPsoDesc.PS = 
    { 
        reinterpret_cast<BYTE*>(mShaders["terrainPS"]->GetBufferPointer()),
        mShaders["terrainPS"]->GetBufferSize()
    };
    terrainPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    terrainPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    terrainPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    terrainPsoDesc.SampleMask = UINT_MAX;
    terrainPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    terrainPsoDesc.NumRenderTargets = 1;
    terrainPsoDesc.RTVFormats[0] = mBackBufferFormat;
    terrainPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    terrainPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    terrainPsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&terrainPsoDesc, IID_PPV_ARGS(&mPSOs["terrain"])));
    
    // Original opaque PSO (for cube)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = 
    { 
        reinterpret_cast<BYTE*>(mShaders["pyramidVS"]->GetBufferPointer()), 
        mShaders["pyramidVS"]->GetBufferSize()
    };
    psoDesc.PS = 
    { 
        reinterpret_cast<BYTE*>(mShaders["pyramidPS"]->GetBufferPointer()),
        mShaders["pyramidPS"]->GetBufferSize()
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));
}
```

---

### Step 7.6: Modify Update() Method

**Location**: In `Update()` (around line 172)

**Replace the method**:

```cpp
void BaselineApp::Update(const GameTimer& gt)
{
    // ============================================================================
    // TERRAIN RENDERER - Update frustum
    // ============================================================================
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    mFrustum.Update(viewProj);
    
    // Original cube rotation (can be removed)
    mCubeRotation += 1.0f * gt.DeltaTime();
    if(mCubeRotation > XM_2PI)
        mCubeRotation -= XM_2PI;

    // Update camera movement
    const float dt = gt.DeltaTime();
    const float moveSpeed = 5.0f;
    
    if(GetAsyncKeyState('W') & 0x8000)
        mCamera.Walk(moveSpeed * dt);
    if(GetAsyncKeyState('S') & 0x8000)
        mCamera.Walk(-moveSpeed * dt);
    if(GetAsyncKeyState('A') & 0x8000)
        mCamera.Strafe(-moveSpeed * dt);
    if(GetAsyncKeyState('D') & 0x8000)
        mCamera.Strafe(moveSpeed * dt);

    mCamera.UpdateViewMatrix();

    // ============================================================================
    // TERRAIN RENDERER - Update terrain and water
    // ============================================================================
    UpdateTerrain(gt);
    UpdateWater(gt);

    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    UpdateObjectCBs(gt);
    UpdatePassCB(gt);
}
```

---

### Step 7.7: Add UpdateTerrain() Method

**Location**: After `Update()` method

```cpp
// ============================================================================
// TERRAIN RENDERER - Update Methods
// ============================================================================

void BaselineApp::UpdateTerrain(const GameTimer& gt)
{
    // Get camera position
    DirectX::XMFLOAT3 cameraPosition;
    XMStoreFloat3(&cameraPosition, mCamera.GetPosition());
    
    // Update quad tree with frustum culling
    mTerrainQuadTree->Update(cameraPosition, mFrustum);
}
```

---

### Step 7.8: Add UpdateWater() Method

**Location**: After `UpdateTerrain()` method

```cpp
void BaselineApp::UpdateWater(const GameTimer& gt)
{
    // Get camera position
    DirectX::XMFLOAT3 cameraPosition;
    XMStoreFloat3(&cameraPosition, mCamera.GetPosition());
    
    // Update water
    mWaterRenderer->Update(cameraPosition, gt.DeltaTime());
}
```

---

### Step 7.9: Modify UpdatePassCB() Method

**Location**: In `UpdatePassCB()` (around line 418)

**Replace the method**:

```cpp
void BaselineApp::UpdatePassCB(const GameTimer& gt)
{
    // Use camera view matrix
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();
    
    XMStoreFloat4x4(&mView, view);
    XMStoreFloat4x4(&mProj, proj);

    auto currPassCB = mCurrFrameResource->PassCB.get();
    PassConstants passConstants;
    
    // ============================================================================
    // TERRAIN RENDERER - Extended pass constants
    // ============================================================================
    // Note: This assumes PassConstants was extended in BaselineFrameResource.h
    // If not, we need to handle terrain pass constants separately
    
    XMStoreFloat4x4(&passConstants.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&passConstants.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(XMMatrixMultiply(view, proj)));
    
    XMFLOAT3 eyePos;
    XMStoreFloat3(&eyePos, mCamera.GetPosition());
    passConstants.EyePosW = eyePos;
    
    passConstants.TotalTime = gt.TotalTime();
    passConstants.DeltaTime = gt.DeltaTime();
    passConstants.AmbientLight = {0.3f, 0.3f, 0.3f, 1.0f};
    
    currPassCB->CopyData(0, passConstants);
}
```

---

### Step 7.10: Modify Draw() Method

**Location**: In `Draw()` (around line 209)

**Replace the method**:

```cpp
void BaselineApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["terrain"].Get())); // Use terrain PSO

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

    // ============================================================================
    // TERRAIN RENDERER - Draw terrain
    // ============================================================================
    DrawTerrain(mCommandList.Get());
    
    // ============================================================================
    // TERRAIN RENDERER - Draw water (after terrain)
    // ============================================================================
    DrawWater(mCommandList.Get());

    // Original cube rendering (can be removed)
    DrawRenderItems(mCommandList.Get());

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    mCurrFrameResource->Fence = ++mCurrentFence;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}
```

---

### Step 7.11: Add DrawTerrain() Method

**Location**: After `DrawRenderItems()` method (around line 452)

```cpp
// ============================================================================
// TERRAIN RENDERER - Draw Methods
// ============================================================================

void BaselineApp::DrawTerrain(ID3D12GraphicsCommandList* cmdList)
{
    // Set terrain PSO
    cmdList->SetPipelineState(mPSOs["terrain"].Get());
    
    // Set root signature
    cmdList->SetGraphicsRootSignature(mRootSignature.Get());
    
    // Set pass constant buffer
    auto passCB = mCurrFrameResource->PassCB->Resource();
    cmdList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());
    
    // Render quad tree
    mTerrainQuadTree->Render(
        cmdList,
        mCurrFrameResource->ObjectCB->Resource(),
        d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants)),
        0 // Pass CB index
    );
}
```

---

### Step 7.12: Add DrawWater() Method

**Location**: After `DrawTerrain()` method

```cpp
void BaselineApp::DrawWater(ID3D12GraphicsCommandList* cmdList)
{
    // Render water (water renderer manages its own PSO and root signature)
    mWaterRenderer->Render(
        cmdList,
        mCurrFrameResource->PassCB->Resource(),
        d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants))
    );
}
```

---

### Step 7.13: Modify UpdateObjectCBs() Method

**Location**: In `UpdateObjectCBs()` (around line 403)

**Note**: This method currently updates the cube. We need to update it to handle terrain tiles. However, since terrain tiles manage their own object CBs through the quad tree, we can keep this for the cube or remove it if we remove the cube.

**For now, keep it as is** (cube will still render). Later, we can remove cube rendering.

---

### Step 7.14: Add Keyboard Controls

**Location**: In `OnKeyPressed()` (around line 489)

**Replace the method**:

```cpp
void BaselineApp::OnKeyPressed(const GameTimer& gt, WPARAM key)
{
    // ============================================================================
    // TERRAIN RENDERER - Keyboard controls
    // ============================================================================
    switch (key)
    {
    case '1': // Increase LOD distance factor
        mLODDistanceFactor *= 1.2f;
        mTerrainQuadTree->SetLODDistanceFactor(mLODDistanceFactor);
        break;
    case '2': // Decrease LOD distance factor
        mLODDistanceFactor /= 1.2f;
        mTerrainQuadTree->SetLODDistanceFactor(mLODDistanceFactor);
        break;
    case '3': // Increase wave height
        if (mWaterRenderer)
            mWaterRenderer->SetWaveHeight(mWaterRenderer->GetWaveHeight() * 1.2f);
        break;
    case '4': // Decrease wave height
        if (mWaterRenderer)
            mWaterRenderer->SetWaveHeight(mWaterRenderer->GetWaveHeight() / 1.2f);
        break;
    case '5': // Increase wave speed
        if (mWaterRenderer)
            mWaterRenderer->SetWaveSpeed(mWaterRenderer->GetWaveSpeed() * 1.2f);
        break;
    case '6': // Decrease wave speed
        if (mWaterRenderer)
            mWaterRenderer->SetWaveSpeed(mWaterRenderer->GetWaveSpeed() / 1.2f);
        break;
    }
    
    // WASD movement is handled in Update() via GetAsyncKeyState
}
```

---

## Phase 8: Shader Creation

### Step 8.1: Create Terrain.hlsl Shader

**Location**: Create new file `baseline/src/Shaders/Terrain.hlsl`

```hlsl
//***************************************************************************************
// Terrain.hlsl - Shader for terrain rendering with height-based coloring
//***************************************************************************************

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
}

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
}

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexCoord : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexCoord : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // Transform to world space
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    
    // Transform normal
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    
    // Transform to homogeneous clip space
    vout.PosH = mul(posW, gViewProj);
    
    // Pass through texture coordinates
    vout.TexCoord = vin.TexCoord;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Simple texture based on height
    float height = pin.PosW.y;
    float3 color;
    
    if (height < 10.0f)
        color = float3(0.2f, 0.6f, 0.2f); // Green for lowlands
    else if (height < 30.0f)
        color = float3(0.5f, 0.4f, 0.2f); // Brown for hills
    else
        color = float3(0.8f, 0.8f, 0.8f); // Gray for mountains
    
    // Simple lighting
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.5f));
    float diffuseFactor = max(0.0f, dot(normalize(pin.NormalW), lightDir));
    
    float3 ambient = gAmbientLight.rgb;
    float3 diffuse = color * diffuseFactor;
    
    float3 finalColor = ambient + diffuse;
    
    return float4(finalColor, 1.0f);
}
```

---

### Step 8.2: Create Water.hlsl Shader

**Location**: Create new file `baseline/src/Shaders/Water.hlsl`

```hlsl
//***************************************************************************************
// Water.hlsl - Shader for animated water surface
//***************************************************************************************

cbuffer cbPass : register(b0)
{
    float4x4 gViewProj;
    float3 gEyePosW;
    float gTotalTime;
}

cbuffer cbWave : register(b1)
{
    float gTime;
    float gWaveSpeed;
    float gWaveHeight;
    float gPadding;
}

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float2 TexCoord : TEXCOORD;
};

// Function for wave generation
float GerstnerWave(float x, float z, float time)
{
    float k = 0.1f; // Wave frequency
    float amplitude = gWaveHeight;
    float speed = gWaveSpeed;
    
    float phase = dot(float2(x, z), float2(1.0f, 0.3f)) * k + time * speed;
    return amplitude * sin(phase);
}

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // Generate waves
    float waveHeight = GerstnerWave(vin.PosL.x, vin.PosL.z, gTime);
    
    // Vertex position with waves
    float3 posW = float3(vin.PosL.x, waveHeight, vin.PosL.z);
    vout.PosW = posW;
    
    // Transform to homogeneous clip space
    vout.PosH = mul(float4(posW, 1.0f), gViewProj);
    
    // Animate texture coordinates
    vout.TexCoord = vin.TexCoord + float2(gTime * 0.05f, gTime * 0.03f);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Base water color
    float3 waterColor = float3(0.0f, 0.3f, 0.8f);
    
    // Depth-based transparency
    float depth = distance(pin.PosW, gEyePosW);
    float alpha = 1.0f - saturate(depth / 100.0f) * 0.3f;
    
    // Depth factor for color variation
    float depthFactor = saturate(pin.PosW.y / 10.0f);
    waterColor = lerp(waterColor * 0.7f, waterColor * 1.2f, depthFactor);
    
    // Add specular highlights
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.5f));
    float3 viewDir = normalize(gEyePosW - pin.PosW);
    float3 halfVector = normalize(lightDir + viewDir);
    
    float3 normal = normalize(float3(0.0f, 1.0f, 0.0f)); // Simplified normal
    float specFactor = pow(max(0.0f, dot(normal, halfVector)), 32.0f);
    
    waterColor += float3(1.0f, 1.0f, 1.0f) * specFactor * 0.5f;
    
    return float4(waterColor, alpha);
}
```

---

## Phase 9: Testing and Optimization

### Step 9.1: Modify BaselineFrameResource.h

**Location**: `baseline/src/BaselineFrameResource.h`

**Update PassConstants structure** (as mentioned in Phase 1, Step 1.3):

```cpp
// Simple constant buffer for pass data
struct PassConstants
{
    DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;
    DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
};
```

---

### Step 9.2: Build and Test

1. **Compile the project** - Ensure all shaders compile correctly
2. **Test basic rendering** - Verify terrain appears
3. **Test LOD system** - Move camera and verify LOD changes
4. **Test water rendering** - Verify water appears and animates
5. **Test frustum culling** - Verify performance improvement

---

### Step 9.3: Performance Optimization Notes

- **Tile caching**: Consider caching generated tiles to avoid regeneration
- **Memory management**: Monitor memory usage with large terrains
- **Shader optimization**: Profile shaders and optimize if needed
- **LOD tuning**: Adjust `mLODDistanceFactor` for optimal performance/quality

---

## Summary

This plan provides a complete implementation of a terrain renderer with:
- ✅ Quad-tree LOD system
- ✅ Frustum culling
- ✅ Height map generation
- ✅ Water surface with animation
- ✅ All code in Baseline.cpp (single file approach)

**Next Steps**:
1. Follow phases in order
2. Test after each major phase
3. Use [[L1inst_refactor_guide.md]] for later refactoring into separate files

---

## Notes for Cursor AI

When implementing with Cursor:
1. Follow phases sequentially
2. Test compilation after each phase
3. Use search/replace for exact code locations
4. Pay attention to line numbers (they may shift as code is added)
5. All nested classes are defined within BaselineApp class
6. All implementations are after the class definition

---

## Troubleshooting

### Common Issues:

1. **Shader compilation errors**: Check shader file paths and syntax
2. **Missing includes**: Ensure all DirectX headers are included
3. **Buffer size mismatches**: Verify constant buffer sizes match structures
4. **LOD not working**: Check camera position and LOD distance factor
5. **Water not rendering**: Verify water PSO is set correctly

---

**End of Implementation Plan**

