# Пошаговый план реализации ландшафта с LOD, Frustum Culling и водной поверхностью

## Этап 1: Подготовка базовой структуры проекта

### Шаг 1.1: Создание необходимых файлов и классов
```
// В корневой директории проекта создайте следующие файлы:
- QuadTreeNode.h/cpp           // Класс узла квадро-дерева
- QuadTree.h/cpp               // Класс квадро-дерева
- TerrainTile.h/cpp            // Класс тайла ландшафта
- HeightMapGenerator.h/cpp     // Генератор карт высот
- WaterRenderer.h/cpp          // Рендеринг водной поверхности
- Frustum.h/cpp                // Класс для frustum culling
- TerrainConstants.h           // Константы для шейдеров ландшафта
```

### Шаг 1.2: Добавление новых заголовков в BaselineApp.h
```cpp
// Добавьте в BaselineApp.h следующие include:
#include "QuadTree.h"
#include "TerrainTile.h"
#include "HeightMapGenerator.h"
#include "WaterRenderer.h"
#include "Frustum.h"
```

### Шаг 1.3: Объявление новых членов класса в BaselineApp.h
```cpp
// Внутри класса BaselineApp добавьте:
private:
    // Система ландшафта
    std::unique_ptr<QuadTree> mTerrainQuadTree;
    std::unique_ptr<HeightMapGenerator> mHeightMapGenerator;
    std::unique_ptr<WaterRenderer> mWaterRenderer;
    
    // Frustum culling
    Frustum mFrustum;
    
    // Параметры ландшафта
    float mTerrainSize = 1000.0f;  // Размер ландшафта в метрах
    int mMaxQuadTreeDepth = 6;     // Максимальная глубина квадро-дерева
    float mLODDistanceFactor = 1.0f; // Коэффициент для расчета LOD
```

## Этап 2: Реализация Frustum Culling

### Шаг 2.1: Создание класса Frustum
```cpp
// Frustum.h
#pragma once
#include <DirectXMath.h>

class Frustum
{
public:
    Frustum() = default;
    ~Frustum() = default;

    void Update(const DirectX::XMMATRIX& viewProj);
    
    // Проверка пересечения с AABB
    bool Intersects(const DirectX::XMFLOAT3& center, float radius) const;
    bool Intersects(const DirectX::BoundingBox& boundingBox) const;
    
    // Проверка точки
    bool ContainsPoint(const DirectX::XMFLOAT3& point) const;

private:
    struct Plane
    {
        DirectX::XMVECTOR normal;
        float distance;
    };
    
    Plane mPlanes[6]; // Near, Far, Left, Right, Top, Bottom
};
```

### Шаг 2.2: Реализация методов Frustum
```cpp
// Frustum.cpp
#include "Frustum.h"
#include <DirectXCollision.h>

void Frustum::Update(const DirectX::XMMATRIX& viewProj)
{
    DirectX::XMFLOAT4X4 viewProjMat;
    DirectX::XMStoreFloat4x4(&viewProjMat, viewProj);
    
    // Извлечение плоскостей из матрицы view-projection
    // Left plane
    mPlanes[0].normal = DirectX::XMVectorSet(
        viewProjMat._14 + viewProjMat._11,
        viewProjMat._24 + viewProjMat._21,
        viewProjMat._34 + viewProjMat._31,
        0.0f);
    mPlanes[0].distance = viewProjMat._44 + viewProjMat._41;
    
    // Right plane
    mPlanes[1].normal = DirectX::XMVectorSet(
        viewProjMat._14 - viewProjMat._11,
        viewProjMat._24 - viewProjMat._21,
        viewProjMat._34 - viewProjMat._31,
        0.0f);
    mPlanes[1].distance = viewProjMat._44 - viewProjMat._41;
    
    // Bottom plane
    mPlanes[2].normal = DirectX::XMVectorSet(
        viewProjMat._14 + viewProjMat._12,
        viewProjMat._24 + viewProjMat._22,
        viewProjMat._34 + viewProjMat._32,
        0.0f);
    mPlanes[2].distance = viewProjMat._44 + viewProjMat._42;
    
    // Top plane
    mPlanes[3].normal = DirectX::XMVectorSet(
        viewProjMat._14 - viewProjMat._12,
        viewProjMat._24 - viewProjMat._22,
        viewProjMat._34 - viewProjMat._32,
        0.0f);
    mPlanes[3].distance = viewProjMat._44 - viewProjMat._42;
    
    // Near plane
    mPlanes[4].normal = DirectX::XMVectorSet(
        viewProjMat._13,
        viewProjMat._23,
        viewProjMat._33,
        0.0f);
    mPlanes[4].distance = viewProjMat._43;
    
    // Far plane
    mPlanes[5].normal = DirectX::XMVectorSet(
        viewProjMat._14 - viewProjMat._13,
        viewProjMat._24 - viewProjMat._23,
        viewProjMat._34 - viewProjMat._33,
        0.0f);
    mPlanes[5].distance = viewProjMat._44 - viewProjMat._43;
    
    // Нормализация плоскостей
    for (int i = 0; i < 6; ++i)
    {
        float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(mPlanes[i].normal));
        mPlanes[i].normal = DirectX::XMVectorScale(mPlanes[i].normal, 1.0f / length);
        mPlanes[i].distance /= length;
    }
}

bool Frustum::Intersects(const DirectX::XMFLOAT3& center, float radius) const
{
    DirectX::XMVECTOR centerVec = DirectX::XMLoadFloat3(&center);
    
    for (int i = 0; i < 6; ++i)
    {
        float distance = DirectX::XMVectorGetX(
            DirectX::XMVector3Dot(mPlanes[i].normal, centerVec)) + mPlanes[i].distance;
        
        if (distance < -radius)
            return false;
    }
    
    return true;
}

bool Frustum::Intersects(const DirectX::BoundingBox& boundingBox) const
{
    for (int i = 0; i < 6; ++i)
    {
        // Проверка всех 8 вершин AABB на принадлежность к отрицательной стороне плоскости
        bool allOutside = true;
        
        for (int x = 0; x < 2; ++x)
        {
            for (int y = 0; y < 2; ++y)
            {
                for (int z = 0; z < 2; ++z)
                {
                    DirectX::XMFLOAT3 point;
                    point.x = x == 0 ? boundingBox.Center.x - boundingBox.Extents.x : boundingBox.Center.x + boundingBox.Extents.x;
                    point.y = y == 0 ? boundingBox.Center.y - boundingBox.Extents.y : boundingBox.Center.y + boundingBox.Extents.y;
                    point.z = z == 0 ? boundingBox.Center.z - boundingBox.Extents.z : boundingBox.Center.z + boundingBox.Extents.z;
                    
                    DirectX::XMVECTOR pointVec = DirectX::XMLoadFloat3(&point);
                    float distance = DirectX::XMVectorGetX(
                        DirectX::XMVector3Dot(mPlanes[i].normal, pointVec)) + mPlanes[i].distance;
                    
                    if (distance >= 0.0f)
                    {
                        allOutside = false;
                        break;
                    }
                }
                if (!allOutside) break;
            }
            if (!allOutside) break;
        }
        
        if (allOutside)
            return false;
    }
    
    return true;
}
```

