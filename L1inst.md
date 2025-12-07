```рассуждение
We are going to extend the provided BaselineApp to create a terrain rendering system with:
 1. Quad-tree for Level of Detail (LOD) selection.
 2. Frustum culling to avoid rendering invisible tiles.
 3. Procedural heightmap generation for terrain tiles.
 4. Infinite procedural water.

 We'll break down the steps:

 Step 1: Understand the BaselineApp structure
 Step 2: Design the terrain system
 Step 3: Implement the quad-tree for LOD
 Step 4: Implement frustum culling
 Step 5: Generate terrain tiles with heightmaps
 Step 6: Implement infinite procedural water
 Step 7: Integrate everything into the BaselineApp

 Let's start with a detailed step-by-step guide.

 Note: The BaselineApp already has a basic structure with a pyramid. We will replace the pyramid with terrain and water.

 Step 1: Understand the BaselineApp structure

 The BaselineApp has:
 - A pyramid geometry (we will remove this)
 - A rotation animation (we will remove this and replace with camera movement)
 - Basic rendering loop with frame resources, root signature, PSO, etc.

 We will keep the basic framework and replace the pyramid with terrain and water.

 Step 2: Design the terrain system

 We will generate a terrain composed of tiles. Each tile is a grid of vertices with a heightmap.
 We will use a quad-tree to manage the LOD of the terrain tiles. The quad-tree will be built such that:
 - The root node covers the entire terrain (which is infinite, but we'll generate around the camera)
 - Each node has four children, and we subdivide based on the distance from the camera.

 We need to define:
 - A TerrainNode class for the quad-tree.
 - A TerrainTile class for the actual geometry of a tile.

 Step 3: Implement the quad-tree for LOD

 Steps for the quad-tree:
 - We start with a root node at a certain level (e.g., level 0) covering a large area.
 - We traverse the tree and decide whether to subdivide a node based on the distance from the camera.
 - The subdivision criteria: if the node's bounding box is close enough to the camera, we split it into four children.

 We will need:
 - A bounding box for each node.
 - A way to compute the distance from the camera to the node.

 Step 4: Implement frustum culling

 We will extract the view-frustum planes from the view-projection matrix.
 For each node in the quad-tree, we check if its bounding box intersects the frustum. If not, we skip rendering that node.

 Step 5: Generate terrain tiles with heightmaps

 For each leaf node in the quad-tree (after subdivision), we generate a terrain tile.
 We will use a procedural heightmap function (e.g., Perlin noise) to generate heights for the vertices of the tile.

 Each tile is a grid of (2^LOD + 1) x (2^LOD + 1) vertices, where LOD is the level of the node (higher level means more detail).

 We will create a vertex buffer and index buffer for each tile (or use a shared one for same LOD?).

 Step 6: Implement infinite procedural water

 We will generate a water plane that extends infinitely. We can use a large grid that moves with the camera.
 We will use a shader to animate the water (e.g., with waves).

 Step 7: Integrate everything into the BaselineApp

 We will:
 - Remove the pyramid.
 - Add a camera that can move (we'll change the Update function to handle camera movement).
 - In the Draw function, we will traverse the quad-tree, perform frustum culling, and render the visible terrain tiles and the water.

 Now, let's write the step-by-step instructions for Cursor.

 Note: This is a complex system, so we'll break it into multiple steps and files.

 Instructions:

 1. Set up the project structure

 We assume the project already has the BaselineApp. We will create new files for the terrain and water.

 2. Create a Camera class

 We need a camera that can move freely in the world. We'll replace the fixed camera in the BaselineApp.

 3. Create a QuadTree class and TerrainNode class

 4. Create a TerrainTile class

 5. Create a Water class

 6. Modify the BaselineApp to use the new classes

 Let's write the instructions in detail.

 Due to the complexity, we'll outline the steps and provide code snippets for critical parts.

 Step 1: Create a Camera class

 We need a camera that can move (WASD) and look around (mouse). We'll update the view matrix accordingly.

 Step 2: Create the QuadTree and TerrainNode

 We'll define the QuadTree and TerrainNode in separate files.

 Step 3: Create the TerrainTile

 The TerrainTile will generate the geometry for a given node (position, size, and LOD).

 Step 4: Create the Water

 The Water will be a large grid that moves with the camera. We'll use a simple grid and a shader for animation.

 Step 5: Integrate into BaselineApp

 We'll replace the pyramid with the terrain and water.

 Let's write the instructions step by step.

 Note: The following instructions are for Cursor to generate the code.

 Instructions:

 1. **Camera Class**

    Create a `Camera` class that supports:
    - Movement: forward, backward, left, right, up, down.
    - Mouse look.
    - Update the view matrix.

    We'll use this camera in the `BaselineApp` to replace the fixed view.

 2. **QuadTree and TerrainNode**

    Create `TerrainNode` and `QuadTree` classes.

    `TerrainNode` should have:
    - Bounding box (min and max in xz plane, and min/max height? or we can compute from heightmap)
    - Level (0 for root, then increasing)
    - Pointers to children (if any)
    - A method to check if it should subdivide (based on distance to camera)
    - A method to generate the tile geometry (if it's a leaf)

    `QuadTree` should have:
    - Root node
    - A method to update the tree (traverse and subdivide or merge based on camera position)

 3. **TerrainTile**

    Create a `TerrainTile` class that can generate the mesh for a given node.

    We'll generate a grid of vertices with positions and normals (and maybe texture coordinates).
    The height of each vertex is determined by a height function (e.g., Perlin noise).

    We'll create vertex and index buffers for the tile.

 4. **Water**

    Create a `Water` class that generates a large grid (e.g., 100x100) that is always centered at the camera's xz position.
    We'll use a simple vertex shader and a pixel shader that animates the water (e.g., using time).

 5. **Integration**

    In `BaselineApp`:
    - Remove the pyramid geometry and render items.
    - Add a `Camera` instance.
    - Add a `QuadTree` instance for the terrain.
    - Add a `Water` instance.
    - Update the camera in the `Update` function.
    - In the `Draw` function, traverse the quad-tree, perform frustum culling, and render the visible terrain tiles.
    - Render the water.

 6. **Shaders**

    We'll need new shaders for the terrain and water.

 7. **Frustum Culling**

    We'll extract the frustum planes from the view-projection matrix and use them to cull nodes.

 8. **Procedural Generation**

    We'll use a noise function (like Perlin) for the heightmap.

 Given the complexity, we'll now write the code for each part in a step-by-step manner.

 However, note that the entire code would be too long to write here. We'll provide the structure and key code snippets.

 Let's start with the Camera class.

 Step 1: Camera Class

 Create `Camera.h` and `Camera.cpp`

 Step 2: QuadTree and TerrainNode

 Create `QuadTree.h` and `QuadTree.cpp`, `TerrainNode.h` and `TerrainNode.cpp`

 Step 3: TerrainTile

 Create `TerrainTile.h` and `TerrainTile.cpp`

 Step 4: Water

 Create `Water.h` and `Water.cpp`

 Step 5: Modify BaselineApp

 We'll modify `BaselineApp.h` and `BaselineApp.cpp`

 Step 6: Create new shaders for terrain and water.

 We'll create `Terrain.hlsl` and `Water.hlsl`

 Let's write the code for each part.

 Due to the length, I'll provide the code for the Camera class and then outline the other classes.

 Camera Class:

 Camera.h:
```
Here's a detailed step-by-step instruction guide for creating a terrain rendering system with quad-tree LOD, frustum culling, and procedural water using DirectX12:

