//***************************************************************************************
// Simplified BaselineApp - Rotating Rainbow Cube with Camera Controls
//***************************************************************************************
#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "GeometryGenerator.h"
#include "Camera.h"
#include <array>
#include <string>
#include "BaselineFrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

// Quadtree node structure for terrain LOD
struct QuadtreeNode
{
    // Bounding information
    DirectX::XMFLOAT3 center;           // Center of this node's terrain patch
    float halfSize;                     // Half the size of this node (in world units)
    
    // LOD information
    UINT level;                         // Level in quadtree (0 = root, higher = more detailed)
    float screenSpaceError;             // Calculated error for this LOD level
    
    // Child nodes
    std::unique_ptr<QuadtreeNode> children[4];  // NW, NE, SW, SE
    
    // Terrain tile information
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;
    ComPtr<ID3D12Resource> vertexBufferUpload;
    ComPtr<ID3D12Resource> indexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    UINT vertexCount;
    UINT indexCount;
    
    // Rendering state
    bool isVisible = false;             // Set during frustum culling
    bool needsUpdate = true;           // Set when geometry needs regeneration
    bool shouldRender = false;         // Set when this node should be rendered (LOD selected)
    
    // Constructor
    QuadtreeNode(const DirectX::XMFLOAT3& centerPos, float size, UINT lvl)
        : center(centerPos), halfSize(size), level(lvl), screenSpaceError(0.0f), shouldRender(false)
    {
        // Initialize child pointers to null
        for (auto& child : children)
        {
            child = nullptr;
        }
    }
    
    // Destructor
    ~QuadtreeNode()
    {
        // Children will be automatically deleted due to unique_ptr
    }
    
    // Check if this node has children
    bool HasChildren() const
    {
        return children[0] != nullptr;
    }
    
    // Get child index based on position
    UINT GetChildIndex(const DirectX::XMFLOAT3& position) const
    {
        // 0 = NW, 1 = NE, 2 = SW, 3 = SE
        if (position.x < center.x)
        {
            return (position.z < center.z) ? 2 : 0; // SW or NW
        }
        else
        {
            return (position.z < center.z) ? 3 : 1; // SE or NE
        }
    }
};

// Simple render item for the cube
struct RenderItem
{
    RenderItem() = default;
    RenderItem(const RenderItem& rhs) = delete;

    XMFLOAT4X4 World = MathHelper::Identity4x4();
    int NumFramesDirty = gNumFrameResources;
    UINT ObjCBIndex = 0;
    MeshGeometry* Geo = nullptr;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class BaselineApp : public D3DApp
{
public:
    BaselineApp(HINSTANCE hInstance);
    BaselineApp(const BaselineApp& rhs) = delete;
    BaselineApp& operator=(const BaselineApp& rhs) = delete;
    ~BaselineApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;
    virtual void DeferredDraw(const GameTimer& gt)override { Draw(gt); }
    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
    virtual void OnKeyPressed(const GameTimer& gt, WPARAM key)override;

    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildTerrainShaders();
    void BuildCubeGeometry();
    void BuildPSOs();
    void BuildTerrainPSO();
    void BuildFrameResources();
    void BuildRenderItems();
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdatePassCB(const GameTimer& gt);
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList);
    
    // Heightmap loading
    bool LoadHeightmapFromFile(const std::wstring& heightmapPath);
    bool LoadTerrainTexture(const std::wstring& texturePath);
    void CreateSrvDescriptorHeap();
    
    // Terrain rendering
    void RenderQuadtreeNodes(ID3D12GraphicsCommandList* cmdList, QuadtreeNode* node);
    
    // Quadtree methods
    void BuildQuadtree();
    UINT CalculateMaxLODLevels();
    void BuildQuadtreeRecursive(QuadtreeNode* node, UINT maxLevels);
    void CreateTerrainTile(QuadtreeNode* node);
    void CalculateScreenSpaceError(QuadtreeNode* node);
    
    // LOD and culling
    void SelectLODLevels();
    void SelectLODRecursive(QuadtreeNode* node, bool parentVisible);
    bool IsNodeVisible(const QuadtreeNode* node) const;
    void ResetRenderFlags(QuadtreeNode* node);

private:
    std::vector<std::unique_ptr<BaselineFrameResource>> mFrameResources;
    BaselineFrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    std::vector<std::unique_ptr<RenderItem>> mAllRitems;
    RenderItem* mCubeRitem = nullptr;

    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mCubeRotation = 0.0f;
    