## Этап 3: Реализация системы карт высот

### Шаг 3.1: Создание генератора карт высот
```cpp
// HeightMapGenerator.h
#pragma once
#include <vector>
#include <DirectXMath.h>

class HeightMapGenerator
{
public:
    HeightMapGenerator(int width, int height, float scale = 1.0f);
    ~HeightMapGenerator() = default;
    
    void GenerateHeightMap(std::vector<float>& heights, const DirectX::XMFLOAT2& offset = {0.0f, 0.0f});
    
    // Настройки генерации
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

### Шаг 3.2: Реализация генератора с использованием Perlin шума
```cpp
// HeightMapGenerator.cpp
#include "HeightMapGenerator.h"
#include <random>
#include <algorithm>
#include <cmath>

HeightMapGenerator::HeightMapGenerator(int width, int height, float scale)
    : mWidth(width), mHeight(height), mScale(scale),
      mOctaves(4), mFrequency(0.01f), mAmplitude(10.0f), mSeed(12345)
{
}

void HeightMapGenerator::GenerateHeightMap(std::vector<float>& heights, const DirectX::XMFLOAT2& offset)
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
            
            // Суммирование октав для получения фрактального шума
            for (int o = 0; o < mOctaves; ++o)
            {
                height += PerlinNoise2D(worldX * frequency, worldY * frequency) * amplitude;
                amplitude *= 0.5f;
                frequency *= 2.0f;
            }
            
            // Нормализация высоты
            height = std::max(0.0f, height);
            heights[y * mWidth + x] = height;
        }
    }
}

// Перлин шум (упрощенная реализация)
float HeightMapGenerator::PerlinNoise2D(float x, float y)
{
    // Используем std::hash для генерации псевдослучайных значений
    auto hash = [](float x, float y, unsigned int seed) {
        return std::hash<int>()(static_cast<int>(x) * 12345 + static_cast<int>(y) * 67890 + seed) % 10000;
    };
    
    // Целочисленные координаты
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    
    // Дробные части
    float sx = x - x0;
    float sy = y - y0;
    
    // Градиентные векторы для 4 углов
    float n0 = hash(x0, y0, mSeed) / 10000.0f;
    float n1 = hash(x1, y0, mSeed) / 10000.0f;
    float n2 = hash(x0, y1, mSeed) / 10000.0f;
    float n3 = hash(x1, y1, mSeed) / 10000.0f;
    
    // Интерполяция
    float i1 = Interpolate(n0, n1, sx);
    float i2 = Interpolate(n2, n3, sx);
    float result = Interpolate(i1, i2, sy);
    
    return result * 2.0f - 1.0f; // Нормализация в [-1, 1]
}

float HeightMapGenerator::Interpolate(float a, float b, float x)
{
    float f = Fade(x);
    return a * (1.0f - f) + b * f;
}

float HeightMapGenerator::Fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}
```

## Этап 4: Реализация квадро-дерева для LOD

### Шаг 4.1: Создание класса QuadTreeNode
```cpp
// QuadTreeNode.h
#pragma once
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include "TerrainTile.h"

class QuadTree;

class QuadTreeNode
{
public:
    QuadTreeNode(QuadTree* quadTree, const DirectX::XMFLOAT3& center, float halfSize, int depth);
    ~QuadTreeNode();
    
    void Update(const DirectX::XMFLOAT3& cameraPosition, const Frustum& frustum);
    void Render(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* objectCB, UINT objCBByteSize, UINT passCBIndex);
    
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
    
    QuadTree* mQuadTree;
    DirectX::XMFLOAT3 mCenter;
    float mHalfSize;
    int mDepth;
    
    std::vector<std::unique_ptr<QuadTreeNode>> mChildren;
    std::unique_ptr<TerrainTile> mTile;
    
    // Параметры LOD
    float mMaxScreenError = 5.0f; // Максимальная ошибка в пикселях
    bool mIsVisible = false;
    DirectX::BoundingBox mBoundingBox;
};
```

### Шаг 4.2: Реализация класса QuadTreeNode
```cpp
// QuadTreeNode.cpp
#include "QuadTreeNode.h"
#include "QuadTree.h"
#include "TerrainConstants.h"
#include <DirectXCollision.h>