## Step 1: Project Structure Setup
1. Create a new C++ project in Cursor using the existing BaselineApp as foundation
2. Create these new header files:
   - `TerrainQuadTree.h`
   - `TerrainNode.h`
   - `HeightMapGenerator.h`
   - `WaterRenderer.h`
   - `Frustum.h`

## Step 2: Implement Height Map Generator
```cpp
// HeightMapGenerator.h
class HeightMapGenerator {
public:
    static std::vector<float> GenerateHeightMap(int width, int height, float scale, XMFLOAT2 offset);
    static float GetHeightAt(float x, float z);
private:
    static float PerlinNoise(float x, float y);
    static float FractalBrownianMotion(float x, float y, int octaves, float persistence);
};
```

## Step 3: Create Terrain Node Structure
```cpp
// TerrainNode.h
struct TerrainNode {
    XMFLOAT2 center;
    float size;
    int lodLevel;
    bool isLeaf;
    std::array<std::unique_ptr<TerrainNode>, 4> children;
    BoundingBox bounds;
    std::unique_ptr<MeshGeometry> geometry;
    
    bool ShouldSplit(const Camera& camera);
    void CreateGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
};
```

## Step 4: Implement Quad-Tree LOD System
```cpp
// TerrainQuadTree.h
class TerrainQuadTree {
public:
    void Initialize(XMFLOAT2 center, float initialSize, int maxLOD);
    void Update(const Camera& camera);
    void BuildRenderItems(std::vector<RenderItem>& renderItems);
    void GenerateGeometryForVisibleNodes(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    
private:
    std::unique_ptr<TerrainNode> root;
    int maxLOD;
    std::vector<TerrainNode*> visibleNodes;
    
    void RecursiveUpdate(TerrainNode* node, const Camera& camera);
    void RecursiveBuildRenderItems(TerrainNode* node, std::vector<RenderItem>& renderItems);
};
```