    // Pass constant buffer data (stored on CPU for updates)
    PassConstants mPassCB;

    // Camera
    Camera mCamera;
    POINT mLastMousePos;
    bool mRightMouseDown = false;
    
    // Heightmap resources
    ComPtr<ID3D12Resource> mHeightmapTexture;
    ComPtr<ID3D12Resource> mHeightmapUploadHeap;
    ComPtr<ID3D12Resource> mTerrainTexture;
    ComPtr<ID3D12Resource> mTerrainTextureUploadHeap;
    ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE mHeightmapSrvHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE mTerrainTextureSrvHandle;
    UINT mHeightmapWidth = 0;
    UINT mHeightmapHeight = 0;
    
    // Quadtree terrain system
    std::unique_ptr<QuadtreeNode> mQuadtreeRoot;
    DirectX::XMFLOAT3 mTerrainCenter = { 0.0f, 0.0f, 0.0f };
    float mTerrainHalfSize = 500.0f;  // Half of total terrain size (1000x1000 total)
    
    // Frustum culling
    DirectX::BoundingFrustum mCameraFrustum;
    XMMATRIX mViewMatrix;
    XMMATRIX mProjectionMatrix;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        BaselineApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

BaselineApp::BaselineApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

BaselineApp::~BaselineApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool BaselineApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildCubeGeometry();
    BuildRenderItems();
    BuildPSOs();
    BuildFrameResources();
    
    // Create terrain shader and PSO
    BuildTerrainShaders();
    BuildTerrainPSO();
    
    // Create SRV descriptor heap for textures
    CreateSrvDescriptorHeap();
    
    // Load heightmap from terrain directory
    if (!LoadHeightmapFromFile(L"Textures\\terrain\\ter_highmap.dds"))
    {
        OutputDebugString(L"Failed to load heightmap. Using default flat terrain.\n");
    }
    
    // Load terrain texture
    if (!LoadTerrainTexture(L"Textures\\terrain\\ter_texture.dds"))
    {
        OutputDebugString(L"Failed to load terrain texture.\n");
    }
    
    // Build quadtree after heightmap is loaded
    BuildQuadtree();

    // Initialize camera
    mCamera.SetPosition(0.0f, 2.0f, -5.0f);
    mCamera.LookAt(XMVectorSet(0.0f, 2.0f, -5.0f, 0.0f), 
                   XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), 
                   XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    mCamera.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    mCamera.UpdateViewMatrix();
    
    // Initialize pass constant buffer with default terrain values
    mPassCB.heightScale = 100.0f;
    mPassCB.terrainSize = 1000.0f;
    mPassCB.heightmapWidth = 256;  // Will be updated when heightmap is loaded
    mPassCB.heightmapHeight = 256;
    mPassCB.tileSize = 32.0f;

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();
    return true;
}

void BaselineApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    if(md3dDevice != nullptr)
    {
        mCamera.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    }
    XMMATRIX P = mCamera.GetProj();
    XMStoreFloat4x4(&mProj, P);
}