QuadTreeNode::QuadTreeNode(QuadTree* quadTree, const DirectX::XMFLOAT3& center, float halfSize, int depth)
    : mQuadTree(quadTree), mCenter(center), mHalfSize(halfSize), mDepth(depth)
{
    // Вычисление bounding box для узла
    DirectX::XMVECTOR minCorner = DirectX::XMVectorSet(
        center.x - halfSize, 0.0f, center.z - halfSize, 1.0f);
    DirectX::XMVECTOR maxCorner = DirectX::XMVectorSet(
        center.x + halfSize, 100.0f, center.z + halfSize, 1.0f); // 100.0f - максимальная высота
    
    DirectX::BoundingBox::CreateFromPoints(mBoundingBox, minCorner, maxCorner);
    
    // Создание геометрии для текущего узла
    BuildTileGeometry();
}

QuadTreeNode::~QuadTreeNode()
{
}

void QuadTreeNode::Update(const DirectX::XMFLOAT3& cameraPosition, const Frustum& frustum)
{
    // Проверка видимости с помощью frustum culling
    mIsVisible = frustum.Intersects(mBoundingBox);
    
    if (!mIsVisible)
        return;
    
    // Проверка необходимости подразделения
    if (mDepth < mQuadTree->GetMaxDepth() && NeedsSubdivision(cameraPosition))
    {
        if (mChildren.empty())
        {
            Subdivide();
        }
        
        // Рекурсивное обновление детей
        for (auto& child : mChildren)
        {
            child->Update(cameraPosition, frustum);
        }
    }
    else
    {
        // Проверка необходимости объединения
        if (!mChildren.empty() && ShouldMerge(cameraPosition))
        {
            Merge();
        }
    }
}

void QuadTreeNode::Render(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* objectCB, UINT objCBByteSize, UINT passCBIndex)
{
    if (!mIsVisible)
        return;
    
    if (!mChildren.empty())
    {
        // Рендеринг детей вместо себя
        for (auto& child : mChildren)
        {
            child->Render(cmdList, objectCB, objCBByteSize, passCBIndex);
        }
    }
    else if (mTile)
    {
        // Рендеринг текущего тайла
        mTile->Render(cmdList, objectCB, objCBByteSize, mDepth, passCBIndex);
    }
}

void QuadTreeNode::Subdivide()
{
    if (!mChildren.empty())
        return;
    
    float childHalfSize = mHalfSize * 0.5f;
    int childDepth = mDepth + 1;
    
    // Создание 4 дочерних узлов
    mChildren.reserve(4);
    
    // Северо-западный дочерний узел
    DirectX::XMFLOAT3 nwCenter(
        mCenter.x - childHalfSize,
        mCenter.y,
        mCenter.z - childHalfSize
    );
    mChildren.push_back(std::make_unique<QuadTreeNode>(mQuadTree, nwCenter, childHalfSize, childDepth));
    
    // Северо-восточный дочерний узел
    DirectX::XMFLOAT3 neCenter(
        mCenter.x + childHalfSize,
        mCenter.y,
        mCenter.z - childHalfSize
    );
    mChildren.push_back(std::make_unique<QuadTreeNode>(mQuadTree, neCenter, childHalfSize, childDepth));
    
    // Юго-западный дочерний узел
    DirectX::XMFLOAT3 swCenter(
        mCenter.x - childHalfSize,
        mCenter.y,
        mCenter.z + childHalfSize
    );
    mChildren.push_back(std::make_unique<QuadTreeNode>(mQuadTree, swCenter, childHalfSize, childDepth));
    
    // Юго-восточный дочерний узел
    DirectX::XMFLOAT3 seCenter(
        mCenter.x + childHalfSize,
        mCenter.y,
        mCenter.z + childHalfSize
    );
    mChildren.push_back(std::make_unique<QuadTreeNode>(mQuadTree, seCenter, childHalfSize, childDepth));
}

void QuadTreeNode::Merge()
{
    mChildren.clear();
}

bool QuadTreeNode::NeedsSubdivision(const DirectX::XMFLOAT3& cameraPosition) const
{
    if (mDepth >= mQuadTree->GetMaxDepth())
        return false;
    
    // Расстояние от камеры до центра узла
    float dx = cameraPosition.x - mCenter.x;
    float dz = cameraPosition.z - mCenter.z;
    float distance = sqrtf(dx * dx + dz * dz);
    
    // Расчет необходимого LOD на основе расстояния
    float lodDistance = mQuadTree->GetLODDistanceFactor() * mHalfSize * (1 << mDepth);
    
    return distance < lodDistance;
}

bool QuadTreeNode::ShouldMerge(const DirectX::XMFLOAT3& cameraPosition) const
{
    // Расстояние от камеры до центра узла
    float dx = cameraPosition.x - mCenter.x;
    float dz = cameraPosition.z - mCenter.z;
    float distance = sqrtf(dx * dx + dz * dz);
    
    // Расчет расстояния для объединения (немного больше, чем для разделения)
    float mergeDistance = mQuadTree->GetLODDistanceFactor() * mHalfSize * (1 << (mDepth - 1)) * 1.5f;
    
    return distance > mergeDistance;
}

void QuadTreeNode::BuildTileGeometry()
{
    // Генерация геометрии для текущего тайла
    mTile = std::make_unique<TerrainTile>();
    
    // Определение размера тайла в мировых координатах
    int tileSize = mQuadTree->GetTileResolution();
    float worldSize = mHalfSize * 2.0f;
    
    // Генерация высот для тайла
    std::vector<float> heights;
    DirectX::XMFLOAT2 offset(
        mCenter.x - mHalfSize,
        mCenter.z - mHalfSize
    );
    
    mQuadTree->GetHeightMapGenerator()->GenerateHeightMap(heights, offset);
    
    // Создание геометрии тайла
    mTile->Initialize(
        mQuadTree->GetDevice(),
        mQuadTree->GetCommandList(),
        heights,
        tileSize,
        worldSize,
        mCenter
    );
}
```

### Шаг 4.3: Создание класса QuadTree
```cpp
// QuadTree.h
#pragma once
#include <DirectXMath.h>
#include <memory>
#include "QuadTreeNode.h"
#include "HeightMapGenerator.h"