## Step 5: Implement Frustum Culling
```cpp
// Frustum.h
class Frustum {
public:
    void ExtractFromMatrix(const XMMATRIX& viewProj);
    bool Intersects(const BoundingBox& bounds) const;
    
private:
    std::array<XMFLOAT4, 6> planes; // near, far, left, right, top, bottom
};
```

## Step 6: Create Water Rendering System
```cpp
// WaterRenderer.h
class WaterRenderer {
public:
    void Initialize(ID3D12Device* device, int gridSize, float tileSize);
    void Update(const GameTimer& gt, const Camera& camera);
    void Render(ID3D12GraphicsCommandList* cmdList);
    
private:
    std::unique_ptr<MeshGeometry> waterGeometry;
    ComPtr<ID3D12PipelineState> waterPSO;
    ComPtr<ID3D12RootSignature> waterRootSignature;
    float time = 0.0f;
    
    void GenerateWaterGeometry(int gridSize, float tileSize);
    void BuildWaterPSO(ID3D12Device* device);
};
```

## Step 7: Modify BaselineApp Class
Add these members to BaselineApp:
```cpp
class BaselineApp : public D3DApp {
private:
    // Terrain system
    std::unique_ptr<TerrainQuadTree> terrainQuadTree;
    std::unique_ptr<WaterRenderer> waterRenderer;
    Frustum cameraFrustum;
    
    // Camera control
    float cameraPitch = 0.0f;
    float cameraYaw = 0.0f;
    XMFLOAT3 cameraPosition = {0.0f, 10.0f, -10.0f};
    
    // New PSOs
    ComPtr<ID3D12PipelineState> mPSOs["terrain"];
    ComPtr<ID3D12PipelineState> mPSOs["water"];
    
    // New shaders
    ComPtr<ID3DBlob> mShaders["terrainVS"];
    ComPtr<ID3DBlob> mShaders["terrainPS"];
    ComPtr<ID3DBlob> mShaders["waterVS"];
    ComPtr<ID3DBlob> mShaders["waterPS"];
};
```

## Step 8: Implement Key Methods in BaselineApp

