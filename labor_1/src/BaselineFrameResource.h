#pragma once

#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

// Simple constant buffer for object transformation
struct ObjectConstants
{
    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
};

// Simple constant buffer for pass data
struct PassConstants
{
    float TotalTime = 0.0f;
    
    // Heightmap parameters
    float heightScale = 100.0f;           // Scale factor for height values
    float terrainSize = 1000.0f;          // Size of terrain in world units
    UINT heightmapWidth = 256;            // Default heightmap width
    UINT heightmapHeight = 256;           // Default heightmap height
    float tileSize = 32.0f;               // Size of each terrain tile
    
    // Camera position for LOD calculations
    DirectX::XMFLOAT3 cameraPosition = { 0.0f, 0.0f, 0.0f };
    float padding = 0.0f;  // Padding for alignment
};

// Simple vertex with position and color
struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
    
    Vertex() {}
    Vertex(const DirectX::XMFLOAT3& p, const DirectX::XMFLOAT4& c) 
        : Pos(p), Color(c) {}
};

// Stores the resources needed for the CPU to build the command lists
// for a frame.  
struct BaselineFrameResource
{
public:
    
    BaselineFrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
    BaselineFrameResource(const BaselineFrameResource& rhs) = delete;
    BaselineFrameResource& operator=(const BaselineFrameResource& rhs) = delete;
    ~BaselineFrameResource();

    // We cannot reset the allocator until the GPU is done processing the commands.
    // So each frame needs their own allocator.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    // We cannot update a cbuffer until the GPU is done processing the commands
    // that reference it.  So each frame needs their own cbuffers.
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
    
    // Fence value to mark commands up to this fence point.  This lets us
    // check if these frame resources are still in use by the GPU.
    UINT64 Fence = 0;
};