class QuadTree
{
public:
    QuadTree(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
             float terrainSize, int maxDepth, int tileResolution);
    ~QuadTree() = default;
    
    void Update(const DirectX::XMFLOAT3& cameraPosition, const Frustum& frustum);
    void Render(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* objectCB, UINT objCBByteSize, UINT passCBIndex);
    
    ID3D12Device* GetDevice() const { return mDevice; }
    ID3D12GraphicsCommandList* GetCommandList() const { return mCommandList; }
    HeightMapGenerator* GetHeightMapGenerator() const { return mHeightMapGenerator.get(); }
    int GetMaxDepth() const { return mMaxDepth; }
    float GetLODDistanceFactor() const { return mLODDistanceFactor; }
    int GetTileResolution() const { return mTileResolution; }
    
    void SetLODDistanceFactor(float factor) { mLODDistanceFactor = factor; }

private:
    ID3D12Device* mDevice;
    ID3D12GraphicsCommandList* mCommandList;
    std::unique_ptr<QuadTreeNode> mRootNode;
    std::unique_ptr<HeightMapGenerator> mHeightMapGenerator;
    
    float mTerrainSize;
    int mMaxDepth;
    int mTileResolution;
    float mLODDistanceFactor;
};
```

### Шаг 4.4: Реализация класса QuadTree
```cpp
// QuadTree.cpp
#include "QuadTree.h"
#include <DirectXMath.h>

QuadTree::QuadTree(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                   float terrainSize, int maxDepth, int tileResolution)
    : mDevice(device), mCommandList(cmdList),
      mTerrainSize(terrainSize), mMaxDepth(maxDepth), mTileResolution(tileResolution),
      mLODDistanceFactor(1.0f)
{
    // Создание генератора карт высот
    mHeightMapGenerator = std::make_unique<HeightMapGenerator>(tileResolution, tileResolution, terrainSize / tileResolution);
    mHeightMapGenerator->SetOctaves(6);
    mHeightMapGenerator->SetFrequency(0.005f);
    mHeightMapGenerator->SetAmplitude(50.0f);
    mHeightMapGenerator->SetSeed(42);
    
    // Создание корневого узла
    DirectX::XMFLOAT3 center(0.0f, 0.0f, 0.0f);
    float halfSize = terrainSize * 0.5f;
    mRootNode = std::make_unique<QuadTreeNode>(this, center, halfSize, 0);
}

void QuadTree::Update(const DirectX::XMFLOAT3& cameraPosition, const Frustum& frustum)
{
    mRootNode->Update(cameraPosition, frustum);
}

void QuadTree::Render(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* objectCB, UINT objCBByteSize, UINT passCBIndex)
{
    mRootNode->Render(cmdList, objectCB, objCBByteSize, passCBIndex);
}
```

## Этап 5: Реализация тайла ландшафта

### Шаг 5.1: Создание класса TerrainTile
```cpp
// TerrainTile.h
#pragma once
#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>
#include <vector>

using Microsoft::WRL::ComPtr;

struct TerrainVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexCoord;
};

class TerrainTile
{
public:
    TerrainTile();
    ~TerrainTile() = default;
    
    void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                    const std::vector<float>& heights, int resolution,
                    float worldSize, const DirectX::XMFLOAT3& center);
    
    void Render(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* objectCB,
                UINT objCBByteSize, int lodLevel, UINT passCBIndex);
    
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const;
    
private:
    void CalculateNormals(std::vector<TerrainVertex>& vertices, const std::vector<float>& heights, int resolution);
    void CreateMesh(const std::vector<float>& heights, int resolution, float worldSize, const DirectX::XMFLOAT3& center);
    
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