### Initialize Method:
```cpp
bool BaselineApp::Initialize() {
    // ... existing code ...
    
    // Initialize terrain and water systems
    terrainQuadTree = std::make_unique<TerrainQuadTree>();
    terrainQuadTree->Initialize(XMFLOAT2(0.0f, 0.0f), 1000.0f, 6);
    
    waterRenderer = std::make_unique<WaterRenderer>();
    waterRenderer->Initialize(md3dDevice.Get(), 100, 10.0f);
    
    BuildTerrainShadersAndInputLayout();
    BuildWaterShadersAndInputLayout();
    BuildTerrainPSO();
    BuildWaterPSO();
    
    // ... rest of initialization ...
}
```

### Update Method:
```cpp
void BaselineApp::Update(const GameTimer& gt) {
    // Update camera
    UpdateCamera(gt);
    
    // Extract frustum for culling
    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX viewProj = view * proj;
    cameraFrustum.ExtractFromMatrix(viewProj);
    
    // Update terrain LOD
    terrainQuadTree->Update(camera); // You'll need to create a Camera struct
    
    // Update water
    waterRenderer->Update(gt, camera);
    
    UpdateObjectCBs(gt);
    UpdatePassCB(gt);
}
```

### Draw Method:
```cpp
void BaselineApp::Draw(const GameTimer& gt) {
    // ... existing setup code ...
    
    // Render terrain
    mCommandList->SetPipelineState(mPSOs["terrain"].Get());
    terrainQuadTree->BuildRenderItems(mAllRitems);
    DrawRenderItems(mCommandList.Get());
    
    // Render water
    mCommandList->SetPipelineState(mPSOs["water"].Get());
    waterRenderer->Render(mCommandList.Get());
    
    // ... existing present code ...
}
```

## Step 9: Create Shader Files

### Terrain.hlsl:
```hlsl
struct VertexIn {
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut {
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

// Include height calculation and normal generation
```

### Water.hlsl:
```hlsl
// Implement Gerstner waves and water shading
float3 GerstnerWave(float3 position, float time) {
    // Wave calculations for realistic water
}
```

## Step 10: Camera Control Implementation
Add camera movement controls to handle input for flying over the terrain.

## Step 11: Memory Management
Implement proper memory management for terrain nodes:
- Use object pooling for terrain geometry
- Implement geometry streaming for large terrains
- Add cleanup for unused nodes

## Step 12: Optimization
- Implement asynchronous geometry generation
- Add distance-based LOD transitions
- Use compute shaders for height map generation
- Implement texture streaming for terrain materials

This system will create an infinite procedural terrain with adaptive LOD using quad-trees, efficient frustum culling, and realistic infinite water. The architecture is designed to be scalable and can handle large viewing distances while maintaining performance.

Based on the example BaselineApp program, here's the main code and algorithm parts for implementing a terrain rendering system with quad-tree LOD, frustum culling, and procedural water:

## 1. Core Data Structures

```cpp
// TerrainNode.h
#pragma once
#include "../../Common/d3dUtil.h"
#include "../../Common/MathHelper.h"
#include <array>
#include <memory>

struct BoundingBox {
    XMFLOAT3 min;
    XMFLOAT3 max;
    
    bool IntersectsFrustum(const std::array<XMFLOAT4, 6>& frustumPlanes) const;
};

class TerrainNode {
public:
    TerrainNode(XMFLOAT2 center, float size, int level);
    
    bool ShouldSplit(XMFLOAT3 cameraPos, float threshold);
    void Split();
    void Merge();
    void GenerateGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    
    XMFLOAT2 center;
    float size;
    int level;
    bool isLeaf = true;
    bool hasGeometry = false;
    
    std::array<std::unique_ptr<TerrainNode>, 4> children;
    BoundingBox bounds;
    std::unique_ptr<MeshGeometry> geometry;
    std::unique_ptr<RenderItem> renderItem;
    
private:
    void CalculateBounds();
    std::vector<Vertex> GenerateVertices();
    std::vector<uint16_t> GenerateIndices();
};
```

## 2. Height Map Generator

