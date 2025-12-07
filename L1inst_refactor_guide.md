# Terrain Renderer Refactoring Guide

> **Purpose**: Track added functionality for extraction into separate files  
> **Status**: Reference document for future refactoring  
> **Related**: [[L1inst_plan.md]]

This document tracks all functionality added to `Baseline.cpp` during terrain renderer implementation. Use this guide to extract code into separate `.h` and `.cpp` files when ready.

---

## Overview

The terrain renderer implementation adds the following components to `Baseline.cpp`:

1. **Frustum Culling System** - `Frustum` class
2. **Height Map Generator** - `HeightMapGenerator` class
3. **Terrain Tile** - `TerrainTile` class
4. **Quad-Tree LOD System** - `QuadTreeNode` and `QuadTree` classes
5. **Water Renderer** - `WaterRenderer` class
6. **Vertex Structures** - `TerrainVertex`, `WaterVertex`
7. **Integration Methods** - Various build/update/draw methods

---

## File Structure Plan

After refactoring, the recommended file structure:

```
baseline/src/
├── Baseline.cpp                    (Main application, minimal terrain code)
├── Baseline.h                      (Main class declaration)
├── Terrain/
│   ├── Frustum.h
│   ├── Frustum.cpp
│   ├── HeightMapGenerator.h
│   ├── HeightMapGenerator.cpp
│   ├── TerrainTile.h
│   ├── TerrainTile.cpp
│   ├── QuadTreeNode.h
│   ├── QuadTreeNode.cpp
│   ├── QuadTree.h
│   ├── QuadTree.cpp
│   ├── WaterRenderer.h
│   └── WaterRenderer.cpp
└── Shaders/
    ├── Terrain.hlsl
    └── Water.hlsl
```

---

## Component Extraction Map

### 1. Frustum Culling System

**Current Location**: Nested class in `BaselineApp` (Baseline.cpp)

**Extract To**:
- `Terrain/Frustum.h`
- `Terrain/Frustum.cpp`

**Components to Extract**:
```cpp
// From BaselineApp class (nested class)
class Frustum
{
    // All methods and members
};

// From BaselineApp (member)
Frustum mFrustum;
```

**Dependencies**:
- `DirectXMath.h`
- `DirectXCollision.h`

**Forward Declarations Needed**: None

**Public Interface**:
- `Update(const DirectX::XMMATRIX& viewProj)`
- `Intersects(const DirectX::XMFLOAT3& center, float radius)`
- `Intersects(const DirectX::BoundingBox& boundingBox)`
- `ContainsPoint(const DirectX::XMFLOAT3& point)`

---

### 2. Height Map Generator

**Current Location**: Nested class in `BaselineApp` (Baseline.cpp)

**Extract To**:
- `Terrain/HeightMapGenerator.h`
- `Terrain/HeightMapGenerator.cpp`

**Components to Extract**:
```cpp
// From BaselineApp class (nested class)
class HeightMapGenerator
{
    // All methods and members
};

// From BaselineApp (member - if kept)
std::unique_ptr<HeightMapGenerator> mHeightMapGenerator;
```

**Dependencies**:
- `DirectXMath.h`
- `<vector>`
- `<random>` (for hash function)
- `<cmath>`

**Forward Declarations Needed**: None

**Public Interface**:
- Constructor: `HeightMapGenerator(int width, int height, float scale)`
- `GenerateHeightMap(std::vector<float>& heights, const DirectX::XMFLOAT2& offset)`
- `SetOctaves(int octaves)`
- `SetFrequency(float frequency)`
- `SetAmplitude(float amplitude)`
- `SetSeed(unsigned int seed)`
- `GetWidth()`, `GetHeight()`, `GetScale()`

**Private Methods**:
- `PerlinNoise2D(float x, float y)`
- `Interpolate(float a, float b, float x)`
- `Fade(float t)`

---

### 3. Terrain Tile

**Current Location**: Nested class in `BaselineApp` (Baseline.cpp)

**Extract To**:
- `Terrain/TerrainTile.h`
- `Terrain/TerrainTile.cpp`