### Шаг 5.2: Реализация класса TerrainTile
```cpp
// TerrainTile.cpp
#include "TerrainTile.h"
#include "../../Common/d3dUtil.h"
#include "../../Common/GeometryGenerator.h"
#include <DirectXCollision.h>

TerrainTile::TerrainTile()
    : mVertexByteStride(0), mVertexBufferByteSize(0), mIndexBufferByteSize(0), mIndexCount(0)
{
}

void TerrainTile::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                            const std::vector<float>& heights, int resolution,
                            float worldSize, const DirectX::XMFLOAT3& center)
{
    // Создание меша для тайла
    CreateMesh(heights, resolution, worldSize, center);
}

void TerrainTile::Render(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* objectCB,
                        UINT objCBByteSize, int lodLevel, UINT passCBIndex)
{
    // Установка vertex и index buffers
    cmdList->IASetVertexBuffers(0, 1, &VertexBufferView());
    cmdList->IASetIndexBuffer(&IndexBufferView());
    
    // Установка константного буфера для объекта
    D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + lodLevel * objCBByteSize;
    cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
    
    // Установка константного буфера для pass
    cmdList->SetGraphicsRootConstantBufferView(1, objectCB->GetGPUVirtualAddress() + passCBIndex * objCBByteSize);
    
    // Рисование
    cmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
}

D3D12_VERTEX_BUFFER_VIEW TerrainTile::VertexBufferView() const
{
    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    vbv.StrideInBytes = mVertexByteStride;
    vbv.SizeInBytes = mVertexBufferByteSize;
    return vbv;
}

D3D12_INDEX_BUFFER_VIEW TerrainTile::IndexBufferView() const
{
    D3D12_INDEX_BUFFER_VIEW ibv;
    ibv.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    ibv.SizeInBytes = mIndexBufferByteSize;
    ibv.Format = DXGI_FORMAT_R32_UINT; // Используем 32-битные индексы для больших тайлов
    return ibv;
}

void TerrainTile::CalculateNormals(std::vector<TerrainVertex>& vertices, const std::vector<float>& heights, int resolution)
{
    int width = resolution;
    int height = resolution;
    
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int index = y * width + x;
            DirectX::XMFLOAT3 normal(0.0f, 0.0f, 0.0f);
            
            // Вычисление нормали с использованием соседних вершин
            if (x > 0 && x < width - 1 && y > 0 && y < height - 1)
            {
                float leftHeight = heights[y * width + (x - 1)];
                float rightHeight = heights[y * width + (x + 1)];
                float bottomHeight = heights[(y - 1) * width + x];
                float topHeight = heights[(y + 1) * width + x];
                
                // Векторы для вычисления нормали
                DirectX::XMFLOAT3 tangent(
                    2.0f, // Разница по X
                    rightHeight - leftHeight,
                    0.0f
                );
                
                DirectX::XMFLOAT3 bitangent(
                    0.0f,
                    topHeight - bottomHeight,
                    2.0f // Разница по Z
                );
                
                // Векторное произведение для получения нормали
                DirectX::XMFLOAT3 cross;
                cross.x = tangent.y * bitangent.z - tangent.z * bitangent.y;
                cross.y = tangent.z * bitangent.x - tangent.x * bitangent.z;
                cross.z = tangent.x * bitangent.y - tangent.y * bitangent.x;
                
                // Нормализация
                float length = sqrtf(cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
                if (length > 0.0f)
                {
                    normal.x = cross.x / length;
                    normal.y = cross.y / length;
                    normal.z = cross.z / length;
                }
            }
            else
            {
                // Для краевых вершин используем упрощенный расчет
                normal = {0.0f, 1.0f, 0.0f};
            }
            
            vertices[index].Normal = normal;
        }
    }
}

void TerrainTile::CreateMesh(const std::vector<float>& heights, int resolution, float worldSize, const DirectX::XMFLOAT3& center)
{
    int width = resolution;
    int height = resolution;
    float halfWorldSize = worldSize * 0.5f;
    float cellSize = worldSize / (width - 1);
    
    // Создание вершин
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
            vertices[index].TexCoord = {static_cast<float>(x) / (width - 1), static_cast<float>(y) / (height - 1)};
        }
    }
    
    // Вычисление нормалей
    CalculateNormals(vertices, heights, resolution);
    
    // Создание индексов (треугольники)
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
            
            // Первый треугольник
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            // Второй треугольник
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
    
    mIndexCount = static_cast<UINT>(indices.size());
    
    // Настройка vertex buffer
    mVertexByteStride = sizeof(TerrainVertex);
    mVertexBufferByteSize = static_cast<UINT>(vertices.size()) * mVertexByteStride;
    
    // Настройка index buffer
    mIndexBufferByteSize = static_cast<UINT>(indices.size()) * sizeof(uint32_t);
    
    // Вычисление bounding box
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
    
    DirectX::BoundingBox::CreateFromPoints(mBoundingBox, 
        DirectX::XMLoadFloat3(&minCorner), 
        DirectX::XMLoadFloat3(&maxCorner));
    
    // Создание и заполнение буферов
    // Vertex buffer
    ThrowIfFailed(D3DCreateBlob(mVertexBufferByteSize, &mVertexBufferUpload));
    CopyMemory(mVertexBufferUpload->GetBufferPointer(), vertices.data(), mVertexBufferByteSize);
    
    mVertexBuffer = d3dUtil::CreateDefaultBuffer(mDevice, mCommandList,
        vertices.data(), mVertexBufferByteSize, mVertexBufferUpload);
    
    // Index buffer
    ThrowIfFailed(D3DCreateBlob(mIndexBufferByteSize, &mIndexBufferUpload));
    CopyMemory(mIndexBufferUpload->GetBufferPointer(), indices.data(), mIndexBufferByteSize);
    
    mIndexBuffer = d3dUtil::CreateDefaultBuffer(mDevice, mCommandList,
        indices.data(), mIndexBufferByteSize, mIndexBufferUpload);
}
```

## Этап 6: Реализация водной поверхности

### Шаг 6.1: Создание класса WaterRenderer
```cpp
// WaterRenderer.h
#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <memory>

using Microsoft::WRL::ComPtr;

class WaterRenderer
{
public:
    WaterRenderer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                 float gridSize, float gridSpacing);
    ~WaterRenderer() = default;
    
    void Update(const DirectX::XMFLOAT3& cameraPosition, float deltaTime);
    void Render(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* passCB, UINT passCBByteSize);
    
    void SetWaveSpeed(float speed) { mWaveSpeed = speed; }
    void SetWaveHeight(float height) { mWaveHeight = height; }
    void SetWaterColor(const DirectX::XMFLOAT4& color) { mWaterColor = color; }
    
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
    
    ID3D12Device* mDevice;
    ID3D12GraphicsCommandList* mCommandList;
    
    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12PipelineState> mPSO;
    ComPtr<ID3DBlob> mWaterVS;
    ComPtr<ID3DBlob> mWaterPS;
    
    ComPtr<ID3D12Resource> mVertexBuffer;
    ComPtr<ID3D12Resource> mVertexBufferUpload;
    ComPtr<ID3D12Resource> mIndexBuffer;
    ComPtr<ID3D12Resource> mIndexBufferUpload;
    ComPtr<ID3D12Resource> mWaveConstantBuffer;
    
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

### Шаг 6.2: Реализация класса WaterRenderer
```cpp
// WaterRenderer.cpp
#include "WaterRenderer.h"
#include "../../Common/d3dUtil.h"
#include <vector>

struct WaterVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT2 TexCoord;
};