```cpp
// HeightMapGenerator.h
#pragma once
#include <vector>
#include <random>
#include "../../Common/MathHelper.h"

class HeightMapGenerator {
public:
    static float GetHeightAt(float x, float z);
    static XMFLOAT3 CalculateNormal(float x, float z, float heightScale = 1.0f);
    
private:
    static float PerlinNoise(float x, float y);
    static float Fade(float t);
    static float Lerp(float t, float a, float b);
    static float Grad(int hash, float x, float y);
    
    static const int PERMUTATION_SIZE = 256;
    static std::vector<int> permutation;
};

// HeightMapGenerator.cpp
std::vector<int> HeightMapGenerator::permutation = {
    // Perlin noise permutation table
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,
    // ... (full permutation table)
};

float HeightMapGenerator::GetHeightAt(float x, float z) {
    float noise = 0.0f;
    float amplitude = 1.0f;
    float frequency = 0.01f;
    
    // Fractal Brownian Motion
    for (int i = 0; i < 4; ++i) {
        noise += PerlinNoise(x * frequency, z * frequency) * amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    
    return noise * 50.0f; // Scale height
}

float HeightMapGenerator::PerlinNoise(float x, float y) {
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;
    
    x -= floor(x);
    y -= floor(y);
    
    float u = Fade(x);
    float v = Fade(y);
    
    int A = permutation[X] + Y;
    int AA = permutation[A & 255];
    int AB = permutation[(A + 1) & 255];
    int B = permutation[(X + 1) & 255] + Y;
    int BA = permutation[B & 255];
    int BB = permutation[(B + 1) & 255];
    
    return Lerp(v, Lerp(u, Grad(permutation[AA & 255], x, y),
                           Grad(permutation[BA & 255], x - 1, y)),
                   Lerp(u, Grad(permutation[AB & 255], x, y - 1),
                           Grad(permutation[BB & 255], x - 1, y - 1)));
}
```

## 3. Quad-Tree System

```cpp
// TerrainQuadTree.h
#pragma once
#include "TerrainNode.h"
#include <vector>

class TerrainQuadTree {
public:
    TerrainQuadTree();
    
    void Initialize(XMFLOAT2 center, float size, int maxLevels);
    void Update(XMFLOAT3 cameraPos, const std::array<XMFLOAT4, 6>& frustumPlanes);
    void GetVisibleRenderItems(std::vector<RenderItem*>& renderItems);
    void GenerateGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    
private:
    std::unique_ptr<TerrainNode> root;
    int maxLevels;
    std::vector<TerrainNode*> visibleNodes;
    
    void RecursiveUpdate(TerrainNode* node, XMFLOAT3 cameraPos, 
                        const std::array<XMFLOAT4, 6>& frustumPlanes);
    void CollectVisibleNodes(TerrainNode* node, const std::array<XMFLOAT4, 6>& frustumPlanes);
};

// TerrainQuadTree.cpp
void TerrainQuadTree::Update(XMFLOAT3 cameraPos, const std::array<XMFLOAT4, 6>& frustumPlanes) {
    visibleNodes.clear();
    if (root) {
        RecursiveUpdate(root.get(), cameraPos, frustumPlanes);
        CollectVisibleNodes(root.get(), frustumPlanes);
    }
}

void TerrainQuadTree::RecursiveUpdate(TerrainNode* node, XMFLOAT3 cameraPos, 
                                     const std::array<XMFLOAT4, 6>& frustumPlanes) {
    if (!node) return;
    
    // Check if node should split based on distance to camera
    float distance = sqrtf((node->center.x - cameraPos.x) * (node->center.x - cameraPos.x) +
                          (node->center.y - cameraPos.z) * (node->center.y - cameraPos.z));
    
    float threshold = node->size * 2.0f; // LOD threshold
    
    if (node->isLeaf && distance < threshold && node->level < maxLevels) {
        node->Split();
    }
    else if (!node->isLeaf && distance > threshold * 2.0f) {
        node->Merge();
    }
    
    if (!node->isLeaf) {
        for (auto& child : node->children) {
            RecursiveUpdate(child.get(), cameraPos, frustumPlanes);
        }
    }
}
```