void BaselineApp::Update(const GameTimer& gt)
{
    // Rotate the cube
    mCubeRotation += 1.0f * gt.DeltaTime();
    if(mCubeRotation > XM_2PI)
        mCubeRotation -= XM_2PI;

    // Update camera movement
    const float dt = gt.DeltaTime();
    const float moveSpeed = 20.0f;  // Increased from 5.0f for faster movement
    
    if(GetAsyncKeyState('W') & 0x8000)
        mCamera.Walk(moveSpeed * dt);
    if(GetAsyncKeyState('S') & 0x8000)
        mCamera.Walk(-moveSpeed * dt);
    if(GetAsyncKeyState('A') & 0x8000)
        mCamera.Strafe(-moveSpeed * dt);
    if(GetAsyncKeyState('D') & 0x8000)
        mCamera.Strafe(moveSpeed * dt);

    mCamera.UpdateViewMatrix();
    
    // Update camera position in constant buffer (for LOD calculations)
    XMVECTOR camPos = mCamera.GetPosition();
    XMStoreFloat3(&mPassCB.cameraPosition, camPos);
    
    // Update view and projection matrices for frustum culling
    mViewMatrix = mCamera.GetView();
    mProjectionMatrix = mCamera.GetProj();
    
    // Create frustum from matrices
    DirectX::BoundingFrustum::CreateFromMatrix(mCameraFrustum, mProjectionMatrix);
    mCameraFrustum.Transform(mCameraFrustum, XMMatrixInverse(nullptr, mViewMatrix));

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

void BaselineApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

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
    
    // Set descriptor heap for textures
    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    
    // Set texture descriptor table
    CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    mCommandList->SetGraphicsRootDescriptorTable(2, texHandle);

    // Perform LOD selection and frustum culling
    if (mQuadtreeRoot)
    {
        SelectLODLevels();
        
        // Render terrain using quadtree
        mCommandList->SetPipelineState(mPSOs["terrain"].Get());
        RenderQuadtreeNodes(mCommandList.Get(), mQuadtreeRoot.get());
    }
    
    // Render cube (for reference)
    mCommandList->SetPipelineState(mPSOs["opaque"].Get());
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

void BaselineApp::BuildRootSignature()
{
    // Create descriptor table for textures (heightmap and terrain texture)
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);  // 2 textures: heightmap + terrain texture

    CD3DX12_ROOT_PARAMETER slotRootParameter[3];

    slotRootParameter[0].InitAsConstantBufferView(0); // Object constants (b0)
    slotRootParameter[1].InitAsConstantBufferView(1); // Pass constants (b1)
    slotRootParameter[2].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_ALL); // Textures (t0, t1) - used in both VS and PS

    // Static sampler for texture sampling
    CD3DX12_STATIC_SAMPLER_DESC samplerDesc(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
        1, &samplerDesc,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void BaselineApp::BuildShadersAndInputLayout()
{
    mShaders["pyramidVS"] = d3dUtil::CompileShader(L"Shaders\\Pyramid.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["pyramidPS"] = d3dUtil::CompileShader(L"Shaders\\Pyramid.hlsl", nullptr, "PS", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void BaselineApp::BuildTerrainShaders()
{
    mShaders["terrainVS"] = d3dUtil::CompileShader(L"Shaders\\Terrain.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["terrainPS"] = d3dUtil::CompileShader(L"Shaders\\Terrain.hlsl", nullptr, "PS", "ps_5_1");
}

void BaselineApp::BuildCubeGeometry()
{
    // Use GeometryGenerator to create a cube with proper normals
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(2.0f, 2.0f, 2.0f, 0); // 2x2x2 cube

    // Convert GeometryGenerator vertices to our Vertex format (Pos + Color)
    // We'll assign colors based on face normals for visual distinction
    std::vector<Vertex> vertices(box.Vertices.size());
    
    for(size_t i = 0; i < box.Vertices.size(); ++i)
    {
        const auto& v = box.Vertices[i];
        XMFLOAT4 color;
        
        // Assign color based on normal direction for visual distinction
        // Map normals to colors (normalized to 0-1 range)
        color.x = (v.Normal.x + 1.0f) * 0.5f;
        color.y = (v.Normal.y + 1.0f) * 0.5f;
        color.z = (v.Normal.z + 1.0f) * 0.5f;
        color.w = 1.0f;
        
        vertices[i] = Vertex(v.Position, color);
    }

    // Convert indices from 32-bit to 16-bit
    std::vector<std::uint16_t> indices16 = box.GetIndices16();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices16.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "cubeGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices16.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices16.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices16.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["cube"] = submesh;

    mGeometries[geo->Name] = std::move(geo);
}

void BaselineApp::BuildPSOs()
{
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

void BaselineApp::BuildTerrainPSO()
{
    // Terrain input layout (only position)
    std::vector<D3D12_INPUT_ELEMENT_DESC> terrainInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { terrainInputLayout.data(), (UINT)terrainInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = 
    { 
        reinterpret_cast<BYTE*>(mShaders["terrainVS"]->GetBufferPointer()), 
        mShaders["terrainVS"]->GetBufferSize()
    };
    psoDesc.PS = 
    { 
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

void BaselineApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<BaselineFrameResource>(md3dDevice.Get(),
            1, 1));
    }
}

void BaselineApp::BuildRenderItems()
{
    auto cubeRitem = std::make_unique<RenderItem>();
    cubeRitem->World = MathHelper::Identity4x4();
    cubeRitem->ObjCBIndex = 0;
    cubeRitem->Geo = mGeometries["cubeGeo"].get();
    cubeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    cubeRitem->IndexCount = cubeRitem->Geo->DrawArgs["cube"].IndexCount;
    cubeRitem->StartIndexLocation = cubeRitem->Geo->DrawArgs["cube"].StartIndexLocation;
    cubeRitem->BaseVertexLocation = cubeRitem->Geo->DrawArgs["cube"].BaseVertexLocation;
    mCubeRitem = cubeRitem.get();
    mAllRitems.push_back(std::move(cubeRitem));
}

void BaselineApp::UpdateObjectCBs(const GameTimer& gt)
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

void BaselineApp::UpdatePassCB(const GameTimer& gt)
{
    // Use camera view matrix
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();
    
    XMStoreFloat4x4(&mView, view);
    XMStoreFloat4x4(&mProj, proj);

    auto currPassCB = mCurrFrameResource->PassCB.get();
    PassConstants passConstants = mPassCB;  // Copy all members including heightmap params
    passConstants.TotalTime = gt.TotalTime();
    currPassCB->CopyData(0, passConstants);
}

void BaselineApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    auto objectCB = mCurrFrameResource->ObjectCB->Resource();

    for(size_t i = 0; i < mAllRitems.size(); ++i)
    {
        auto ri = mAllRitems[i].get();

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void BaselineApp::CreateSrvDescriptorHeap()
{
    // Create descriptor heap for shader resource views
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 2;  // Heightmap + terrain texture
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));
    
    mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

bool BaselineApp::LoadTerrainTexture(const std::wstring& texturePath)
{
    // Use DDSTextureLoader to load the terrain texture
    ComPtr<ID3D12Resource> texture;
    ComPtr<ID3D12Resource> uploadHeap;
    
    HRESULT hr = DirectX::CreateDDSTextureFromFile12(
        md3dDevice.Get(),
        mCommandList.Get(),
        texturePath.c_str(),
        texture,
        uploadHeap
    );
    
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to load terrain texture file.\n");
        return false;
    }
    
    // Store the texture and upload heap
    mTerrainTexture = texture;
    mTerrainTextureUploadHeap = uploadHeap;
    
    // Create SRV for the terrain texture
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, mCbvSrvUavDescriptorSize);
    
    D3D12_RESOURCE_DESC texDesc = mTerrainTexture->GetDesc();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
    
    md3dDevice->CreateShaderResourceView(mTerrainTexture.Get(), &srvDesc, srvHandle);
    mTerrainTextureSrvHandle = srvHandle;
    
    OutputDebugString(L"Successfully loaded terrain texture.\n");
    return true;
}

bool BaselineApp::LoadHeightmapFromFile(const std::wstring& heightmapPath)
{
    // Use DDSTextureLoader to load the heightmap
    ComPtr<ID3D12Resource> texture;
    ComPtr<ID3D12Resource> uploadHeap;
    
    HRESULT hr = DirectX::CreateDDSTextureFromFile12(
        md3dDevice.Get(),
        mCommandList.Get(),
        heightmapPath.c_str(),
        texture,
        uploadHeap
    );
    
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to load heightmap texture file.\n");
        return false;
    }
    
    // Store the texture and upload heap
    mHeightmapTexture = texture;
    mHeightmapUploadHeap = uploadHeap;
    
    // Get texture dimensions
    D3D12_RESOURCE_DESC texDesc = mHeightmapTexture->GetDesc();
    mHeightmapWidth = static_cast<UINT>(texDesc.Width);
    mHeightmapHeight = static_cast<UINT>(texDesc.Height);
    
    // Update constant buffer with heightmap dimensions
    mPassCB.heightmapWidth = mHeightmapWidth;
    mPassCB.heightmapHeight = mHeightmapHeight;
    
    // Create SRV for the heightmap
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, mCbvSrvUavDescriptorSize);
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
    
    md3dDevice->CreateShaderResourceView(mHeightmapTexture.Get(), &srvDesc, srvHandle);
    mHeightmapSrvHandle = srvHandle;
    
    OutputDebugString(L"Successfully loaded heightmap from external generator.\n");
    return true;
}

void BaselineApp::BuildQuadtree()
{
    // Clear existing quadtree
    mQuadtreeRoot.reset();
    
    // Create root node covering entire terrain
    mQuadtreeRoot = std::make_unique<QuadtreeNode>(mTerrainCenter, mTerrainHalfSize, 0);
    
    // Set maximum LOD levels based on heightmap resolution
    UINT maxLODLevels = CalculateMaxLODLevels();
    
    // Recursively build quadtree
    BuildQuadtreeRecursive(mQuadtreeRoot.get(), maxLODLevels);
    
    OutputDebugString(L"Quadtree construction completed.\n");
}

UINT BaselineApp::CalculateMaxLODLevels()
{
    // Calculate based on heightmap resolution - create more LOD levels for smaller tiles
    UINT width = mPassCB.heightmapWidth;
    UINT height = mPassCB.heightmapHeight;
    
    // Find the larger dimension (use parentheses to prevent Windows.h macro expansion)
    UINT maxDim = (std::max)(width, height);
    
    // Calculate LOD levels - we want more levels for better granularity
    // Each level splits terrain into 4 smaller pieces
    UINT levels = 0;
    float currentSize = static_cast<float>(maxDim);
    
    // Continue splitting until nodes are small enough (around 32-64 pixels per node)
    while (currentSize > 32.0f && levels < 10)
    {
        currentSize /= 2.0f;
        levels++;
    }
    
    // Ensure minimum of 4 levels and maximum of 10 for performance
    return (std::max)(4u, (std::min)(levels, 10u));
}

void BaselineApp::BuildQuadtreeRecursive(QuadtreeNode* node, UINT maxLevels)
{
    if (node->level >= maxLevels)
    {
        // This is a leaf node - create terrain geometry
        CreateTerrainTile(node);
        return;
    }
    
    // Calculate child node positions and sizes
    float childSize = node->halfSize / 2.0f;
    DirectX::XMFLOAT3 childCenters[4];
    
    // NW child (0)
    childCenters[0] = {
        node->center.x - childSize,
        node->center.y,
        node->center.z - childSize
    };
    
    // NE child (1)
    childCenters[1] = {
        node->center.x + childSize,
        node->center.y,
        node->center.z - childSize
    };
    
    // SW child (2)
    childCenters[2] = {
        node->center.x - childSize,
        node->center.y,
        node->center.z + childSize
    };
    
    // SE child (3)
    childCenters[3] = {
        node->center.x + childSize,
        node->center.y,
        node->center.z + childSize
    };
    
    // Create children
    for (UINT i = 0; i < 4; i++)
    {
        node->children[i] = std::make_unique<QuadtreeNode>(childCenters[i], childSize, node->level + 1);
        BuildQuadtreeRecursive(node->children[i].get(), maxLevels);
    }
}

void BaselineApp::CreateTerrainTile(QuadtreeNode* node)
{
    // This method creates the actual geometry for a terrain tile at this quadtree node
    ID3D12Device* device = md3dDevice.Get();
    
    // Calculate tile dimensions based on LOD level - smaller tiles for higher LOD levels
    // Higher LOD = more detail = smaller tiles
    // Level 0 (root): 5x5 vertices (smallest)
    // Level 1: 6x6 vertices
    // Level 2+: 8x8 vertices (maximum detail for leaf nodes)
    UINT baseVertices = 5;
    if (node->level >= 2)
        baseVertices = 8;
    else if (node->level == 1)
        baseVertices = 6;
    
    UINT verticesPerSide = baseVertices;  // Small tiles for better LOD performance
    UINT totalVertices = verticesPerSide * verticesPerSide;
    UINT totalIndices = (verticesPerSide - 1) * (verticesPerSide - 1) * 6;  // 2 triangles per quad
    
    // Calculate vertex positions
    std::vector<DirectX::XMFLOAT3> vertices(totalVertices);
    std::vector<UINT> indices(totalIndices);
    
    float tileWorldSize = node->halfSize * 2.0f;
    float vertexSpacing = tileWorldSize / (verticesPerSide - 1);
    
    for (UINT z = 0; z < verticesPerSide; z++)
    {
        for (UINT x = 0; x < verticesPerSide; x++)
        {
            UINT index = z * verticesPerSide + x;
            
            // Calculate world position
            float worldX = node->center.x - node->halfSize + x * vertexSpacing;
            float worldZ = node->center.z - node->halfSize + z * vertexSpacing;
            
            // Sample height from heightmap (will be implemented in vertex shader)
            float height = 0.0f;  // Placeholder - actual height will be sampled in shader
            
            vertices[index] = { worldX, height, worldZ };
        }
    }
    
    // Create indices (triangle list)
    UINT index = 0;
    for (UINT z = 0; z < verticesPerSide - 1; z++)
    {
        for (UINT x = 0; x < verticesPerSide - 1; x++)
        {
            UINT topLeft = z * verticesPerSide + x;
            UINT topRight = topLeft + 1;
            UINT bottomLeft = (z + 1) * verticesPerSide + x;
            UINT bottomRight = bottomLeft + 1;
            
            // First triangle (top-left, bottom-left, top-right)
            indices[index++] = topLeft;
            indices[index++] = bottomLeft;
            indices[index++] = topRight;
            
            // Second triangle (top-right, bottom-left, bottom-right)
            indices[index++] = topRight;
            indices[index++] = bottomLeft;
            indices[index++] = bottomRight;
        }
    }
    
    // Create vertex buffer
    const UINT vertexBufferSize = static_cast<UINT>(vertices.size() * sizeof(DirectX::XMFLOAT3));
    
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
    
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&node->vertexBuffer)
    ));
    
    // Create upload heap for vertex data
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&node->vertexBufferUpload)
    ));
    
    // Copy vertex data to upload heap
    UINT8* vertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(node->vertexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin)));
    memcpy(vertexDataBegin, vertices.data(), vertexBufferSize);
    node->vertexBufferUpload->Unmap(0, nullptr);
    
    // Copy data to default heap
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = vertices.data();
    vertexData.RowPitch = vertexBufferSize;
    vertexData.SlicePitch = vertexBufferSize;
    
    // Transition to COPY_DEST before copying
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        node->vertexBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);
    mCommandList->ResourceBarrier(1, &barrier);
    
    UpdateSubresources(mCommandList.Get(), node->vertexBuffer.Get(), 
                      node->vertexBufferUpload.Get(), 0, 0, 1, &vertexData);
    
    // Transition back to COMMON after copying
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        node->vertexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_COMMON);
    mCommandList->ResourceBarrier(1, &barrier);
    
    // Create index buffer
    const UINT indexBufferSize = static_cast<UINT>(indices.size() * sizeof(UINT));
    
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
    
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&node->indexBuffer)
    ));
    
    // Create upload heap for index data
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&node->indexBufferUpload)
    ));
    
    // Copy index data to upload heap
    UINT8* indexDataBegin;
    ThrowIfFailed(node->indexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&indexDataBegin)));
    memcpy(indexDataBegin, indices.data(), indexBufferSize);
    node->indexBufferUpload->Unmap(0, nullptr);
    
    // Copy data to default heap
    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = indices.data();
    indexData.RowPitch = indexBufferSize;
    indexData.SlicePitch = indexBufferSize;
    
    // Transition to COPY_DEST before copying
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        node->indexBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);
    mCommandList->ResourceBarrier(1, &barrier);
    
    UpdateSubresources(mCommandList.Get(), node->indexBuffer.Get(),
                      node->indexBufferUpload.Get(), 0, 0, 1, &indexData);
    
    // Transition back to COMMON after copying
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        node->indexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_COMMON);
    mCommandList->ResourceBarrier(1, &barrier);
    
    // Set up buffer views
    node->vertexBufferView.BufferLocation = node->vertexBuffer->GetGPUVirtualAddress();
    node->vertexBufferView.StrideInBytes = sizeof(DirectX::XMFLOAT3);
    node->vertexBufferView.SizeInBytes = vertexBufferSize;
    
    node->indexBufferView.BufferLocation = node->indexBuffer->GetGPUVirtualAddress();
    node->indexBufferView.SizeInBytes = indexBufferSize;
    node->indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    
    node->vertexCount = totalVertices;
    node->indexCount = totalIndices;
    node->needsUpdate = false;
    
    // Calculate screen space error for this LOD level
    CalculateScreenSpaceError(node);
}