WaterRenderer::WaterRenderer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                            float gridSize, float gridSpacing)
    : mDevice(device), mCommandList(cmdList),
      mGridSize(gridSize), mGridSpacing(gridSpacing)
{
    BuildWaterGeometry();
    BuildWaterPSO();
}

void WaterRenderer::Update(const DirectX::XMFLOAT3& cameraPosition, float deltaTime)
{
    mTime += deltaTime;
}

void WaterRenderer::Render(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* passCB, UINT passCBByteSize)
{
    // Установка PSO и root signature
    cmdList->SetPipelineState(mPSO.Get());
    cmdList->SetGraphicsRootSignature(mRootSignature.Get());
    
    // Установка pass constant buffer
    cmdList->SetGraphicsRootConstantBufferView(0, passCB->GetGPUVirtualAddress());
    
    // Обновление wave constants
    WaveConstants waveConstants;
    waveConstants.Time = mTime;
    waveConstants.WaveSpeed = mWaveSpeed;
    waveConstants.WaveHeight = mWaveHeight;
    waveConstants.Padding = 0.0f;
    
    void* mappedData = nullptr;
    ThrowIfFailed(mWaveConstantBuffer->Map(0, nullptr, &mappedData));
    memcpy(mappedData, &waveConstants, sizeof(WaveConstants));
    mWaveConstantBuffer->Unmap(0, nullptr);
    
    // Установка wave constant buffer
    cmdList->SetGraphicsRootConstantBufferView(1, mWaveConstantBuffer->GetGPUVirtualAddress());
    
    // Установка vertex и index buffers
    cmdList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    cmdList->IASetIndexBuffer(&mIndexBufferView);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    // Рисование
    cmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
}

void WaterRenderer::BuildWaterGeometry()
{
    int vertexCountPerSide = static_cast<int>(mGridSize / mGridSpacing) + 1;
    mVertexCount = vertexCountPerSide * vertexCountPerSide;
    mIndexCount = (vertexCountPerSide - 1) * (vertexCountPerSide - 1) * 6;
    mVBByteStride = sizeof(WaterVertex);
    
    std::vector<WaterVertex> vertices(mVertexCount);
    std::vector<uint16_t> indices(mIndexCount);
    
    float halfGridSize = mGridSize * 0.5f;
    
    // Создание вершин
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
    
    // Создание индексов
    int index = 0;
    for (int z = 0; z < vertexCountPerSide - 1; ++z)
    {
        for (int x = 0; x < vertexCountPerSide - 1; ++x)
        {
            uint16_t topLeft = z * vertexCountPerSide + x;
            uint16_t topRight = topLeft + 1;
            uint16_t bottomLeft = (z + 1) * vertexCountPerSide + x;
            uint16_t bottomRight = bottomLeft + 1;
            
            // Первый треугольник
            indices[index++] = topLeft;
            indices[index++] = bottomLeft;
            indices[index++] = topRight;
            
            // Второй треугольник
            indices[index++] = topRight;
            indices[index++] = bottomLeft;
            indices[index++] = bottomRight;
        }
    }
    
    // Создание буферов
    const UINT vbByteSize = mVertexCount * mVBByteStride;
    const UINT ibByteSize = mIndexCount * sizeof(uint16_t);
    
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mVertexBufferUpload));
    CopyMemory(mVertexBufferUpload->GetBufferPointer(), vertices.data(), vbByteSize);
    
    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mIndexBufferUpload));
    CopyMemory(mIndexBufferUpload->GetBufferPointer(), indices.data(), ibByteSize);
    
    mVertexBuffer = d3dUtil::CreateDefaultBuffer(mDevice, mCommandList,
        vertices.data(), vbByteSize, mVertexBufferUpload);
    
    mIndexBuffer = d3dUtil::CreateDefaultBuffer(mDevice, mCommandList,
        indices.data(), ibByteSize, mIndexBufferUpload);
    
    // Настройка view
    mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    mVertexBufferView.StrideInBytes = mVBByteStride;
    mVertexBufferView.SizeInBytes = vbByteSize;
    
    mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    mIndexBufferView.SizeInBytes = ibByteSize;
    mIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    
    // Создание wave constant buffer
    const UINT waveCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(WaveConstants));
    mWaveConstantBuffer = d3dUtil::CreateDefaultBuffer(mDevice, mCommandList,
        waveCBByteSize, mWaveConstantBufferUpload);
}

void WaterRenderer::BuildWaterPSO()
{
    // Компиляция шейдеров
    mWaterVS = d3dUtil::CompileShader(L"Shaders/Water.hlsl", nullptr, "VS", "vs_5_1");
    mWaterPS = d3dUtil::CompileShader(L"Shaders/Water.hlsl", nullptr, "PS", "ps_5_1");
    
    // Создание root signature
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
    
    ThrowIfFailed(mDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature)));
    
    // Описание input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    
    // Настройка PSO
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
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // Не отсекаем полигоны для воды
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Не пишем в depth buffer
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    
    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}
```

## Этап 7: Интеграция в основное приложение

### Шаг 7.1: Модификация BaselineApp.h для поддержки ландшафта
```cpp
// В BaselineApp.h добавьте или измените следующие методы и переменные:

private:
    // Методы для инициализации ландшафта
    void BuildTerrain();
    void BuildWater();
    
    // Методы для обновления и рендеринга
    void UpdateTerrain(const GameTimer& gt);
    void DrawTerrain(ID3D12GraphicsCommandList* cmdList);
    void DrawWater(ID3D12GraphicsCommandList* cmdList);
    
    // Добавьте в существующие методы:
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
    
    // Новые шейдеры и PSOs
    void BuildTerrainShadersAndInputLayout();
    void BuildWaterShadersAndInputLayout();
    void BuildTerrainPSOs();
    void BuildWaterPSOs();
    
    // Новые константные буферы
    void UpdateTerrainCBs(const GameTimer& gt);
    void UpdateWaterCBs(const GameTimer& gt);