## 4. Frustum Culling System

```cpp
// Frustum.h
#pragma once
#include "../../Common/MathHelper.h"
#include <array>

class Frustum {
public:
    void ExtractFromMatrix(const XMMATRIX& viewProj);
    std::array<XMFLOAT4, 6> GetPlanes() const { return planes; }
    
private:
    std::array<XMFLOAT4, 6> planes; // Left, Right, Top, Bottom, Near, Far
};

// Frustum.cpp
void Frustum::ExtractFromMatrix(const XMMATRIX& viewProj) {
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, viewProj);
    
    // Left plane
    planes[0].x = m._14 + m._11;
    planes[0].y = m._24 + m._21;
    planes[0].z = m._34 + m._31;
    planes[0].w = m._44 + m._41;
    
    // Right plane
    planes[1].x = m._14 - m._11;
    planes[1].y = m._24 - m._21;
    planes[1].z = m._34 - m._31;
    planes[1].w = m._44 - m._41;
    
    // Top plane
    planes[2].x = m._14 - m._12;
    planes[2].y = m._24 - m._22;
    planes[2].z = m._34 - m._32;
    planes[2].w = m._44 - m._42;
    
    // Bottom plane
    planes[3].x = m._14 + m._12;
    planes[3].y = m._24 + m._22;
    planes[3].z = m._34 + m._32;
    planes[3].w = m._44 + m._42;
    
    // Near plane
    planes[4].x = m._13;
    planes[4].y = m._23;
    planes[4].z = m._33;
    planes[4].w = m._43;
    
    // Far plane
    planes[5].x = m._14 - m._13;
    planes[5].y = m._24 - m._23;
    planes[5].z = m._34 - m._33;
    planes[5].w = m._44 - m._43;
    
    // Normalize planes
    for (auto& plane : planes) {
        float length = sqrtf(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        plane.x /= length;
        plane.y /= length;
        plane.z /= length;
        plane.w /= length;
    }
}
```

## 5. Water Renderer

```cpp
// WaterRenderer.h
#pragma once
#include "../../Common/d3dUtil.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"

class WaterRenderer {
public:
    WaterRenderer();
    
    void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, 
                   ComPtr<ID3D12RootSignature> rootSignature);
    void Update(const GameTimer& gt, XMFLOAT3 cameraPos);
    RenderItem* GetRenderItem() { return waterRenderItem.get(); }
    
private:
    std::unique_ptr<MeshGeometry> waterGeometry;
    std::unique_ptr<RenderItem> waterRenderItem;
    
    void BuildWaterGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
};

// WaterRenderer.cpp
void WaterRenderer::BuildWaterGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList) {
    const int gridSize = 100;
    const float tileSize = 10.0f;
    
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    
    // Generate water grid
    for (int z = 0; z <= gridSize; ++z) {
        for (int x = 0; x <= gridSize; ++x) {
            float worldX = (x - gridSize * 0.5f) * tileSize;
            float worldZ = (z - gridSize * 0.5f) * tileSize;
            
            Vertex vertex;
            vertex.Pos = XMFLOAT3(worldX, 0.0f, worldZ);
            vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
            vertex.TexC = XMFLOAT2((float)x / gridSize, (float)z / gridSize);
            vertex.Color = XMFLOAT4(0.0f, 0.3f, 0.8f, 0.7f); // Blue water color
            
            vertices.push_back(vertex);
        }
    }
    
    // Generate indices
    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            int topLeft = z * (gridSize + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * (gridSize + 1) + x;
            int bottomRight = bottomLeft + 1;
            
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
    
    // Create geometry buffers (similar to pyramid geometry creation)
    // ... buffer creation code similar to BaselineApp::BuildPyramidGeometry
}
```

## 6. Modified BaselineApp