void BaselineApp::CalculateScreenSpaceError(QuadtreeNode* node)
{
    // Screen space error calculation based on node size and LOD level
    // More accurate calculation that considers distance and screen resolution
    
    // Get viewport dimensions
    float viewportHeight = static_cast<float>(mScreenViewport.Height);
    float viewportWidth = static_cast<float>(mScreenViewport.Width);
    float aspectRatio = viewportWidth / viewportHeight;
    
    // Calculate base error as world space size of the node
    float nodeWorldSize = node->halfSize * 2.0f;
    
    // Scale by LOD level - each level represents finer detail
    // Higher LOD levels have exponentially smaller error
    float lodScale = 1.0f / (1.0f + static_cast<float>(node->level) * 0.5f);
    
    // Calculate projected screen space error
    // This approximates how many pixels the node would occupy on screen
    // at a typical viewing distance
    float baseError = nodeWorldSize * lodScale;
    
    // Normalize by viewport size (assuming typical FOV)
    // This gives us pixels of error
    node->screenSpaceError = (baseError / 100.0f) * (viewportHeight / 720.0f);
    
    // Apply terrain complexity factor
    float terrainComplexityFactor = 1.0f;
    node->screenSpaceError *= terrainComplexityFactor;
}

void BaselineApp::ResetRenderFlags(QuadtreeNode* node)
{
    if (!node) return;
    node->shouldRender = false;
    node->isVisible = false;
    for (auto& child : node->children)
    {
        if (child) ResetRenderFlags(child.get());
    }
}