**Components to Extract**:
```cpp
// From BaselineApp class (nested class)
class TerrainTile
{
    // All methods and members
};

// Vertex structure (can be in header or separate)
struct TerrainVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexCoord;
};
```

**Dependencies**:
- `d3d12.h`
- `d3dUtil.h` (for `CreateDefaultBuffer`, `ThrowIfFailed`)
- `DirectXMath.h`
- `DirectXCollision.h`
- `<vector>`
- `<wrl.h>` (for ComPtr)

**Forward Declarations Needed**: None

**Public Interface**:
- Constructor: `TerrainTile()`
- `Initialize(ID3D12Device*, ID3D12GraphicsCommandList*, const std::vector<float>&, int, float, const DirectX::XMFLOAT3&)`
- `Render(ID3D12GraphicsCommandList*, ID3D12Resource*, UINT, int, UINT)`
- `VertexBufferView()`, `IndexBufferView()`
- `GetBoundingBox()`, `GetIndexCount()`

**Private Methods**:
- `CalculateNormals(...)`
- `CreateMesh(...)`

**Member Variables**:
- Vertex/Index buffers (ComPtr)
- Buffer views
- Bounding box
- Counts and sizes

---

### 4. Quad-Tree Node

**Current Location**: Nested class in `BaselineApp` (Baseline.cpp)

**Extract To**:
- `Terrain/QuadTreeNode.h`
- `Terrain/QuadTreeNode.cpp`

**Components to Extract**:
```cpp
// From BaselineApp class (nested class)
class QuadTreeNode
{
    // All methods and members
};
```

**Dependencies**:
- `TerrainTile.h` (forward declare or include)
- `QuadTree.h` (forward declare)
- `DirectXMath.h`
- `DirectXCollision.h`
- `<vector>`
- `<memory>`

**Forward Declarations Needed**:
```cpp
class QuadTree;  // Forward declaration
class TerrainTile;  // Forward declaration
```

**Public Interface**:
- Constructor: `QuadTreeNode(BaselineApp*, const DirectX::XMFLOAT3&, float, int)`
- `Update(const DirectX::XMFLOAT3&, const Frustum&)`
- `Render(ID3D12GraphicsCommandList*, ID3D12Resource*, UINT, UINT)`
- `Subdivide()`, `Merge()`
- `GetCenter()`, `GetHalfSize()`, `GetDepth()`, `IsLeaf()`
- `NeedsSubdivision(...)`, `ShouldMerge(...)`

**Private Methods**:
- `BuildTileGeometry()`

**Member Variables**:
- `BaselineApp* mApp` (consider removing dependency)
- Center, half size, depth
- Children vector
- Terrain tile
- Bounding box
- Visibility flag

**Refactoring Notes**:
- `mApp` dependency should be replaced with function parameters or a context struct
- Consider passing required services (device, command list, height generator) instead of full app pointer

---

### 5. Quad-Tree

**Current Location**: Nested class in `BaselineApp` (Baseline.cpp)

**Extract To**:
- `Terrain/QuadTree.h`
- `Terrain/QuadTree.cpp`

**Components to Extract**:
```cpp
// From BaselineApp class (nested class)
class QuadTree
{
    // All methods and members
};

// From BaselineApp (member)
std::unique_ptr<QuadTree> mTerrainQuadTree;
```

**Dependencies**:
- `QuadTreeNode.h`
- `HeightMapGenerator.h`
- `DirectXMath.h`
- `<memory>`

**Forward Declarations Needed**:
```cpp
class BaselineApp;  // Forward declaration (or remove dependency)
```

**Public Interface**:
- Constructor: `QuadTree(BaselineApp*, float, int, int)`
- `Update(const DirectX::XMFLOAT3&, const Frustum&)`
- `Render(ID3D12GraphicsCommandList*, ID3D12Resource*, UINT, UINT)`
- `GetHeightMapGenerator()`
- `GetMaxDepth()`, `GetLODDistanceFactor()`, `GetTileResolution()`
- `SetLODDistanceFactor(float)`