```

### Шаг 7.2: Реализация методов инициализации в BaselineApp.cpp
```cpp
// В BaselineApp.cpp добавьте следующие методы:

void BaselineApp::Initialize()
{
    // Существующий код инициализации...
    
    // После инициализации камеры добавьте:
    BuildTerrainShadersAndInputLayout();
    BuildWaterShadersAndInputLayout();
    
    BuildTerrain();
    BuildWater();
    
    BuildTerrainPSOs();
    BuildWaterPSOs();
}

void BaselineApp::BuildTerrain()
{
    // Инициализация квадро-дерева
    mTerrainQuadTree = std::make_unique<QuadTree>(
        md3dDevice.Get(),
        mCommandList.Get(),
        mTerrainSize,
        mMaxQuadTreeDepth,
        65 // Разрешение тайла (65x65 вершин)
    );
    
    // Настройка параметров LOD
    mTerrainQuadTree->SetLODDistanceFactor(mLODDistanceFactor);
}

void BaselineApp::BuildWater()
{
    // Инициализация водной поверхности
    float waterGridSize = mTerrainSize * 2.0f; // Больше, чем ландшафт
    float waterGridSpacing = 5.0f; // Шаг сетки для воды
    
    mWaterRenderer = std::make_unique<WaterRenderer>(
        md3dDevice.Get(),
        mCommandList.Get(),
        waterGridSize,
        waterGridSpacing
    );
    
    // Настройка параметров воды
    mWaterRenderer->SetWaveSpeed(0.8f);
    mWaterRenderer->SetWaveHeight(0.3f);
    mWaterRenderer->SetWaterColor({0.0f, 0.4f, 0.7f, 1.0f});
}

void BaselineApp::BuildTerrainShadersAndInputLayout()
{
    // Компиляция шейдеров для ландшафта
    mShaders["terrainVS"] = d3dUtil::CompileShader(L"Shaders/Terrain.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["terrainPS"] = d3dUtil::CompileShader(L"Shaders/Terrain.hlsl", nullptr, "PS", "ps_5_1");
    
    // Input layout для ландшафта
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void BaselineApp::BuildWaterShadersAndInputLayout()
{
    // Водные шейдеры уже компилируются в WaterRenderer
    // Добавляем их в карту шейдеров для управления
    mShaders["waterVS"] = d3dUtil::CompileShader(L"Shaders/Water.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["waterPS"] = d3dUtil::CompileShader(L"Shaders/Water.hlsl", nullptr, "PS", "ps_5_1");
}

void BaselineApp::BuildTerrainPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = { 
        reinterpret_cast<BYTE*>(mShaders["terrainVS"]->GetBufferPointer()), 
        mShaders["terrainVS"]->GetBufferSize()
    };
    psoDesc.PS = { 
        reinterpret_cast<BYTE*>(mShaders["terrainPS"]->GetBufferPointer()),
        mShaders["terrainPS"]->GetBufferSize()
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
    
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["terrain"])));
}

void BaselineApp::BuildWaterPSOs()
{
    // PSO для воды уже создается в WaterRenderer
    // Нужно добавить ссылку в карту PSO
    // (Этот шаг может потребовать модификации WaterRenderer для экспорта PSO)
}
```

### Шаг 7.3: Обновление логики обновления и рендеринга
```cpp
void BaselineApp::Update(const GameTimer& gt)
{
    // Существующий код обновления...
    
    // Обновление frustum
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    mFrustum.Update(viewProj);
    
    // Обновление ландшафта
    UpdateTerrain(gt);
    
    // Обновление воды
    UpdateWater(gt);
}

void BaselineApp::UpdateTerrain(const GameTimer& gt)
{
    // Получение позиции камеры
    DirectX::XMFLOAT3 cameraPosition;
    XMStoreFloat3(&cameraPosition, mCamera.GetPosition());
    
    // Обновление квадро-дерева с учетом frustum culling
    mTerrainQuadTree->Update(cameraPosition, mFrustum);
}

void BaselineApp::UpdateWater(const GameTimer& gt)
{
    // Получение позиции камеры
    DirectX::XMFLOAT3 cameraPosition;
    XMStoreFloat3(&cameraPosition, mCamera.GetPosition());
    
    // Обновление воды
    mWaterRenderer->Update(cameraPosition, gt.DeltaTime());
}

void BaselineApp::Draw(const GameTimer& gt)
{
    // Существующий код подготовки командного списка...
    
    // Рендеринг ландшафта
    DrawTerrain(mCommandList.Get());
    
    // Рендеринг воды (после ландшафта, но с отключенной записью в depth buffer)
    DrawWater(mCommandList.Get());
    
    // Существующий код рендеринга (куб и т.д.)...
    
    // Завершение командного списка и представление...
}

void BaselineApp::DrawTerrain(ID3D12GraphicsCommandList* cmdList)
{
    // Установка PSO для ландшафта
    cmdList->SetPipelineState(mPSOs["terrain"].Get());
    
    // Установка root signature
    cmdList->SetGraphicsRootSignature(mRootSignature.Get());
    
    // Установка pass constant buffer
    auto passCB = mCurrFrameResource->PassCB->Resource();
    cmdList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());
    
    // Рендеринг квадро-дерева
    mTerrainQuadTree->Render(
        cmdList,
        mCurrFrameResource->ObjectCB->Resource(),
        d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants)),
        0 // Pass CB index
    );
}

void BaselineApp::DrawWater(ID3D12GraphicsCommandList* cmdList)
{
    // Рендеринг воды
    mWaterRenderer->Render(
        cmdList,
        mCurrFrameResource->PassCB->Resource(),
        d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants))
    );
}
```

## Этап 8: Создание шейдеров

### Шаг 8.1: Создание шейдера для ландшафта (Terrain.hlsl)
```hlsl
// Shaders/Terrain.hlsl
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
}