void BaselineApp::SelectLODLevels()
{
    // Reset all render flags first
    ResetRenderFlags(mQuadtreeRoot.get());
    
    // Start from root node
    SelectLODRecursive(mQuadtreeRoot.get(), false);
}

void BaselineApp::SelectLODRecursive(QuadtreeNode* node, bool parentVisible)
{
    if (!node)
        return;
    
    // Check visibility first
    bool isVisible = parentVisible || IsNodeVisible(node);
    node->isVisible = isVisible;
    
    if (!isVisible)
    {
        // If not visible, don't process children
        return;
    }
    
    // Get camera distance to node center
    XMVECTOR cameraPos = XMLoadFloat3(&mPassCB.cameraPosition);
    XMVECTOR nodeCenter = XMLoadFloat3(&node->center);
    XMVECTOR diff = cameraPos - nodeCenter;
    float distance = XMVectorGetX(XMVector3Length(diff));
    
    // Calculate screen space error threshold - more aggressive for smaller tiles
    // Lower threshold means more detail (smaller tiles) will be used
    const float maxScreenSpaceError = 2.0f;  // Reduced from 3.0f for more detail
    
    // Distance-based LOD - use higher detail when closer
    float distanceFactor = 1.0f;
    if (distance < 100.0f)
        distanceFactor = 0.5f;  // Use more detail when close
    else if (distance > 500.0f)
        distanceFactor = 2.0f;  // Use less detail when far
    
    float adjustedThreshold = maxScreenSpaceError * distanceFactor;
    
    // Check if this node's error is acceptable or if we should use children
    bool useThisNode = true;
    
    if (node->HasChildren())
    {
        // Calculate this node's projected screen space error
        float nodeProjectedError = node->screenSpaceError;
        
        // Check if children would provide better detail
        for (auto& child : node->children)
        {
            if (child)
            {
                // Calculate distance to child center
                XMVECTOR childCenter = XMLoadFloat3(&child->center);
                XMVECTOR childDiff = cameraPos - childCenter;
                float childDistance = XMVectorGetX(XMVector3Length(childDiff));
                
                // Calculate projected screen space error for child
                // Children are smaller, so their error should be less
                float childProjectedError = child->screenSpaceError;
                
                // If child has significantly less error and is closer, use children
                if (childProjectedError < adjustedThreshold && childDistance < distance)
                {
                    // Child provides better detail - use children instead of this node
                    useThisNode = false;
                    break;
                }
            }
        }
        
        // Also check if this node's error is too high - force subdivision
        if (nodeProjectedError > adjustedThreshold * 2.0f)
        {
            useThisNode = false;  // Force subdivision for large errors
        }
    }
    
    if (useThisNode)
    {
        // Use this node - mark it for rendering, don't process children
        node->shouldRender = true;
        if (node->needsUpdate)
        {
            CreateTerrainTile(node);
        }
    }
    else
    {
        // Don't render this node, use children instead
        node->shouldRender = false;
        // Process each child
        for (auto& child : node->children)
        {
            if (child)
            {
                SelectLODRecursive(child.get(), isVisible);
            }
        }
    }
}