```cpp
// Modified BaselineApp.h
class BaselineApp : public D3DApp {
public:
    // ... existing code ...
    
private:
    // Terrain system
    std::unique_ptr<TerrainQuadTree> terrainQuadTree;
    std::unique_ptr<WaterRenderer> waterRenderer;
    Frustum cameraFrustum;
    
    // Camera
    XMFLOAT3 cameraPosition = {0.0f, 50.0f, -50.0f};
    float cameraPitch = 0.0f;
    float cameraYaw = 0.0f;
    
    // Modified methods
    void BuildTerrainGeometry();
    void BuildWaterGeometry();
    void BuildTerrainPSO();
    void BuildWaterPSO();
    void UpdateCamera(const GameTimer& gt);
    
    // ... existing members ...
};

// Modified BaselineApp.cpp Update method
void BaselineApp::Update(const GameTimer& gt) {
    // Update camera
    UpdateCamera(gt);
    
    // Update view matrix
    XMVECTOR pos = XMVectorSet(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f);
    XMVECTOR target = XMVectorSet(
        cameraPosition.x + cosf(cameraYaw) * cosf(cameraPitch),
        cameraPosition.y + sinf(cameraPitch),
        cameraPosition.z + sinf(cameraYaw) * cosf(cameraPitch),
        1.0f
    );
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);
    
    // Extract frustum
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX viewProj = view * proj;
    cameraFrustum.ExtractFromMatrix(viewProj);
    
    // Update terrain LOD
    terrainQuadTree->Update(cameraPosition, cameraFrustum.GetPlanes());
    
    // Update water
    waterRenderer->Update(gt, cameraPosition);
    
    // Frame resource management (existing code)
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
    
    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
    
    UpdateObjectCBs(gt);
    UpdatePassCB(gt);
}

// Modified Draw method
void BaselineApp::Draw(const GameTimer& gt) {
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
    
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["terrain"].Get()));
    
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);
    
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSkyBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), 
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
    
    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());
    
    // Draw terrain
    std::vector<RenderItem*> terrainItems;
    terrainQuadTree->GetVisibleRenderItems(terrainItems);
    DrawTerrainItems(mCommandList.Get(), terrainItems);
    
    // Draw water
    mCommandList->SetPipelineState(mPSOs["water"].Get());
    DrawWaterItems(mCommandList.Get());
    
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

## 7. Shader Files

### Terrain.hlsl
```hlsl
cbuffer cbPerObject : register(b0) {
    float4x4 gWorld;
    float4x4 gTexTransform;
};

cbuffer cbPass : register(b1) {
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};

struct VertexIn {
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float4 Color : COLOR;
};

struct VertexOut {
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin) {
    VertexOut vout = (VertexOut)0.0f;
    
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);
    
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;
    vout.Color = vin.Color;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target {
    // Simple terrain shading
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.5f));
    float3 normalW = normalize(pin.NormalW);
    float ndotl = max(dot(normalW, -lightDir), 0.1f);
    
    float4 color = pin.Color * ndotl;
    
    // Height-based coloring
    float height = pin.PosW.y;
    if (height < 0) color = float4(0.8f, 0.7f, 0.5f, 1.0f); // Sand
    else if (height < 20) color = float4(0.2f, 0.8f, 0.2f, 1.0f); // Grass
    else if (height < 40) color = float4(0.5f, 0.5f, 0.5f, 1.0f); // Rock
    else color = float4(1.0f, 1.0f, 1.0f, 1.0f); // Snow
    
    return color * ndotl;
}
```

### Water.hlsl
```hlsl
// Same cbuffers as terrain...

VertexOut VS(VertexIn vin) {
    VertexOut vout = (VertexOut)0.0f;
    
    // Apply wave animation
    float3 pos = vin.PosL;
    pos.y = sin(pos.x * 0.1f + gTotalTime * 2.0f) * 0.5f + 
            cos(pos.z * 0.15f + gTotalTime * 1.5f) * 0.3f;
    
    float4 posW = mul(float4(pos, 1.0f), gWorld);
    