cbuffer cbPass : register(b1)
{
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
    float3 gEyePosW;
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
    
    // Преобразование в мировые координаты
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    
    // Преобразование нормали
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    
    // Преобразование в однородные координаты
    vout.PosH = mul(posW, gViewProj);
    
    // Передача текстурных координат
    vout.TexCoord = vin.TexCoord;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Простая текстура на основе высоты
    float height = pin.PosW.y;
    float3 color;
    
    if (height < 10.0f)
        color = float3(0.2f, 0.6f, 0.2f); // Зеленый для низменностей
    else if (height < 30.0f)
        color = float3(0.5f, 0.4f, 0.2f); // Коричневый для холмов
    else
        color = float3(0.8f, 0.8f, 0.8f); // Серый для гор
    
    // Простое освещение
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.5f));
    float diffuseFactor = max(0.0f, dot(normalize(pin.NormalW), lightDir));
    
    float3 ambient = gAmbientLight.rgb;
    float3 diffuse = color * diffuseFactor;
    
    float3 finalColor = ambient + diffuse;
    
    return float4(finalColor, 1.0f);
}
```

### Шаг 8.2: Создание шейдера для воды (Water.hlsl)
```hlsl
// Shaders/Water.hlsl
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

// Функция для генерации волн
float GerstnerWave(float x, float z, float time)
{
    float k = 0.1f; // Частота волны
    float amplitude = gWaveHeight;
    float speed = gWaveSpeed;
    
    float phase = dot(float2(x, z), float2(1.0f, 0.3f)) * k + time * speed;
    return amplitude * sin(phase);
}

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // Генерация волн
    float waveHeight = GerstnerWave(vin.PosL.x, vin.PosL.z, gTime);
    
    // Позиция вершины с учетом волн
    float3 posW = float3(vin.PosL.x, waveHeight, vin.PosL.z);
    vout.PosW = posW;
    
    // Преобразование в однородные координаты
    vout.PosH = mul(float4(posW, 1.0f), gViewProj);
    
    // Смещение текстурных координат для анимации
    vout.TexCoord = vin.TexCoord + float2(gTime * 0.05f, gTime * 0.03f);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Основной цвет воды
    float3 waterColor = float3(0.0f, 0.3f, 0.8f);
    
    // Анимация прозрачности на основе глубины
    float depth = distance(pin.PosW, gEyePosW);
    float alpha = 1.0f - saturate(depth / 100.0f) * 0.3f;
    
    // Добавление эффекта глубины
    float depthFactor = saturate(pin.PosW.y / 10.0f);
    waterColor = lerp(waterColor * 0.7f, waterColor * 1.2f, depthFactor);
    
    // Добавление бликов
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.5f));
    float3 viewDir = normalize(gEyePosW - pin.PosW);
    float3 halfVector = normalize(lightDir + viewDir);
    
    float3 normal = normalize(float3(0.0f, 1.0f, 0.0f)); // Упрощенная нормаль
    float specFactor = pow(max(0.0f, dot(normal, halfVector)), 32.0f);
    
    waterColor += float3(1.0f, 1.0f, 1.0f) * specFactor * 0.5f;
    
    return float4(waterColor, alpha);
}
```

## Этап 9: Финальная настройка и оптимизация

### Шаг 9.1: Добавление управления параметрами через клавиатуру
```cpp
void BaselineApp::OnKeyPressed(const GameTimer& gt, WPARAM key)
{
    // Существующий код...
    
    switch (key)
    {
    case '1': // Увеличение LOD distance factor
        mLODDistanceFactor *= 1.2f;
        mTerrainQuadTree->SetLODDistanceFactor(mLODDistanceFactor);
        break;
    case '2': // Уменьшение LOD distance factor
        mLODDistanceFactor /= 1.2f;
        mTerrainQuadTree->SetLODDistanceFactor(mLODDistanceFactor);
        break;
    case '3': // Увеличение высоты волн
        mWaterRenderer->SetWaveHeight(mWaterRenderer->GetWaveHeight() * 1.2f);
        break;
    case '4': // Уменьшение высоты волн
        mWaterRenderer->SetWaveHeight(mWaterRenderer->GetWaveHeight() / 1.2f);
        break;
    case '5': // Увеличение скорости волн
        mWaterRenderer->SetWaveSpeed(mWaterRenderer->GetWaveSpeed() * 1.2f);
        break;
    case '6': // Уменьшение скорости волн
        mWaterRenderer->SetWaveSpeed(mWaterRenderer->GetWaveSpeed() / 1.2f);
        break;
    }
}
```

### Шаг 9.2: Оптимизация производительности
- Реализовать кэширование тайлов
- Добавить систему повторного использования памяти
- Оптимизировать шейдеры для мобильных устройств
- Добавить уровень детализации для воды в зависимости от расстояния

### Шаг 9.3: Добавление текстур и улучшение визуального качества
- Добавить текстуры для ландшафта на основе высоты
- Реализовать смешивание текстур между уровнями высот
- Добавить нормал-мапы для воды
- Реализовать отражение и преломление для воды

## Заключение

Этот подробный план предоставляет пошаговую инструкцию для реализации ландшафта с использованием квадро-дерева для LOD, frustum culling, генерации карт высот и бесконечной водной поверхности. Каждый этап содержит конкретные шаги реализации, которые можно скопировать и использовать в Cursor для разработки.

Для успешной реализации следуйте шагам в указанном порядке, тестируя каждый компонент отдельно перед интеграцией. Начните с frustum culling, затем реализуйте генератор карт высот, квадро-дерево, тайлы ландшафта и, наконец, водную поверхность.

При возникновении проблем с производительностью оптимизируйте шейдеры и алгоритмы, используя профилирование GPU.