bool BaselineApp::IsNodeVisible(const QuadtreeNode* node) const
{
    if (!node)
        return false;
    
    // Create bounding box for more accurate culling (better than sphere for terrain tiles)
    DirectX::BoundingBox box;
    box.Center = node->center;
    
    // Calculate extent (half-size) - account for height variation
    float extent = node->halfSize;
    box.Extents = DirectX::XMFLOAT3(extent, mPassCB.heightScale * 0.5f, extent);
    
    // Check against frustum using bounding box
    DirectX::ContainmentType containment = mCameraFrustum.Contains(box);
    
    // Also check if node is too far away (optimization)
    if (containment != DirectX::DISJOINT)
    {
        XMVECTOR cameraPos = XMLoadFloat3(&mPassCB.cameraPosition);
        XMVECTOR nodeCenter = XMLoadFloat3(&node->center);
        XMVECTOR diff = cameraPos - nodeCenter;
        float distance = XMVectorGetX(XMVector3Length(diff));
        
        // Cull nodes that are very far away (beyond reasonable render distance)
        const float maxRenderDistance = 2000.0f;
        if (distance > maxRenderDistance)
            return false;
    }
    
    return containment != DirectX::DISJOINT;
}

void BaselineApp::RenderQuadtreeNodes(ID3D12GraphicsCommandList* cmdList, QuadtreeNode* node)
{
    if (!node || !node->isVisible)
        return;
    
    // If this node should be rendered (LOD selected it)
    if (node->shouldRender)
    {
        // This is a leaf node - render it
        if (node->vertexBuffer && node->indexBuffer)
        {
            // Ensure buffers are in correct state for reading (COMMON is fine for buffers)
            // Buffers in COMMON state can be read by GPU without explicit transition
            
            // Set vertex and index buffers
            cmdList->IASetVertexBuffers(0, 1, &node->vertexBufferView);
            cmdList->IASetIndexBuffer(&node->indexBufferView);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            
            // Set world matrix (identity for now, terrain is in world space)
            XMMATRIX world = XMMatrixIdentity();
            XMMATRIX viewProj = XMLoadFloat4x4(&mView) * XMLoadFloat4x4(&mProj);
            
            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
            XMStoreFloat4x4(&objConstants.ViewProj, XMMatrixTranspose(viewProj));
            
            // Update object constant buffer
            auto currObjectCB = mCurrFrameResource->ObjectCB.get();
            currObjectCB->CopyData(0, objConstants);
            
            UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
            auto objectCB = mCurrFrameResource->ObjectCB->Resource();
            D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + 0 * objCBByteSize;
            cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
            
            // Draw the terrain tile
            cmdList->DrawIndexedInstanced(node->indexCount, 1, 0, 0, 0);
        }
    }
    else
    {
        // Render children recursively
        for (auto& child : node->children)
        {
            if (child)
            {
                RenderQuadtreeNodes(cmdList, child.get());
            }
        }
    }
}

void BaselineApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    if((btnState & MK_RBUTTON) != 0)
    {
        mRightMouseDown = true;
        mLastMousePos.x = x;
        mLastMousePos.y = y;
        SetCapture(mhMainWnd);
    }
}

void BaselineApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
    mRightMouseDown = false;
}

void BaselineApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if(mRightMouseDown)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

        mCamera.Pitch(dy);
        mCamera.Yaw(dx);

        mCamera.UpdateViewMatrix();
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void BaselineApp::OnKeyPressed(const GameTimer& gt, WPARAM key)
{
    // WASD movement is handled in Update() via GetAsyncKeyState
    // This method is called for key down events, but we're handling continuous
    // movement in Update() instead for smoother control
}