**Member Variables**:
- `BaselineApp* mApp` (consider removing)
- Root node
- Height map generator
- Terrain parameters

**Refactoring Notes**:
- Remove `BaselineApp*` dependency
- Pass device, command list, and other services as parameters or through a context struct

---

### 6. Water Renderer

**Current Location**: Nested class in `BaselineApp` (Baseline.cpp)

**Extract To**:
- `Terrain/WaterRenderer.h`
- `Terrain/WaterRenderer.cpp`

**Components to Extract**:
```cpp
// From BaselineApp class (nested class)
class WaterRenderer
{
    // All methods and members
};

// Vertex structure
struct WaterVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT2 TexCoord;
};

// From BaselineApp (member)
std::unique_ptr<WaterRenderer> mWaterRenderer;
```

**Dependencies**:
- `d3d12.h`
- `d3dUtil.h`
- `DirectXMath.h`
- `<vector>`
- `<wrl.h>`

**Forward Declarations Needed**:
```cpp
class BaselineApp;  // Forward declaration (or remove dependency)
```

**Public Interface**:
- Constructor: `WaterRenderer(BaselineApp*, float, float)`
- `Update(const DirectX::XMFLOAT3&, float)`
- `Render(ID3D12GraphicsCommandList*, ID3D12Resource*, UINT)`
- `SetWaveSpeed(float)`, `SetWaveHeight(float)`, `SetWaterColor(...)`
- `GetWaveSpeed()`, `GetWaveHeight()`

**Private Methods**:
- `BuildWaterGeometry()`
- `BuildWaterPSO()`
- `UpdateWaveBuffer(float)`

**Member Variables**:
- `BaselineApp* mApp` (consider removing)
- Root signature, PSO
- Shader blobs
- Vertex/Index buffers
- Wave constant buffer
- Buffer views
- Grid parameters
- Wave parameters

**Refactoring Notes**:
- Remove `BaselineApp*` dependency
- Pass device, command list, and format information as parameters
- Consider extracting PSO creation to a separate method or factory

---

### 7. Vertex Structures

**Current Location**: Top of Baseline.cpp (before class definition)

**Extract To**:
- `Terrain/TerrainTypes.h` (or include in respective headers)

**Components to Extract**:
```cpp
struct TerrainVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexCoord;
};

struct WaterVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT2 TexCoord;
};
```

**Dependencies**:
- `DirectXMath.h`

---

### 8. Integration Methods

**Current Location**: Methods in `BaselineApp` class (Baseline.cpp)

**Keep in Baseline.cpp** (or move to `Baseline.h`/`Baseline.cpp`):

```cpp
// In BaselineApp class
void BuildTerrain();
void BuildWater();
void UpdateTerrain(const GameTimer& gt);
void UpdateWater(const GameTimer& gt);
void DrawTerrain(ID3D12GraphicsCommandList* cmdList);
void DrawWater(ID3D12GraphicsCommandList* cmdList);
```

**Modified Methods** (keep in Baseline.cpp):
- `Initialize()` - Add terrain/water initialization
- `Update()` - Add frustum update, terrain/water updates
- `Draw()` - Add terrain/water rendering
- `UpdatePassCB()` - Extended for terrain
- `BuildShadersAndInputLayout()` - Add terrain shaders
- `BuildPSOs()` - Add terrain PSO
- `OnKeyPressed()` - Add terrain/water controls

**Member Variables** (keep in BaselineApp):
```cpp
// Terrain system
std::unique_ptr<QuadTree> mTerrainQuadTree;
std::unique_ptr<WaterRenderer> mWaterRenderer;
Frustum mFrustum;

// Terrain parameters
float mTerrainSize = 1000.0f;
int mMaxQuadTreeDepth = 6;
float mLODDistanceFactor = 1.0f;
int mTileResolution = 65;
```

---

## Refactoring Steps

### Step 1: Create Directory Structure
```bash
mkdir baseline/src/Terrain
```

### Step 2: Extract Frustum
1. Create `Terrain/Frustum.h` with class declaration
2. Create `Terrain/Frustum.cpp` with implementation
3. Update `Baseline.cpp` to include `Frustum.h` instead of nested class
4. Remove nested class definition from `BaselineApp`

### Step 3: Extract HeightMapGenerator
1. Create `Terrain/HeightMapGenerator.h` with class declaration
2. Create `Terrain/HeightMapGenerator.cpp` with implementation
3. Update `Baseline.cpp` to include header
4. Remove nested class definition

### Step 4: Extract TerrainTile
1. Create `Terrain/TerrainTile.h` with class and vertex structure
2. Create `Terrain/TerrainTile.cpp` with implementation
3. Update `Baseline.cpp` to include header
4. Remove nested class definition

### Step 5: Extract QuadTreeNode
1. Create `Terrain/QuadTreeNode.h` with forward declarations
2. Create `Terrain/QuadTreeNode.cpp` with implementation
3. Update includes and forward declarations
4. Remove `BaselineApp*` dependency (pass required services)

### Step 6: Extract QuadTree
1. Create `Terrain/QuadTree.h` with forward declarations
2. Create `Terrain/QuadTree.cpp` with implementation
3. Update includes
4. Remove `BaselineApp*` dependency

### Step 7: Extract WaterRenderer
1. Create `Terrain/WaterRenderer.h` with class and vertex structure
2. Create `Terrain/WaterRenderer.cpp` with implementation
3. Update `Baseline.cpp` to include header
4. Remove `BaselineApp*` dependency

### Step 8: Update Baseline.cpp
1. Add includes for all extracted headers
2. Remove all nested class definitions
3. Keep integration methods in `BaselineApp`
4. Update method implementations to use extracted classes

### Step 9: Update Project Files
1. Add new `.cpp` files to project
2. Update include paths if needed
3. Rebuild and test

---

## Dependency Resolution

### Circular Dependency Prevention

**Issue**: `QuadTreeNode` needs `QuadTree`, `QuadTree` needs `QuadTreeNode`

**Solution**: Use forward declarations in headers, include full definitions in `.cpp` files

```cpp
// QuadTreeNode.h
class QuadTree;  // Forward declaration
class TerrainTile;  // Forward declaration

class QuadTreeNode {
    // Use pointers/references only
};

// QuadTreeNode.cpp
#include "QuadTree.h"
#include "TerrainTile.h"
// Full implementations
```

### BaselineApp Dependency Removal

**Current**: Many classes hold `BaselineApp* mApp`

**Refactored**: Pass required services through constructor or methods

```cpp
// Before
class QuadTreeNode {
    BaselineApp* mApp;
    void BuildTileGeometry() {
        mApp->md3dDevice.Get();
    }
};

// After
class QuadTreeNode {
    ID3D12Device* mDevice;
    ID3D12GraphicsCommandList* mCmdList;
    HeightMapGenerator* mHeightGen;
    
    void BuildTileGeometry() {
        mDevice->...
    }
};
```

---

## Testing After Refactoring

1. **Compilation**: Ensure all files compile without errors
2. **Linking**: Verify all symbols resolve correctly
3. **Runtime**: Test terrain rendering, LOD, water animation
4. **Performance**: Compare performance before/after refactoring
5. **Memory**: Check for memory leaks or issues

---

## Code Organization Best Practices

### Header Files
- Include only necessary headers
- Use forward declarations when possible
- Group includes: system, DirectX, project
- Use include guards or `#pragma once`

### Implementation Files
- Include corresponding header first
- Include other project headers
- Include system headers last

### Naming Conventions
- Classes: `PascalCase`
- Methods: `PascalCase`
- Members: `mCamelCase`
- Constants: `kConstantName` or `gConstantName`

---

## Summary

This guide provides a complete map for extracting terrain renderer functionality from `Baseline.cpp` into a well-organized file structure. Follow the steps sequentially, test after each extraction, and resolve dependencies carefully to avoid circular references.

**Key Principles**:
1. Extract one component at a time
2. Test after each extraction
3. Remove dependencies on `BaselineApp` where possible
4. Use forward declarations to break circular dependencies
5. Keep integration code in `BaselineApp`

---

**End of Refactoring Guide**

