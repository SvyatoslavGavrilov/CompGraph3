//***************************************************************************************
// Simplified Labor4App - Rotating Rainbow Cube with Camera Controls
//***************************************************************************************
#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "GeometryGenerator.h"
#include "Camera.h"
#include <array>
#include <string>
#include "labor_4FrameResource.h"
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

// [[Quadtree-LOD-system]] Quadtree node structure for terrain LOD
struct QuadtreeNode
{
    // Bounding information
    DirectX::XMFLOAT3 center;           // Center of this node's terrain patch
    float halfSize;                     // Half the size of this node (in world units)
    
    // [[LOD-selection-algorithm]] LOD information
    UINT level;                         // Level in quadtree (0 = root, higher = more detailed)
    float screenSpaceError;             // Calculated error for this LOD level
    
    // Child nodes
    std::unique_ptr<QuadtreeNode> children[4];  // NW, NE, SW, SE
    
    // [[Terrain-tile-generation]] Terrain tile information
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;
    ComPtr<ID3D12Resource> vertexBufferUpload;
    ComPtr<ID3D12Resource> indexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    UINT vertexCount;
    UINT indexCount;
    
    // Skirt geometry to hide [[LOD-selection-algorithm]] LOD gaps
    ComPtr<ID3D12Resource> skirtVertexBuffer;
    ComPtr<ID3D12Resource> skirtIndexBuffer;
    ComPtr<ID3D12Resource> skirtVertexBufferUpload;
    ComPtr<ID3D12Resource> skirtIndexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW skirtVertexBufferView;
    D3D12_INDEX_BUFFER_VIEW skirtIndexBufferView;
    UINT skirtVertexCount;
    UINT skirtIndexCount;
    
    // Rendering state
    bool isVisible = false;             // Set during [[Frustum-culling-module]] frustum culling
    bool needsUpdate = true;           // Set when geometry needs regeneration
    bool shouldRender = false;         // Set when this node should be rendered ([[LOD-selection-algorithm]] LOD selected)
    
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

class Labor4App : public D3DApp
{
public:
    Labor4App(HINSTANCE hInstance);
    Labor4App(const Labor4App& rhs) = delete;
    Labor4App& operator=(const Labor4App& rhs) = delete;
    ~Labor4App();

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
    
    // Persistent paint system
    void InitializePaintTexture();
    bool RaycastTerrainCPU(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDir, DirectX::XMFLOAT3& hitPoint);
    void PaintTerrainAtPosition(const DirectX::XMFLOAT3& worldPos);
    void UpdatePaintTexture();
    
    // [[Terrain-rendering-pipeline]] Terrain rendering
    void RenderQuadtreeNodes(ID3D12GraphicsCommandList* cmdList, QuadtreeNode* node);
    
    // [[Quadtree-LOD-system]] Quadtree methods
    void BuildQuadtree();
    UINT CalculateMaxLODLevels();
    void BuildQuadtreeRecursive(QuadtreeNode* node, UINT maxLevels);
    void CreateTerrainTile(QuadtreeNode* node);
    void CreateSkirtGeometry(QuadtreeNode* node, UINT verticesPerSide, float tileWorldSize);
    void CalculateScreenSpaceError(QuadtreeNode* node);
    
    // [[LOD-selection-algorithm]] LOD and [[Frustum-culling-module]] culling
    void SelectLODLevels();
    void SelectLODRecursive(QuadtreeNode* node, bool parentVisible);
    bool IsNodeVisible(const QuadtreeNode* node) const;
    void ResetRenderFlags(QuadtreeNode* node);
    
    // Atmosphere rendering
    void InitializeAtmosphere();
    void BuildAtmosphereShaders();
    void BuildAtmospherePSO();
    void BuildAtmosphereRootSignature();
    void BuildSkyDomeGeometry();
    void RenderAtmosphere(ID3D12GraphicsCommandList* cmdList);
    void RenderAtmosphereGUI();
    void UpdateAtmosphereCB();
    void UpdateTerrainAtmosphereCB();

private:
    std::vector<std::unique_ptr<Labor4FrameResource>> mFrameResources;
    Labor4FrameResource* mCurrFrameResource = nullptr;
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
    bool mLeftMouseDown = false;  // Left mouse button state for terrain drawing
    
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
    
    // Paint texture resources (persistent terrain painting)
    ComPtr<ID3D12Resource> mPaintTexture;
    ComPtr<ID3D12Resource> mPaintTextureUploadHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE mPaintTextureSrvHandle;
    std::vector<UINT8> mPaintTextureData;  // CPU-side paint data (RGBA)
    UINT mPaintTextureWidth = 512;  // Paint texture resolution
    UINT mPaintTextureHeight = 512;
    bool mPaintTextureDirty = false;  // Flag to update GPU texture
    float mBrushRadius = 5.0f;  // Brush size in world units
    DirectX::XMFLOAT3 mPaintColor = DirectX::XMFLOAT3(1.0f, 0.41f, 0.71f);  // Pink default
    
    // IMGUI descriptor heap
    ComPtr<ID3D12DescriptorHeap> mImGuiDescriptorHeap;
    
    // [[Quadtree-LOD-system]] Quadtree terrain system
    std::unique_ptr<QuadtreeNode> mQuadtreeRoot;
    DirectX::XMFLOAT3 mTerrainCenter = { 0.0f, 0.0f, 0.0f };
    float mTerrainHalfSize = 50.0f;  // Half of total terrain size (100x100 total)
    
    // [[Frustum-culling-module]] Frustum culling
    DirectX::BoundingFrustum mCameraFrustum;
    XMMATRIX mViewMatrix;
    XMMATRIX mProjectionMatrix;
    bool mFrustumCullingEnabled = true;  // Enable/disable [[Frustum-culling-module]] frustum culling
    bool mFrustumNeedsUpdate = true;     // Flag to update frustum only on 'C' key press
    
    // Tessellation constants
    struct TessellationConstants
    {
        float minTessellationFactor = 1.0f;
        float maxTessellationFactor = 64.0f;
        float tessellationDistance = 100.0f;
        float padding = 0.0f;
    };
    TessellationConstants mTessellationConstants;
    std::unique_ptr<UploadBuffer<TessellationConstants>> mTessellationCB = nullptr;
    
    // Atmosphere rendering
    struct AtmosphereParams
    {
        DirectX::XMFLOAT4X4 View;
        DirectX::XMFLOAT4X4 Projection;
        DirectX::XMFLOAT3 CameraPos;
        float CameraAltitudeDisplacement; // Artificial altitude offset for better atmospheric calculations
        DirectX::XMFLOAT3 SunDirection;
        float padding1;
        DirectX::XMFLOAT3 PlanetCenter;
        float AtmosphereRadius;
        float PlanetRadius;
        float padding2;
        DirectX::XMFLOAT3 RayleighScattering;
        float padding3;
        DirectX::XMFLOAT3 MieScattering;
        float MieG;
        float SunIntensity;
    int AtmosphereMode; // 0 = Hoffman-Preetham, 1 = Ray Marching
    float DensityMultiplier;
    float PollutionLevel;
    float SunAngularRadius; // Angular radius of sun disk in radians
    float padding4;
    // Exponential Height Fog parameters (used by terrain shader)
    float FogHeight; // Reference height for fog
    float FogDensity; // Fog density multiplier
    float FogHeightFalloff; // How quickly fog density changes with height
    float MinFogOpacity; // Minimum fog opacity
    DirectX::XMFLOAT3 FogColor; // Fog inscattering color
    float padding5;
    int EnableFog; // Enable/disable exponential height fog (1 = enabled, 0 = disabled)
    float padding6[3];
    };
    
    AtmosphereParams mAtmosphereSettings;
    bool mEnableAtmosphere = true;
    bool mAnimateSunDirection = false; // Enable/disable sun direction animation
    ComPtr<ID3D12RootSignature> mAtmosphereRootSignature = nullptr;
    std::unique_ptr<UploadBuffer<AtmosphereParams>> mAtmosphereCB = nullptr;
    MeshGeometry* mSkyDomeGeo = nullptr;
    
    // Terrain atmosphere constants (simplified structure matching shader)
    struct TerrainAtmosphereConstants
    {
        DirectX::XMFLOAT3 sunDirection;
        float atmosphereRadius;
        float planetRadius;
        float pollutionLevel;
        float densityMultiplier;
        int atmosphereMode;
        float SunIntensity; // Sun intensity for terrain lighting
        // Exponential Height Fog parameters for terrain
        float FogHeight;
        float FogDensity;
        float FogHeightFalloff;
        float MinFogOpacity;
        DirectX::XMFLOAT3 FogColor;
        float paddingFog0;
        int EnableFog;
        float paddingFog1[3];
    };
    std::unique_ptr<UploadBuffer<TerrainAtmosphereConstants>> mTerrainAtmosphereCB = nullptr;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        Labor4App theApp(hInstance);
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

Labor4App::Labor4App(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

Labor4App::~Labor4App()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool Labor4App::Initialize()
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
    
    // Create [[Terrain-shader-pipeline]] terrain shader and PSO
    BuildTerrainShaders();
    BuildTerrainPSO();
    
    // Create SRV descriptor heap for [[Texture-loading-system]] textures
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
    
    // Build [[Quadtree-LOD-system]] quadtree after heightmap is loaded
    BuildQuadtree();
    
    // Initialize persistent paint texture
    InitializePaintTexture();
    
    // Create tessellation constant buffer
    mTessellationCB = std::make_unique<UploadBuffer<TessellationConstants>>(md3dDevice.Get(), 1, true);
    mTessellationCB->CopyData(0, mTessellationConstants);
    
    // Initialize atmosphere rendering
    InitializeAtmosphere();
    
    // Initialize IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(mhMainWnd);
    
    // Create descriptor heap for IMGUI (need more descriptors for fonts and textures)
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 100; // Enough for fonts and textures
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mImGuiDescriptorHeap)));
    
    // Get descriptor handles for the first descriptor (for font texture)
    D3D12_CPU_DESCRIPTOR_HANDLE fontSrvCpuHandle = mImGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE fontSrvGpuHandle = mImGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    
    // Initialize DirectX12 backend for IMGUI using legacy method (simpler)
    ImGui_ImplDX12_Init(md3dDevice.Get(), gNumFrameResources, mBackBufferFormat, 
                       mImGuiDescriptorHeap.Get(), fontSrvCpuHandle, fontSrvGpuHandle);

    // Initialize camera
    mCamera.SetPosition(0.0f, 2.0f, -5.0f);
    mCamera.LookAt(XMVectorSet(0.0f, 2.0f, -5.0f, 0.0f), 
                   XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), 
                   XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    // Increased far plane to accommodate sky dome (radius 1000.0f needs at least 2000.0f far plane)
    mCamera.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 20000.0f);
    mCamera.UpdateViewMatrix();
    
    // Initialize pass constant buffer with default terrain values
    mPassCB.heightScale = 100.0f;  // Increased for more prominent height changes (was 10.0f)
    mPassCB.terrainSize = 100.0f;  // Reduced by 10x (was 1000.0f)
    mPassCB.heightmapWidth = 256;  // Will be updated when heightmap is loaded
    mPassCB.heightmapHeight = 256;
    mPassCB.tileSize = 3.2f;  // Reduced by 10x

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();
    return true;
}

void Labor4App::OnResize()
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

void Labor4App::Update(const GameTimer& gt)
{
    // [[Rendering-pipeline]] Update function: Called every frame before rendering
    // This function prepares all data needed for the current frame:
    // 1. Updates camera position/orientation
    // 2. Updates frustum for culling
    // 3. Updates constant buffers
    // 4. Manages frame resource synchronization
    // 4. Manages frame resource synchronizatio
    
    // Rotate the cube (reference object, not terrain-related)
    mCubeRotation += 1.0f * gt.DeltaTime();
    if(mCubeRotation > XM_2PI)
        mCubeRotation -= XM_2PI;

    // [[Rendering-pipeline]] STEP 1: Update camera movement based on keyboard input
    // WASD keys control camera movement in world space
    // This allows the user to explore the terrain
    const float dt = gt.DeltaTime();
    const float moveSpeed = 20.0f;  // Increased from 5.0f for faster movement
    
    if(GetAsyncKeyState('W') & 0x8000)
        mCamera.Walk(moveSpeed * dt);   // Move forward along look vector
    if(GetAsyncKeyState('S') & 0x8000)
        mCamera.Walk(-moveSpeed * dt);  // Move backward
    if(GetAsyncKeyState('A') & 0x8000)
        mCamera.Strafe(-moveSpeed * dt); // Strafe left (perpendicular to look vector)
    if(GetAsyncKeyState('D') & 0x8000)
        mCamera.Strafe(moveSpeed * dt);  // Strafe right

    // [[Rendering-pipeline]] STEP 2: Recalculate view matrix after camera movement
    // The view matrix transforms world coordinates to view space (camera-relative)
    // Must be updated whenever camera position or orientation changes
    mCamera.UpdateViewMatrix();
    
    // [[LOD-selection-algorithm]] STEP 3: Update camera position in constant buffer
    // This position is used by:
    // - CPU-side LOD selection: Calculate distance from camera to terrain nodes
    // - GPU-side tessellation: Calculate distance from camera to patches (in ConstantHS)
    // The constant buffer is updated once per frame and shared by all terrain patches
    XMVECTOR camPos = mCamera.GetPosition();
    XMStoreFloat3(&mPassCB.cameraPosition, camPos);
    
    // [[Frustum-culling-module]] STEP 4: Update view and projection matrices
    // These matrices are needed to construct the frustum for visibility testing
    // View matrix: transforms world to view space
    // Projection matrix: defines the shape of the view frustum (FOV, aspect, near/far planes)
    mViewMatrix = mCamera.GetView();
    mProjectionMatrix = mCamera.GetProj();
    
    // [[Frustum-culling-module]] STEP 5: Update frustum when flag is set
    // The frustum is only updated when mFrustumNeedsUpdate is true
    // This flag is set when 'C' key is pressed (for testing) or could be set every frame
    // Lazy updates save CPU time if camera doesn't move
    if (mFrustumNeedsUpdate)
    {
        // [[Frustum-culling-module]] STEP 5a: Create frustum from projection matrix
        // The projection matrix encodes the frustum shape (FOV, aspect ratio, near/far planes)
        // CreateFromMatrix extracts this information and creates a BoundingFrustum object
        // The frustum is initially in VIEW SPACE (relative to camera at origin)
        DirectX::BoundingFrustum::CreateFromMatrix(mCameraFrustum, mProjectionMatrix);
        
        // [[Frustum-culling-module]] STEP 5b: Transform frustum to world space
        // The frustum is in view space, but terrain nodes are in world space
        // We need to transform the frustum to world space for comparison
        // The inverse view matrix transforms from view space to world space
        // This accounts for camera position and orientation
        XMMATRIX inverseView = XMMatrixInverse(nullptr, mViewMatrix);
        mCameraFrustum.Transform(mCameraFrustum, inverseView);
        
        // [[Frustum-culling-module]] Reset flag after update
        // Prevents unnecessary updates until flag is set again
        mFrustumNeedsUpdate = false;
    }

    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    // [[Sun-animation]] Animate sun direction Y parameter on sine wave
    if (mAnimateSunDirection)
    {
        // Animate sun direction Y from -1 to 1 using sine wave
        // Use total time for smooth continuous animation
        float sunY = sinf(gt.TotalTime() * 0.5f); // 0.5f controls animation speed
        mAtmosphereSettings.SunDirection.y = sunY;
        
        // Normalize sun direction after modifying Y component
        XMVECTOR sunDir = XMLoadFloat3(&mAtmosphereSettings.SunDirection);
        sunDir = XMVector3Normalize(sunDir);
        XMStoreFloat3(&mAtmosphereSettings.SunDirection, sunDir);
    }

    UpdateObjectCBs(gt);
    UpdatePassCB(gt);
    
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Labor4App::Draw(const GameTimer& gt)
{
    // [[Rendering-pipeline]] Draw function: Records all rendering commands for the current frame
    // This function orchestrates the entire rendering process:
    // 1. Sets up render targets and viewports
    // 2. Binds resources (constant buffers, textures, samplers)
    // 3. Performs LOD selection and frustum culling
    // 4. Issues draw calls for terrain patches
    // 5. Presents the final image to screen
    
    // [[Rendering-pipeline]] STEP 1: Get command allocator for current frame
    // Each frame has its own command allocator to prevent CPU-GPU synchronization issues
    // Triple buffering (3 frame resources) allows CPU to work 2 frames ahead
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // [[Rendering-pipeline]] STEP 2: Reset command allocator and command list
    // Reset allocator: Makes memory available for new commands (reuses existing memory)
    // Reset command list: Prepares for recording new commands, binds initial PSO
    // The initial PSO ("opaque") is for non-terrain objects, terrain PSO is set later
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
    
    // Update paint texture if dirty (paint changes need to be uploaded to GPU)
    UpdatePaintTexture();

    // [[Rendering-pipeline]] STEP 3: Set viewport and scissor rect
    // Viewport: Defines the rendering area on the render target (full screen typically)
    // Scissor rect: Clips rendering to a specific rectangle (can be used for optimization)
    // These are set once per frame and apply to all subsequent draw calls
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // [[Rendering-pipeline]] STEP 4: Transition back buffer to render target state
    // Back buffer starts in PRESENT state (ready for display)
    // Must transition to RENDER_TARGET state before we can render to it
    // Resource barriers are required in DirectX 12 for state transitions
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // [[Rendering-pipeline]] STEP 5: Clear render target and depth buffer
    // Clear render target: Fills with black background (atmosphere will render over it)
    // Clear depth buffer: Sets all depth values to 1.0 (far plane = maximum depth)
    // Clear stencil buffer: Sets all stencil values to 0
    // Clearing ensures we start with a clean slate each frame
    // If atmosphere is enabled, it will render as the background; otherwise black background is shown
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Black background
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), clearColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // [[Rendering-pipeline]] STEP 6: Set render targets
    // OMSetRenderTargets: Binds render target and depth/stencil buffer
    // The GPU will render to these buffers for all subsequent draw calls
    // First parameter: Number of render targets (1 = single render target)
    // Second parameter: Pointer to render target view
    // Third parameter: true = also bind depth/stencil buffer
    // Fourth parameter: Pointer to depth/stencil view
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
    
    // Render atmosphere first (background)
    if (mEnableAtmosphere)
    {
        RenderAtmosphere(mCommandList.Get());
    }

    // [[Rendering-pipeline]] STEP 7: Set root signature
    // Root signature defines how shaders access resources (constant buffers, textures, samplers)
    // Must be set before setting any resources or drawing
    // The root signature acts as a contract between CPU and GPU about resource layout
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // [[Rendering-pipeline]] STEP 8: Set pass constant buffer (root parameter slot 1, register b1)
    // Pass constants contain data shared by all terrain patches:
    // - Camera position (for LOD and tessellation calculations)
    // - Heightmap parameters (width, height, scale, terrain size)
    // - Total time (for animations, if needed)
    // These are updated once per frame in UpdatePassCB()
    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());
    
    // [[Rendering-pipeline]] STEP 9: Set descriptor heap for textures
    // Descriptor heap contains shader resource views (SRVs) for textures
    // Must be set before using textures in shaders
    // Only one descriptor heap can be active at a time for each type (SRV/CBV/UAV)
    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    
    // [[Rendering-pipeline]] STEP 10: Set texture descriptor table (root parameter slot 4, registers t0, t1)
    // This binds the heightmap texture (t0) and terrain texture (t1) to the shader
    // The descriptor table points to the start of the SRV heap
    // Shaders can access textures using register indices (t0, t1)
    CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    mCommandList->SetGraphicsRootDescriptorTable(4, texHandle);
    
    // [[Rendering-pipeline]] STEP 11: Set tessellation constant buffer (root parameter slot 2, register b2)
    // Tessellation constants: min/max tessellation factors, tessellation distance
    // These control how much detail the GPU tessellation adds to terrain patches
    // Updated rarely (only when tessellation settings change)
    auto tessCB = mTessellationCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(2, tessCB->GetGPUVirtualAddress());
    
    // Set atmosphere constant buffer for terrain extinction (root parameter slot 3, register b3)
    if (mEnableAtmosphere && mTerrainAtmosphereCB)
    {
        UpdateTerrainAtmosphereCB();
        auto terrainAtmosphereCB = mTerrainAtmosphereCB->Resource();
        mCommandList->SetGraphicsRootConstantBufferView(3, terrainAtmosphereCB->GetGPUVirtualAddress());
    }

    // [[LOD-selection-algorithm]] [[Frustum-culling-module]] STEP 12: Perform LOD selection and frustum culling
    // This is the critical optimization step that determines which terrain nodes to render
    // SelectLODLevels() does the following:
    // 1. Resets all render flags (shouldRender, isVisible)
    // 2. Traverses the quadtree recursively
    // 3. For each node:
    //    a. Tests visibility against frustum (IsNodeVisible)
    //    b. Calculates distance from camera
    //    c. Determines if node should render or subdivide (LOD selection)
    // 4. Marks nodes for rendering (shouldRender = true)
    if (mQuadtreeRoot)
    {
        SelectLODLevels();  // [[LOD-selection-algorithm]] Traverse quadtree, select LOD, perform culling
        
        // [[Terrain-rendering-pipeline]] STEP 13: Set terrain pipeline state
        // This activates the terrain shader pipeline:
        // - Vertex Shader (VS): Processes control points
        // - Hull Shader (HS): Calculates tessellation factors
        // - Domain Shader (DS): Evaluates final vertex positions, samples heightmap
        // - Pixel Shader (PS): Applies terrain texture and lighting
        // The PSO was created in BuildTerrainPSO() with patch topology
        mCommandList->SetPipelineState(mPSOs["terrain"].Get());
        
        // [[Terrain-rendering-pipeline]] STEP 14: Render terrain patches
        // This recursively traverses the quadtree and issues draw calls for visible nodes
        // RenderQuadtreeNodes() does the following:
        // 1. Checks if node is visible (from frustum culling)
        // 2. If node should render (from LOD selection):
        //    a. Binds vertex and index buffers
        //    b. Sets patch topology
        //    c. Updates object constant buffer (world matrix, view-projection)
        //    d. Issues draw call for patches
        // 3. If node should not render, recursively processes children
        RenderQuadtreeNodes(mCommandList.Get(), mQuadtreeRoot.get());
    }
    
    // [[Rendering-pipeline]] STEP 15: Render other objects (cube, etc.)
    // Switch to opaque PSO for non-terrain objects
    mCommandList->SetPipelineState(mPSOs["opaque"].Get());
    DrawRenderItems(mCommandList.Get());
    
    // Render IMGUI windows
    ImGui::Begin("Atmosphere Controls");
    RenderAtmosphereGUI();
    ImGui::End();
    
    // Render IMGUI
    ImGui::Render();
    
    // Set IMGUI descriptor heap
    ID3D12DescriptorHeap* imGuiHeaps[] = { mImGuiDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(imGuiHeaps), imGuiHeaps);
    
    // Render IMGUI draw data
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

    // [[Rendering-pipeline]] STEP 16: Transition back buffer to present state
    // Back buffer must be in PRESENT state before presenting to screen
    // This is required by the swap chain
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // [[Rendering-pipeline]] STEP 17: Close command list
    // Command list must be closed before execution
    // Closing finalizes the command list and makes it ready for GPU execution
    ThrowIfFailed(mCommandList->Close());

    // [[Rendering-pipeline]] STEP 18: Execute command list
    // Submit the command list to the GPU for execution
    // The GPU will process all commands asynchronously
    // The CPU can continue working while GPU processes commands
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // [[Rendering-pipeline]] STEP 19: Present to screen
    // Swap the back buffer to the front buffer for display
    // This makes the rendered frame visible on screen
    // Present(0, 0) means: sync interval 0 (no VSync), flags 0
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // [[Rendering-pipeline]] STEP 20: Signal fence
    // Mark this frame resource as in use by setting fence value
    // This allows the CPU to track when the GPU finishes processing this frame
    // The fence is checked in Update() to prevent overwriting data the GPU is still using
    mCurrFrameResource->Fence = ++mCurrentFence;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void Labor4App::BuildRootSignature()
{
    // Create descriptor table for textures (heightmap, terrain texture, and paint texture)
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);  // 3 textures: heightmap + terrain texture + paint texture

    CD3DX12_ROOT_PARAMETER slotRootParameter[5];

    slotRootParameter[0].InitAsConstantBufferView(0); // Object constants (b0)
    slotRootParameter[1].InitAsConstantBufferView(1); // Pass constants (b1)
    slotRootParameter[2].InitAsConstantBufferView(2); // Tessellation constants (b2)
    slotRootParameter[3].InitAsConstantBufferView(3); // Atmosphere constants (b3) for terrain extinction
    slotRootParameter[4].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_ALL); // Textures (t0, t1) - used in both VS and PS

    // Static sampler for texture sampling
    CD3DX12_STATIC_SAMPLER_DESC samplerDesc(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
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

void Labor4App::BuildShadersAndInputLayout()
{
    mShaders["pyramidVS"] = d3dUtil::CompileShader(L"Shaders\\Pyramid.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["pyramidPS"] = d3dUtil::CompileShader(L"Shaders\\Pyramid.hlsl", nullptr, "PS", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void Labor4App::BuildTerrainShaders()
{
    mShaders["terrainVS"] = d3dUtil::CompileShader(L"Shaders\\Terrain.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["terrainHS"] = d3dUtil::CompileShader(L"Shaders\\Terrain.hlsl", nullptr, "HS", "hs_5_1");
    mShaders["terrainDS"] = d3dUtil::CompileShader(L"Shaders\\Terrain.hlsl", nullptr, "DS", "ds_5_1");
    mShaders["terrainPS"] = d3dUtil::CompileShader(L"Shaders\\Terrain.hlsl", nullptr, "PS", "ps_5_1");
}

void Labor4App::BuildCubeGeometry()
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

void Labor4App::BuildPSOs()
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

void Labor4App::BuildTerrainPSO()
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
    psoDesc.HS = 
    { 
        reinterpret_cast<BYTE*>(mShaders["terrainHS"]->GetBufferPointer()),
        mShaders["terrainHS"]->GetBufferSize()
    };
    psoDesc.DS = 
    { 
        reinterpret_cast<BYTE*>(mShaders["terrainDS"]->GetBufferPointer()),
        mShaders["terrainDS"]->GetBufferSize()
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
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;  // Patch topology for tessellation
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["terrain"])));
}

void Labor4App::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<Labor4FrameResource>(md3dDevice.Get(),
            1, 1));
    }
}

void Labor4App::BuildRenderItems()
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

void Labor4App::UpdateObjectCBs(const GameTimer& gt)
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

void Labor4App::UpdatePassCB(const GameTimer& gt)
{
    // Use camera view matrix
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();
    
    XMStoreFloat4x4(&mView, view);
    XMStoreFloat4x4(&mProj, proj);

    auto currPassCB = mCurrFrameResource->PassCB.get();
    PassConstants passConstants = mPassCB;  // Copy all members including heightmap params
    passConstants.TotalTime = gt.TotalTime();
    
    // Convert mouse position from screen coordinates to normalized device coordinates (NDC)
    // NDC: x in [-1, 1], y in [-1, 1] where (0,0) is center, (-1,-1) is bottom-left, (1,1) is top-right
    // Screen: x in [0, mClientWidth], y in [0, mClientHeight] where (0,0) is top-left
    float mouseX = static_cast<float>(mLastMousePos.x);
    float mouseY = static_cast<float>(mLastMousePos.y);
    float ndcX = (mouseX / mClientWidth) * 2.0f - 1.0f;  // Convert [0, width] to [-1, 1]
    float ndcY = 1.0f - (mouseY / mClientHeight) * 2.0f;  // Convert [0, height] to [1, -1] (flip Y)
    passConstants.mouseScreenPos = DirectX::XMFLOAT2(ndcX, ndcY);
    
    // Set mouse button state
    passConstants.mouseButtonPressed = mLeftMouseDown ? 1 : 0;
    
    // Store view and projection matrices for ray tracing in shader
    XMStoreFloat4x4(&passConstants.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&passConstants.Proj, XMMatrixTranspose(proj));
    
    currPassCB->CopyData(0, passConstants);
}

void Labor4App::DrawRenderItems(ID3D12GraphicsCommandList* cmdList)
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

void Labor4App::CreateSrvDescriptorHeap()
{
    // Create descriptor heap for shader resource views
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 3;  // Heightmap + terrain texture + paint texture
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

bool Labor4App::LoadTerrainTexture(const std::wstring& texturePath)
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

bool Labor4App::LoadHeightmapFromFile(const std::wstring& heightmapPath)
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

void Labor4App::InitializePaintTexture()
{
    // Initialize paint texture data (RGBA8, all zeros = transparent)
    mPaintTextureData.resize(mPaintTextureWidth * mPaintTextureHeight * 4, 0);
    
    // Create paint texture resource
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = mPaintTextureWidth;
    texDesc.Height = mPaintTextureHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mPaintTexture)));
    
    // Create upload heap
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mPaintTexture.Get(), 0, 1);
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mPaintTextureUploadHeap)));
    
    // Initialize texture with transparent data
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = mPaintTextureData.data();
    textureData.RowPitch = mPaintTextureWidth * 4;
    textureData.SlicePitch = textureData.RowPitch * mPaintTextureHeight;
    
    UpdateSubresources(mCommandList.Get(), mPaintTexture.Get(), mPaintTextureUploadHeap.Get(),
        0, 0, 1, &textureData);
    
    // Transition to shader resource state
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        mPaintTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    
    // Create SRV for paint texture
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 2, mCbvSrvUavDescriptorSize);
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    
    md3dDevice->CreateShaderResourceView(mPaintTexture.Get(), &srvDesc, srvHandle);
    mPaintTextureSrvHandle = srvHandle;
    
    OutputDebugString(L"Paint texture initialized.\n");
}

bool Labor4App::RaycastTerrainCPU(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDir, DirectX::XMFLOAT3& hitPoint)
{
    // Simplified CPU-side raycast - approximate by finding intersection with terrain plane
    // Since we can't easily sample heightmap on CPU, we'll use ray-marching with height approximation
    // For better accuracy, we'd need to read heightmap data, but for painting this approximation works
    
    const float maxDist = 2000.0f;
    const float stepSize = 5.0f;  // Larger step for CPU (performance vs accuracy tradeoff)
    float currentDist = 0.0f;
    
    XMVECTOR origin = XMLoadFloat3(&rayOrigin);
    XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&rayDir));
    
    while (currentDist < maxDist)
    {
        XMVECTOR testPosV = origin + dir * currentDist;
        XMFLOAT3 testPos;
        XMStoreFloat3(&testPos, testPosV);
        
        // Convert world position to terrain UV coordinates
        float u = (testPos.x / mPassCB.terrainSize) * 0.5f + 0.5f;
        float v = (testPos.z / mPassCB.terrainSize) * 0.5f + 0.5f;
        
        // Check if within terrain bounds
        if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)
        {
            // Approximate terrain height (assume middle of height scale for simplicity)
            // In a full implementation, we'd sample the heightmap texture data here
            // For now, use a simple approximation: assume terrain is mostly flat with slight variation
            float estimatedHeight = mPassCB.heightScale * 0.5f;  // Middle of height range
            
            // Check if ray is below estimated terrain surface
            if (testPos.y <= estimatedHeight + 10.0f)  // Add small tolerance
            {
                hitPoint = testPos;
                hitPoint.y = estimatedHeight;  // Use estimated height
                return true;
            }
        }
        else
        {
            // Out of bounds, no hit
            break;
        }
        
        currentDist += stepSize;
    }
    
    return false;
}

void Labor4App::PaintTerrainAtPosition(const DirectX::XMFLOAT3& worldPos)
{
    // Convert world position to paint texture UV coordinates
    float u = (worldPos.x / mPassCB.terrainSize) * 0.5f + 0.5f;
    float v = (worldPos.z / mPassCB.terrainSize) * 0.5f + 0.5f;
    
    // Clamp to valid range
    u = (std::max)(0.0f, (std::min)(1.0f, u));
    v = (std::max)(0.0f, (std::min)(1.0f, v));
    
    // Convert UV to pixel coordinates
    float centerX = u * mPaintTextureWidth;
    float centerY = v * mPaintTextureHeight;
    
    // Calculate brush radius in pixels
    float brushRadiusPixels = (mBrushRadius / mPassCB.terrainSize) * mPaintTextureWidth;
    
    // Paint a circle at this position
    int minX = static_cast<int>((std::max)(0.0f, centerX - brushRadiusPixels));
    int maxX = static_cast<int>((std::min)(static_cast<float>(mPaintTextureWidth), centerX + brushRadiusPixels));
    int minY = static_cast<int>((std::max)(0.0f, centerY - brushRadiusPixels));
    int maxY = static_cast<int>((std::min)(static_cast<float>(mPaintTextureHeight), centerY + brushRadiusPixels));
    
    for (int y = minY; y < maxY; ++y)
    {
        for (int x = minX; x < maxX; ++x)
        {
            float dx = x - centerX;
            float dy = y - centerY;
            float dist = sqrtf(dx * dx + dy * dy);
            
            if (dist <= brushRadiusPixels)
            {
                // Calculate alpha based on distance (smooth falloff)
                float alpha = 1.0f - (dist / brushRadiusPixels);
                alpha = (std::max)(0.0f, (std::min)(1.0f, alpha));
                
                // Get current pixel
                int pixelIndex = (y * mPaintTextureWidth + x) * 4;
                
                // Blend paint color with existing paint (non-premultiplied alpha blending)
                float existingAlpha = mPaintTextureData[pixelIndex + 3] / 255.0f;
                float newAlpha = alpha * 0.8f;  // Paint opacity
                
                // Standard non-premultiplied alpha blending formula:
                // FinalAlpha = existingAlpha + newAlpha * (1 - existingAlpha)
                // FinalColor = (ExistingColor * existingAlpha + NewColor * newAlpha * (1 - existingAlpha)) / FinalAlpha
                float combinedAlpha = existingAlpha + newAlpha * (1.0f - existingAlpha);
                
                if (combinedAlpha > 0.001f)  // Avoid division by zero
                {
                    // Get existing color (non-premultiplied)
                    float existingR = mPaintTextureData[pixelIndex + 0] / 255.0f;
                    float existingG = mPaintTextureData[pixelIndex + 1] / 255.0f;
                    float existingB = mPaintTextureData[pixelIndex + 2] / 255.0f;
                    
                    // Blend colors using standard alpha blending
                    float r = (existingR * existingAlpha + mPaintColor.x * newAlpha * (1.0f - existingAlpha)) / combinedAlpha;
                    float g = (existingG * existingAlpha + mPaintColor.y * newAlpha * (1.0f - existingAlpha)) / combinedAlpha;
                    float b = (existingB * existingAlpha + mPaintColor.z * newAlpha * (1.0f - existingAlpha)) / combinedAlpha;
                    
                    mPaintTextureData[pixelIndex + 0] = static_cast<UINT8>((std::min)(255.0f, r * 255.0f));
                    mPaintTextureData[pixelIndex + 1] = static_cast<UINT8>((std::min)(255.0f, g * 255.0f));
                    mPaintTextureData[pixelIndex + 2] = static_cast<UINT8>((std::min)(255.0f, b * 255.0f));
                    mPaintTextureData[pixelIndex + 3] = static_cast<UINT8>((std::min)(255.0f, combinedAlpha * 255.0f));
                }
                else
                {
                    // No existing paint, just set new color
                    mPaintTextureData[pixelIndex + 0] = static_cast<UINT8>(mPaintColor.x * 255.0f);
                    mPaintTextureData[pixelIndex + 1] = static_cast<UINT8>(mPaintColor.y * 255.0f);
                    mPaintTextureData[pixelIndex + 2] = static_cast<UINT8>(mPaintColor.z * 255.0f);
                    mPaintTextureData[pixelIndex + 3] = static_cast<UINT8>(newAlpha * 255.0f);
                }
            }
        }
    }
    
    mPaintTextureDirty = true;
}

void Labor4App::UpdatePaintTexture()
{
    if (!mPaintTextureDirty)
        return;
    
    // Update the paint texture on GPU
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = mPaintTextureData.data();
    textureData.RowPitch = mPaintTextureWidth * 4;
    textureData.SlicePitch = textureData.RowPitch * mPaintTextureHeight;
    
    // Transition texture to copy dest
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        mPaintTexture.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COPY_DEST));
    
    UpdateSubresources(mCommandList.Get(), mPaintTexture.Get(), mPaintTextureUploadHeap.Get(),
        0, 0, 1, &textureData);
    
    // Transition back to shader resource
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        mPaintTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    
    mPaintTextureDirty = false;
}

void Labor4App::BuildQuadtree()
{
    // Clear existing [[Quadtree-LOD-system]] quadtree
    mQuadtreeRoot.reset();
    
    // Create root node covering entire terrain
    mQuadtreeRoot = std::make_unique<QuadtreeNode>(mTerrainCenter, mTerrainHalfSize, 0);
    
    // Set maximum [[LOD-selection-algorithm]] LOD levels based on heightmap resolution
    UINT maxLODLevels = CalculateMaxLODLevels();
    
    // Recursively build [[Quadtree-LOD-system]] quadtree
    BuildQuadtreeRecursive(mQuadtreeRoot.get(), maxLODLevels);
    
    OutputDebugString(L"Quadtree construction completed.\n");
}

UINT Labor4App::CalculateMaxLODLevels()
{
    // Increase quadtree depth to split terrain into more tiles
    // For more tiles, we'll use a fixed higher depth
    // This creates more subdivisions and thus more tiles
    return 6;  // Increased from calculated value to create more tiles (was ~2-3 levels)
}

void Labor4App::BuildQuadtreeRecursive(QuadtreeNode* node, UINT maxLevels)
{
    if (node->level >= maxLevels)
    {
        // This is a leaf node - create [[Terrain-tile-generation]] terrain geometry
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

void Labor4App::CreateTerrainTile(QuadtreeNode* node)
{
    // This method creates quad patches for [[GPU-tessellation-system]] GPU tessellation
    // Each patch is a quad with 4 control points
    ID3D12Device* device = md3dDevice.Get();
    
    // Create a grid of patches - each patch is a quad (4 control points)
    // For simplicity, we'll create a single patch per tile
    // More patches = more control, but single patch works well for terrain
    const UINT patchesPerSide = 1;  // 1 patch per tile (can be increased for more control)
    const UINT controlPointsPerPatch = 4;  // Quad patch has 4 control points for [[GPU-tessellation-system]] tessellation
    const UINT totalControlPoints = (patchesPerSide + 1) * (patchesPerSide + 1);
    const UINT totalPatches = patchesPerSide * patchesPerSide;
    
    // Calculate control point positions
    std::vector<DirectX::XMFLOAT3> vertices(totalControlPoints);
    std::vector<UINT> indices(totalPatches * controlPointsPerPatch);
    
    float tileWorldSize = node->halfSize * 2.0f;
    float spacing = tileWorldSize / patchesPerSide;
    
    // Create control points in a grid
    for (UINT z = 0; z <= patchesPerSide; z++)
    {
        for (UINT x = 0; x <= patchesPerSide; x++)
        {
            UINT index = z * (patchesPerSide + 1) + x;
            
            // Calculate world position (height will be sampled in domain shader)
            float worldX = node->center.x - node->halfSize + x * spacing;
            float worldZ = node->center.z - node->halfSize + z * spacing;
            
            vertices[index] = { worldX, 0.0f, worldZ };
        }
    }
    
    // Create patch indices (each patch references 4 control points)
    UINT patchIndex = 0;
    for (UINT z = 0; z < patchesPerSide; z++)
    {
        for (UINT x = 0; x < patchesPerSide; x++)
        {
            UINT topLeft = z * (patchesPerSide + 1) + x;
            UINT topRight = topLeft + 1;
            UINT bottomLeft = (z + 1) * (patchesPerSide + 1) + x;
            UINT bottomRight = bottomLeft + 1;
            
            // Quad patch order for counter-clockwise winding: top-left, bottom-left, bottom-right, top-right
            // This ensures correct front-facing triangles when tessellated
            indices[patchIndex * 4 + 0] = topLeft;
            indices[patchIndex * 4 + 1] = bottomLeft;
            indices[patchIndex * 4 + 2] = bottomRight;
            indices[patchIndex * 4 + 3] = topRight;
            patchIndex++;
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
    
    // Transition vertex buffer to COPY_DEST before copying
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        node->vertexBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST));
    
    // Copy data to default heap
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = vertices.data();
    vertexData.RowPitch = vertexBufferSize;
    vertexData.SlicePitch = vertexBufferSize;
    
    UpdateSubresources(mCommandList.Get(), node->vertexBuffer.Get(), 
                      node->vertexBufferUpload.Get(), 0, 0, 1, &vertexData);
    
    // Transition vertex buffer from COPY_DEST to VERTEX_AND_CONSTANT_BUFFER state
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        node->vertexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
    
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
    
    // Transition index buffer to COPY_DEST before copying
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        node->indexBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST));
    
    // Copy data to default heap
    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = indices.data();
    indexData.RowPitch = indexBufferSize;
    indexData.SlicePitch = indexBufferSize;
    
    UpdateSubresources(mCommandList.Get(), node->indexBuffer.Get(),
                      node->indexBufferUpload.Get(), 0, 0, 1, &indexData);
    
    // Transition index buffer from COPY_DEST to INDEX_BUFFER state
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        node->indexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDEX_BUFFER));
    
    // Set up buffer views
    node->vertexBufferView.BufferLocation = node->vertexBuffer->GetGPUVirtualAddress();
    node->vertexBufferView.StrideInBytes = sizeof(DirectX::XMFLOAT3);
    node->vertexBufferView.SizeInBytes = vertexBufferSize;
    
    node->indexBufferView.BufferLocation = node->indexBuffer->GetGPUVirtualAddress();
    node->indexBufferView.SizeInBytes = indexBufferSize;
    node->indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    
    // Store patch count for rendering
    node->indexCount = totalPatches * controlPointsPerPatch;  // This is the number of indices (4 per patch)
    
    node->vertexCount = totalControlPoints;
    node->indexCount = totalPatches * controlPointsPerPatch;  // Total indices for patches (4 per patch)
    node->needsUpdate = false;
    
    // Note: Skirts are not needed with [[GPU-tessellation-system]] GPU tessellation as the domain shader handles edge continuity
    // The tessellation factors ensure smooth transitions between [[LOD-selection-algorithm]] LOD levels
    
    // Calculate screen space error for this [[LOD-selection-algorithm]] LOD level
    CalculateScreenSpaceError(node);
}

void Labor4App::CalculateScreenSpaceError(QuadtreeNode* node)
{
    // [[LOD-selection-algorithm]] Screen space error calculation based on node size and LOD level
    // Formula: error = (node_size / (2^lod_level)) / screen_resolution * viewport_height
    
    // Get viewport dimensions
    float viewportHeight = static_cast<float>(mScreenViewport.Height);
    
    // Calculate base error (higher levels have smaller error)
    float baseError = node->halfSize * 2.0f;  // Full node size
    
    // Scale by [[LOD-selection-algorithm]] LOD level - each level halves the error
    for (UINT i = 0; i < node->level; i++)
    {
        baseError /= 2.0f;
    }
    
    // Scale by screen size - larger screens can show more detail
    node->screenSpaceError = baseError / 1024.0f * viewportHeight;
    
    // Apply terrain complexity factor based on height variation in this area
    // (This would sample the heightmap to determine roughness)
    float terrainComplexityFactor = 1.0f;  // Placeholder - would be calculated from heightmap data
    
    node->screenSpaceError *= terrainComplexityFactor;
}

void Labor4App::CreateSkirtGeometry(QuadtreeNode* node, UINT verticesPerSide, float tileWorldSize)
{
    // Skirts are additional geometry added to the edges of terrain tiles
    // to hide gaps that appear when adjacent tiles have different LOD levels
    // Skirt extends downward from the edge vertices
    const float skirtHeight = -50.0f;  // Negative Y to extend downward
    
    ID3D12Device* device = md3dDevice.Get();
    float vertexSpacing = tileWorldSize / (verticesPerSide - 1);
    
    // Create skirt vertices for all 4 edges (North, South, East, West)
    // Each edge needs verticesPerSide vertices (top edge) + verticesPerSide vertices (bottom edge of skirt)
    UINT skirtVerticesPerEdge = verticesPerSide;
    UINT totalSkirtVertices = skirtVerticesPerEdge * 4 * 2;  // 4 edges, 2 rows per edge (top and bottom)
    UINT totalSkirtIndices = (skirtVerticesPerEdge - 1) * 4 * 6;  // 2 triangles per quad, 4 edges
    
    std::vector<DirectX::XMFLOAT3> skirtVertices(totalSkirtVertices);
    std::vector<UINT> skirtIndices(totalSkirtIndices);
    
    UINT vertexIndex = 0;
    
    // North edge (top, Z = -halfSize)
    // Top row (edge of terrain)
    for (UINT x = 0; x < skirtVerticesPerEdge; x++)
    {
        float worldX = node->center.x - node->halfSize + x * vertexSpacing;
        float worldZ = node->center.z - node->halfSize;
        skirtVertices[vertexIndex++] = { worldX, 0.0f, worldZ };  // Top edge at terrain level
    }
    // Bottom row (skirt extends downward)
    for (UINT x = 0; x < skirtVerticesPerEdge; x++)
    {
        float worldX = node->center.x - node->halfSize + x * vertexSpacing;
        float worldZ = node->center.z - node->halfSize;
        skirtVertices[vertexIndex++] = { worldX, skirtHeight, worldZ };  // Skirt extends down
    }
    
    // South edge (bottom, Z = +halfSize)
    for (UINT x = 0; x < skirtVerticesPerEdge; x++)
    {
        float worldX = node->center.x - node->halfSize + x * vertexSpacing;
        float worldZ = node->center.z + node->halfSize;
        skirtVertices[vertexIndex++] = { worldX, 0.0f, worldZ };
    }
    for (UINT x = 0; x < skirtVerticesPerEdge; x++)
    {
        float worldX = node->center.x - node->halfSize + x * vertexSpacing;
        float worldZ = node->center.z + node->halfSize;
        skirtVertices[vertexIndex++] = { worldX, skirtHeight, worldZ };
    }
    
    // West edge (left, X = -halfSize)
    for (UINT z = 0; z < skirtVerticesPerEdge; z++)
    {
        float worldX = node->center.x - node->halfSize;
        float worldZ = node->center.z - node->halfSize + z * vertexSpacing;
        skirtVertices[vertexIndex++] = { worldX, 0.0f, worldZ };
    }
    for (UINT z = 0; z < skirtVerticesPerEdge; z++)
    {
        float worldX = node->center.x - node->halfSize;
        float worldZ = node->center.z - node->halfSize + z * vertexSpacing;
        skirtVertices[vertexIndex++] = { worldX, skirtHeight, worldZ };
    }
    
    // East edge (right, X = +halfSize)
    for (UINT z = 0; z < skirtVerticesPerEdge; z++)
    {
        float worldX = node->center.x + node->halfSize;
        float worldZ = node->center.z - node->halfSize + z * vertexSpacing;
        skirtVertices[vertexIndex++] = { worldX, 0.0f, worldZ };
    }
    for (UINT z = 0; z < skirtVerticesPerEdge; z++)
    {
        float worldX = node->center.x + node->halfSize;
        float worldZ = node->center.z - node->halfSize + z * vertexSpacing;
        skirtVertices[vertexIndex++] = { worldX, skirtHeight, worldZ };
    }
    
    // Create indices for skirts (quads connecting top edge to bottom skirt)
    UINT index = 0;
    UINT baseIndex = 0;
    
    // North edge skirt
    for (UINT x = 0; x < skirtVerticesPerEdge - 1; x++)
    {
        UINT topLeft = baseIndex + x;
        UINT topRight = baseIndex + x + 1;
        UINT bottomLeft = baseIndex + skirtVerticesPerEdge + x;
        UINT bottomRight = baseIndex + skirtVerticesPerEdge + x + 1;
        
        skirtIndices[index++] = topLeft;
        skirtIndices[index++] = bottomLeft;
        skirtIndices[index++] = topRight;
        
        skirtIndices[index++] = topRight;
        skirtIndices[index++] = bottomLeft;
        skirtIndices[index++] = bottomRight;
    }
    
    // South edge skirt
    baseIndex = skirtVerticesPerEdge * 2;
    for (UINT x = 0; x < skirtVerticesPerEdge - 1; x++)
    {
        UINT topLeft = baseIndex + x;
        UINT topRight = baseIndex + x + 1;
        UINT bottomLeft = baseIndex + skirtVerticesPerEdge + x;
        UINT bottomRight = baseIndex + skirtVerticesPerEdge + x + 1;
        
        skirtIndices[index++] = topLeft;
        skirtIndices[index++] = bottomLeft;
        skirtIndices[index++] = topRight;
        
        skirtIndices[index++] = topRight;
        skirtIndices[index++] = bottomLeft;
        skirtIndices[index++] = bottomRight;
    }
    
    // West edge skirt
    baseIndex = skirtVerticesPerEdge * 4;
    for (UINT z = 0; z < skirtVerticesPerEdge - 1; z++)
    {
        UINT topLeft = baseIndex + z;
        UINT topRight = baseIndex + z + 1;
        UINT bottomLeft = baseIndex + skirtVerticesPerEdge + z;
        UINT bottomRight = baseIndex + skirtVerticesPerEdge + z + 1;
        
        skirtIndices[index++] = topLeft;
        skirtIndices[index++] = bottomLeft;
        skirtIndices[index++] = topRight;
        
        skirtIndices[index++] = topRight;
        skirtIndices[index++] = bottomLeft;
        skirtIndices[index++] = bottomRight;
    }
    
    // East edge skirt
    baseIndex = skirtVerticesPerEdge * 6;
    for (UINT z = 0; z < skirtVerticesPerEdge - 1; z++)
    {
        UINT topLeft = baseIndex + z;
        UINT topRight = baseIndex + z + 1;
        UINT bottomLeft = baseIndex + skirtVerticesPerEdge + z;
        UINT bottomRight = baseIndex + skirtVerticesPerEdge + z + 1;
        
        skirtIndices[index++] = topLeft;
        skirtIndices[index++] = bottomLeft;
        skirtIndices[index++] = topRight;
        
        skirtIndices[index++] = topRight;
        skirtIndices[index++] = bottomLeft;
        skirtIndices[index++] = bottomRight;
    }
    
    // Create vertex buffer for skirts
    const UINT skirtVertexBufferSize = static_cast<UINT>(skirtVertices.size() * sizeof(DirectX::XMFLOAT3));
    
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(skirtVertexBufferSize);
    
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&node->skirtVertexBuffer)
    ));
    
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(skirtVertexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&node->skirtVertexBufferUpload)
    ));
    
    UINT8* vertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(node->skirtVertexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin)));
    memcpy(vertexDataBegin, skirtVertices.data(), skirtVertexBufferSize);
    node->skirtVertexBufferUpload->Unmap(0, nullptr);
    
    // Transition skirt vertex buffer to COPY_DEST before copying
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        node->skirtVertexBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST));
    
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = skirtVertices.data();
    vertexData.RowPitch = skirtVertexBufferSize;
    vertexData.SlicePitch = skirtVertexBufferSize;
    
    UpdateSubresources(mCommandList.Get(), node->skirtVertexBuffer.Get(),
                      node->skirtVertexBufferUpload.Get(), 0, 0, 1, &vertexData);
    
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        node->skirtVertexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
    
    // Create index buffer for skirts
    const UINT skirtIndexBufferSize = static_cast<UINT>(skirtIndices.size() * sizeof(UINT));
    
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(skirtIndexBufferSize);
    
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&node->skirtIndexBuffer)
    ));
    
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(skirtIndexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&node->skirtIndexBufferUpload)
    ));
    
    UINT8* indexDataBegin;
    ThrowIfFailed(node->skirtIndexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&indexDataBegin)));
    memcpy(indexDataBegin, skirtIndices.data(), skirtIndexBufferSize);
    node->skirtIndexBufferUpload->Unmap(0, nullptr);
    
    // Transition skirt index buffer to COPY_DEST before copying
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        node->skirtIndexBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST));
    
    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = skirtIndices.data();
    indexData.RowPitch = skirtIndexBufferSize;
    indexData.SlicePitch = skirtIndexBufferSize;
    
    UpdateSubresources(mCommandList.Get(), node->skirtIndexBuffer.Get(),
                      node->skirtIndexBufferUpload.Get(), 0, 0, 1, &indexData);
    
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        node->skirtIndexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDEX_BUFFER));
    
    // Set up buffer views
    node->skirtVertexBufferView.BufferLocation = node->skirtVertexBuffer->GetGPUVirtualAddress();
    node->skirtVertexBufferView.StrideInBytes = sizeof(DirectX::XMFLOAT3);
    node->skirtVertexBufferView.SizeInBytes = skirtVertexBufferSize;
    
    node->skirtIndexBufferView.BufferLocation = node->skirtIndexBuffer->GetGPUVirtualAddress();
    node->skirtIndexBufferView.SizeInBytes = skirtIndexBufferSize;
    node->skirtIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
    
    node->skirtVertexCount = totalSkirtVertices;
    node->skirtIndexCount = totalSkirtIndices;
}

void Labor4App::ResetRenderFlags(QuadtreeNode* node)
{
    if (!node) return;
    node->shouldRender = false;
    node->isVisible = false;
    for (auto& child : node->children)
    {
        if (child) ResetRenderFlags(child.get());
    }
}

void Labor4App::SelectLODLevels()
{
    // Reset all render flags first
    ResetRenderFlags(mQuadtreeRoot.get());
    
    // Start [[LOD-selection-algorithm]] LOD selection from root node
    SelectLODRecursive(mQuadtreeRoot.get(), false);
}

void Labor4App::SelectLODRecursive(QuadtreeNode* node, bool parentVisible)
{
    if (!node)
        return;
    
    // Check visibility for THIS node individually using [[Frustum-culling-module]] frustum culling
    bool isVisible = IsNodeVisible(node);
    node->isVisible = isVisible;
    
    if (!isVisible)
    {
        // If not visible, mark as not rendering and don't process children
        node->shouldRender = false;
        return;
    }
    
    // Get camera distance to node center using 2D distance (X, Z only - no Y)
    // This makes [[LOD-selection-algorithm]] LOD transitions more obvious and easier to test
    // NOTE: Camera coordinate system has X/Z swapped relative to terrain coordinate system
    // Camera's forward/back (Z) maps to terrain's left/right (X), and vice versa
    DirectX::XMFLOAT3 cameraPos = mPassCB.cameraPosition;
    float dx = cameraPos.z - node->center.x;  // Camera Z (forward/back) -> Terrain X (left/right)
    float dz = cameraPos.x - node->center.z;  // Camera X (left/right) -> Terrain Z (forward/back)
    float distance = sqrtf(dx * dx + dz * dz);  // 2D distance only
    
    // Improved [[LOD-selection-algorithm]] LOD calculation: closer nodes should subdivide for more detail
    // Calculate threshold based on node size and level
    // Smaller nodes (deeper levels) need to be closer to subdivide
    // Base threshold: larger nodes can be subdivided from further away
    float nodeSize = node->halfSize * 2.0f;  // Full size of the node
    float baseThreshold = nodeSize * 2.0f;  // Base distance threshold
    
    // Deeper levels (higher level number) need to be closer to subdivide
    // Level 0: threshold = baseThreshold
    // Level 1: threshold = baseThreshold * 0.5
    // Level 2: threshold = baseThreshold * 0.25
    // etc.
    float levelMultiplier = 1.0f / (1.0f + node->level * 0.5f);
    float lodDistanceThreshold = baseThreshold * levelMultiplier;
    
    // Check if this node should subdivide (use children) or render itself
    bool useThisNode = true;
    
    if (node->HasChildren())
    {
        // If we're close enough, subdivide to get more detail
        // Closer = higher detail needed = subdivide
        if (distance < lodDistanceThreshold)
        {
            // Close enough - use children for more detail
            useThisNode = false;
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
        // Process each child (each child will check its own visibility)
        for (auto& child : node->children)
        {
            if (child)
            {
                SelectLODRecursive(child.get(), false);  // Pass false, each child checks its own visibility
            }
        }
    }
}

bool Labor4App::IsNodeVisible(const QuadtreeNode* node) const
{
    if (!node)
        return false;
    
    // Only cull if [[Frustum-culling-module]] frustum culling is enabled
    if (!mFrustumCullingEnabled)
        return true;
    
    // Create bounding box for this node (more accurate than sphere for terrain tiles)
    DirectX::BoundingBox boundingBox;
    boundingBox.Center = node->center;
    boundingBox.Extents = DirectX::XMFLOAT3(node->halfSize, 100.0f, node->halfSize);  // Height of 100 for terrain
    
    // Check against [[Frustum-culling-module]] frustum - only cull if completely outside (DISJOINT)
    DirectX::ContainmentType containment = mCameraFrustum.Contains(boundingBox);
    
    // Return true if not completely outside (INTERSECTS or CONTAINS)
    return containment != DirectX::DISJOINT;
}

void Labor4App::RenderQuadtreeNodes(ID3D12GraphicsCommandList* cmdList, QuadtreeNode* node)
{
    if (!node || !node->isVisible)
        return;
    
    // If this node should be rendered ([[LOD-selection-algorithm]] LOD selected it)
    if (node->shouldRender)
    {
        // This is a leaf node - render it
        if (node->vertexBuffer && node->indexBuffer)
        {
            // Set vertex and index buffers
            cmdList->IASetVertexBuffers(0, 1, &node->vertexBufferView);
            cmdList->IASetIndexBuffer(&node->indexBufferView);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);  // Patch topology for [[GPU-tessellation-system]] tessellation
            
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
            
            // Draw patches - indexCount is total indices (4 per patch)
            // DrawIndexedInstanced with patch topology: first param is index count (4 per patch)
            cmdList->DrawIndexedInstanced(node->indexCount, 1, 0, 0, 0);
            
            // Note: Skirts not needed with [[GPU-tessellation-system]] GPU tessellation - domain shader handles continuity
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

void Labor4App::OnMouseDown(WPARAM btnState, int x, int y)
{
    if((btnState & MK_RBUTTON) != 0)
    {
        mRightMouseDown = true;
        mLastMousePos.x = x;
        mLastMousePos.y = y;
        SetCapture(mhMainWnd);
    }
    if((btnState & MK_LBUTTON) != 0)
    {
        mLeftMouseDown = true;
        mLastMousePos.x = x;
        mLastMousePos.y = y;
    }
}

void Labor4App::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
    mRightMouseDown = false;
    if((btnState & MK_LBUTTON) == 0)  // Left button released
    {
        mLeftMouseDown = false;
    }
}

void Labor4App::OnMouseMove(WPARAM btnState, int x, int y)
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
    
    // Handle terrain painting with left mouse button
    if(mLeftMouseDown)
    {
        // Convert mouse position to NDC
        float mouseX = static_cast<float>(x);
        float mouseY = static_cast<float>(y);
        float ndcX = (mouseX / mClientWidth) * 2.0f - 1.0f;
        float ndcY = 1.0f - (mouseY / mClientHeight) * 2.0f;
        
        // Reconstruct ray from screen position using current view/projection matrices
        XMMATRIX view = mCamera.GetView();
        XMMATRIX proj = mCamera.GetProj();
        XMMATRIX invView = XMMatrixInverse(nullptr, view);
        XMMATRIX invProj = XMMatrixInverse(nullptr, proj);
        
        // Camera position
        XMFLOAT3 camPos;
        XMStoreFloat3(&camPos, mCamera.GetPosition());
        
        // NDC to view space (far plane)
        XMVECTOR screenPos = XMVectorSet(ndcX, ndcY, 1.0f, 1.0f);
        XMVECTOR viewPos = XMVector4Transform(screenPos, invProj);
        viewPos = XMVectorScale(viewPos, 1.0f / XMVectorGetW(viewPos));
        
        // View space ray direction (from origin to far plane point)
        XMVECTOR viewDir = XMVector3Normalize(viewPos);
        
        // Transform view direction to world space
        XMMATRIX invViewRotation = invView;
        invViewRotation.r[3] = XMVectorSet(0, 0, 0, 1);  // Remove translation, keep rotation
        XMVECTOR worldDir = XMVector3Normalize(XMVector3TransformNormal(viewDir, invViewRotation));
        
        XMFLOAT3 rayDir;
        XMStoreFloat3(&rayDir, worldDir);
        
        // Raycast to terrain
        XMFLOAT3 hitPoint;
        if (RaycastTerrainCPU(camPos, rayDir, hitPoint))
        {
            PaintTerrainAtPosition(hitPoint);
        }
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void Labor4App::OnKeyPressed(const GameTimer& gt, WPARAM key)
{
    // WASD movement is handled in Update() via GetAsyncKeyState
    // This method is called for key down events, but we're handling continuous
    // movement in Update() instead for smoother control
    
    // 'C' key to update [[Frustum-culling-module]] frustum culling (for testing)
    if (key == 'C' || key == 'c')
    {
        mFrustumNeedsUpdate = true;
        OutputDebugString(L"Frustum culling update triggered by 'C' key.\n");
    }
}

void Labor4App::InitializeAtmosphere()
{
    // Initialize default atmosphere parameters
    mAtmosphereSettings.CameraPos = { 0, 0, 0 };
    // Sun direction: points FROM sun TO planet (normalized)
    // For a sun in the sky, we want it coming from above and to the side
    mAtmosphereSettings.SunDirection = { 0.3f, -0.8f, 0.5f }; // Sun from upper-right
    XMVECTOR sunDir = XMLoadFloat3(&mAtmosphereSettings.SunDirection);
    sunDir = XMVector3Normalize(sunDir);
    XMStoreFloat3(&mAtmosphereSettings.SunDirection, sunDir);
    
    // Smaller planet/atmosphere for better visualization
    mAtmosphereSettings.PlanetCenter = { 0, -1000.0f, 0 }; // Planet center
    mAtmosphereSettings.PlanetRadius = 100.0f; // Planet radius
    mAtmosphereSettings.AtmosphereRadius = 110.0f; // Atmosphere radius (100 units above planet)
    // Realistic Rayleigh scattering coefficients based on GPU Gems 2 Chapter 16
    // Reference: https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-16-accurate-atmospheric-scattering
    // Scaled for rendering (original values are 5.8e-6, 1.35e-5, 3.31e-5)
    // Blue is strongest (why sky is blue), red is weakest
    mAtmosphereSettings.RayleighScattering = { 0.0058f, 0.0135f, 0.0331f }; // RGB scattering coefficients
    mAtmosphereSettings.MieScattering = { 0.0021f, 0.0021f, 0.0021f }; // Increased from 0.000399 for more visible effect
    mAtmosphereSettings.MieG = -0.75f; // Negative for aerosols (GPU Gems 2: -0.75 to -0.999)
    mAtmosphereSettings.SunIntensity = 20.0f;
    mAtmosphereSettings.AtmosphereMode = 0; // Default to Hoffman-Preetham
    mAtmosphereSettings.DensityMultiplier = 1.0f;
    mAtmosphereSettings.PollutionLevel = 0.1f; // Reduced pollution for cleaner, bluer sky
    mAtmosphereSettings.SunAngularRadius = 0.035f; // ~2 degrees (much more visible than 0.27 degrees)
    mAtmosphereSettings.CameraAltitudeDisplacement = 0.0f; // No displacement by default
    // Exponential Height Fog defaults
    mAtmosphereSettings.FogHeight = 0.0f; // Fog at ground level
    mAtmosphereSettings.FogDensity = 0.05f; // Moderate fog density
    mAtmosphereSettings.FogHeightFalloff = 0.2f; // Moderate height falloff
    mAtmosphereSettings.MinFogOpacity = 0.0f; // No minimum opacity
    mAtmosphereSettings.FogColor = { 0.9f, 0.95f, 1.0f }; // Light blue fog color
    mAtmosphereSettings.EnableFog = 1; // Enable fog by default (1 = enabled)
    
    // Build atmosphere components
    BuildAtmosphereRootSignature();
    BuildAtmosphereShaders();
    BuildSkyDomeGeometry();
    BuildAtmospherePSO();
    
    // Create constant buffer
    mAtmosphereCB = std::make_unique<UploadBuffer<AtmosphereParams>>(md3dDevice.Get(), 1, true);
    
    // Create terrain atmosphere constant buffer
    mTerrainAtmosphereCB = std::make_unique<UploadBuffer<TerrainAtmosphereConstants>>(md3dDevice.Get(), 1, true);
    UpdateTerrainAtmosphereCB();
}

void Labor4App::BuildAtmosphereRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];
    slotRootParameter[0].InitAsConstantBufferView(0); // Atmosphere constants (b0)
    
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter,
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
    
    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mAtmosphereRootSignature.GetAddressOf())));
}

void Labor4App::BuildAtmosphereShaders()
{
    mShaders["atmosphereVS"] = d3dUtil::CompileShader(L"Shaders\\Atmosphere.hlsl", nullptr, "VS_Main", "vs_5_1");
    mShaders["atmospherePS"] = d3dUtil::CompileShader(L"Shaders\\Atmosphere.hlsl", nullptr, "PS_Main", "ps_5_1");
}

void Labor4App::BuildSkyDomeGeometry()
{
    GeometryGenerator geoGen;
    // Sky dome radius - smaller to avoid culling issues
    // Using 500.0f radius ensures it's always visible and not culled
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(500.0f, 20, 40); // Sky dome sphere
    
    std::vector<DirectX::XMFLOAT3> vertices;
    vertices.reserve(sphere.Vertices.size());
    for (const auto& v : sphere.Vertices)
    {
        vertices.push_back(v.Position);
    }
    
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(DirectX::XMFLOAT3);
    
    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "skyDomeGeo";
    
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
    
    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    
    geo->VertexByteStride = sizeof(DirectX::XMFLOAT3);
    geo->VertexBufferByteSize = vbByteSize;
    
    // Convert indices
    std::vector<std::uint16_t> indices16 = sphere.GetIndices16();
    const UINT ibByteSize = (UINT)indices16.size() * sizeof(std::uint16_t);
    
    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices16.data(), ibByteSize);
    
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices16.data(), ibByteSize, geo->IndexBufferUploader);
    
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;
    
    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices16.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    
    geo->DrawArgs["skyDome"] = submesh;
    
    mGeometries[geo->Name] = std::move(geo);
    mSkyDomeGeo = mGeometries["skyDomeGeo"].get();
}

void Labor4App::BuildAtmospherePSO()
{
    std::vector<D3D12_INPUT_ELEMENT_DESC> atmosphereInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { atmosphereInputLayout.data(), (UINT)atmosphereInputLayout.size() };
    psoDesc.pRootSignature = mAtmosphereRootSignature.Get();
    psoDesc.VS = 
    { 
        reinterpret_cast<BYTE*>(mShaders["atmosphereVS"]->GetBufferPointer()), 
        mShaders["atmosphereVS"]->GetBufferSize()
    };
    psoDesc.PS = 
    { 
        reinterpret_cast<BYTE*>(mShaders["atmospherePS"]->GetBufferPointer()),
        mShaders["atmospherePS"]->GetBufferSize()
    };
    // Rasterizer state: disable culling for sky dome (we're rendering the inside of the sphere)
    // Sky dome sphere has outward-facing normals, but we're inside looking out
    // Disable culling to ensure all faces render correctly
    D3D12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // Disable culling for sky dome
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    
    // Depth stencil state: disable depth test and write for sky dome
    // Sky dome should always render as background, so we disable depth testing
    // This ensures the sky is always visible behind terrain
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depthStencilDesc.DepthEnable = FALSE; // Disable depth test for sky dome
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write to depth buffer
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS; // Always pass (not used since DepthEnable is FALSE)
    psoDesc.DepthStencilState = depthStencilDesc;
    
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["atmosphere"])));
}

void Labor4App::UpdateAtmosphereCB()
{
    // Update camera position with altitude displacement
    // This allows simulating higher altitude views for better atmospheric scattering
    XMFLOAT3 camPos = mCamera.GetPosition3f();
    camPos.y += mAtmosphereSettings.CameraAltitudeDisplacement; // Add artificial altitude
    mAtmosphereSettings.CameraPos = camPos;
    
    // Update view and projection matrices
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();
    XMStoreFloat4x4(&mAtmosphereSettings.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mAtmosphereSettings.Projection, XMMatrixTranspose(proj));
    
    // Update constant buffer
    mAtmosphereCB->CopyData(0, mAtmosphereSettings);
    
    // Also update terrain atmosphere constant buffer
    UpdateTerrainAtmosphereCB();
}

void Labor4App::UpdateTerrainAtmosphereCB()
{
    TerrainAtmosphereConstants terrainAtm;
    terrainAtm.sunDirection = mAtmosphereSettings.SunDirection;
    terrainAtm.atmosphereRadius = mAtmosphereSettings.AtmosphereRadius;
    terrainAtm.planetRadius = mAtmosphereSettings.PlanetRadius;
    terrainAtm.pollutionLevel = mAtmosphereSettings.PollutionLevel;
    terrainAtm.densityMultiplier = mAtmosphereSettings.DensityMultiplier;
    terrainAtm.atmosphereMode = mAtmosphereSettings.AtmosphereMode;
    terrainAtm.SunIntensity = mAtmosphereSettings.SunIntensity;
    // Copy fog parameters to terrain constant buffer
    terrainAtm.FogHeight = mAtmosphereSettings.FogHeight;
    terrainAtm.FogDensity = mAtmosphereSettings.FogDensity;
    terrainAtm.FogHeightFalloff = mAtmosphereSettings.FogHeightFalloff;
    terrainAtm.MinFogOpacity = mAtmosphereSettings.MinFogOpacity;
    terrainAtm.FogColor = mAtmosphereSettings.FogColor;
    terrainAtm.paddingFog0 = 0.0f;
    terrainAtm.EnableFog = mAtmosphereSettings.EnableFog;
    terrainAtm.paddingFog1[0] = 0.0f;
    terrainAtm.paddingFog1[1] = 0.0f;
    terrainAtm.paddingFog1[2] = 0.0f;
    
    mTerrainAtmosphereCB->CopyData(0, terrainAtm);
}

void Labor4App::RenderAtmosphere(ID3D12GraphicsCommandList* cmdList)
{
    if (!mEnableAtmosphere || !mSkyDomeGeo)
        return;
    
    // Update atmosphere constant buffer
    UpdateAtmosphereCB();
    
    // Set root signature
    cmdList->SetGraphicsRootSignature(mAtmosphereRootSignature.Get());
    
    // Set constant buffer
    auto atmosphereCB = mAtmosphereCB->Resource();
    cmdList->SetGraphicsRootConstantBufferView(0, atmosphereCB->GetGPUVirtualAddress());
    
    // Set pipeline state
    cmdList->SetPipelineState(mPSOs["atmosphere"].Get());
    
    // Set vertex and index buffers
    auto geo = mSkyDomeGeo;
    cmdList->IASetVertexBuffers(0, 1, &geo->VertexBufferView());
    cmdList->IASetIndexBuffer(&geo->IndexBufferView());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    // Draw sky dome
    auto drawArgs = geo->DrawArgs["skyDome"];
    cmdList->DrawIndexedInstanced(drawArgs.IndexCount, 1, drawArgs.StartIndexLocation, drawArgs.BaseVertexLocation, 0);
}

void Labor4App::RenderAtmosphereGUI()
{
    // This function should be called from your IMGUI rendering code
    // Example usage (add this where you render IMGUI windows, typically before ImGui::Render()):
    // ImGui::Begin("Atmosphere Controls");
    // RenderAtmosphereGUI();
    // ImGui::End();
    
    if (ImGui::CollapsingHeader("Atmosphere Settings"))
    {
        ImGui::Checkbox("Enable Atmosphere", &mEnableAtmosphere);
        
        ImGui::Text("Rendering Mode:");
        const char* modes[] = { "Hoffman-Preetham (Ground Level)", "Ray Marching (High Altitude)" };
        ImGui::Combo("Atmosphere Mode", &mAtmosphereSettings.AtmosphereMode, modes, 2);
        
        ImGui::Separator();
        ImGui::Text("Environmental Parameters:");
        
        ImGui::SliderFloat("Pollution Level", &mAtmosphereSettings.PollutionLevel, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Density Multiplier", &mAtmosphereSettings.DensityMultiplier, 0.1f, 10.0f, "%.2f");
        // Increased range for sun intensity to make it more influential
        ImGui::SliderFloat("Sun Intensity", &mAtmosphereSettings.SunIntensity, 0.0f, 2000.0f, "%.1f");
        
        ImGui::Separator();
        ImGui::Text("Scattering Parameters:");
        
        // Increased ranges for more visible effects (based on GPU Gems 2)
        ImGui::SliderFloat3("Rayleigh Scattering", &mAtmosphereSettings.RayleighScattering.x, 0.0f, 1.1f, "%.5f");
        ImGui::SliderFloat3("Mie Scattering", &mAtmosphereSettings.MieScattering.x, 0.0f, 1.01f, "%.5f");
        // Mie G for aerosols should be negative (-0.75 to -0.999 per GPU Gems 2)
        ImGui::SliderFloat("Mie G (Phase)", &mAtmosphereSettings.MieG, -0.99f, 0.0f, "%.3f");
        
        ImGui::Separator();
        ImGui::Text("Camera Settings:");    
        // Camera altitude displacement for better atmospheric calculations at higher altitudes
        ImGui::SliderFloat("Camera Altitude Displacement (m)", &mAtmosphereSettings.CameraAltitudeDisplacement, 0.0f, 100000.0f, "%.0f");
        ImGui::Text("(Increases effective camera height for atmospheric calculations)");
        
        ImGui::Separator();
        ImGui::Text("Physical Parameters:");
        
        ImGui::SliderFloat("Planet Radius", &mAtmosphereSettings.PlanetRadius, 100.0f, 5000.0f, "%.0f");
        ImGui::SliderFloat("Atmosphere Radius", &mAtmosphereSettings.AtmosphereRadius, 200.0f, 6000.0f, "%.0f");
        
        ImGui::Separator();
        ImGui::Text("Sun Direction:");
        ImGui::Checkbox("Animate Sun Direction Y", &mAnimateSunDirection);
        ImGui::Text("(Animates sun Y from -1 to 1 on sine wave)");
        ImGui::SliderFloat3("Sun Direction", &mAtmosphereSettings.SunDirection.x, -1.0f, 1.0f, "%.2f");
        // Increased range for sun angular radius (larger sun is more visible)
        ImGui::SliderFloat("Sun Angular Radius (rad)", &mAtmosphereSettings.SunAngularRadius, 0.01f, 0.1f, "%.4f");
        ImGui::Text("(Larger values = bigger sun disk, default ~2 degrees = 0.035)");
        
        ImGui::Separator();
        ImGui::Text("Exponential Height Fog:");
        bool enableFogBool = mAtmosphereSettings.EnableFog != 0;
        if (ImGui::Checkbox("Enable Fog", &enableFogBool))
        {
            mAtmosphereSettings.EnableFog = enableFogBool ? 1 : 0;
        }
        ImGui::SliderFloat("Fog Height", &mAtmosphereSettings.FogHeight, -100.0f, 100.0f, "%.1f");
        ImGui::ColorEdit3("Fog Color", &mAtmosphereSettings.FogColor.x);
        ImGui::SliderFloat("Fog Density", &mAtmosphereSettings.FogDensity, 0.0f, 0.5f, "%.3f");
        ImGui::Text("(Higher = more fog, default 0.05)");
        ImGui::SliderFloat("Fog Height Falloff", &mAtmosphereSettings.FogHeightFalloff, 0.0f, 1.0f, "%.3f");
        ImGui::Text("(Higher = fog decreases faster with altitude)");
        ImGui::SliderFloat("Min Fog Opacity", &mAtmosphereSettings.MinFogOpacity, 0.0f, 1.0f, "%.2f");
        
        // Normalize sun direction
        XMVECTOR sunDir = XMLoadFloat3(&mAtmosphereSettings.SunDirection);
        sunDir = XMVector3Normalize(sunDir);
        XMStoreFloat3(&mAtmosphereSettings.SunDirection, sunDir);
        
        // Update terrain atmosphere constant buffer when parameters change
        UpdateTerrainAtmosphereCB();
        
        // Presets for clean/dirty atmosphere (updated with GPU Gems 2 values)
        if (ImGui::Button("Clean Atmosphere (Mountain)"))
        {
            mAtmosphereSettings.PollutionLevel = 0.1f;
            mAtmosphereSettings.DensityMultiplier = 0.8f;
            mAtmosphereSettings.RayleighScattering = {0.0058f, 0.0135f, 0.0331f};
            mAtmosphereSettings.MieScattering = {0.0021f, 0.0021f, 0.0021f};
            mAtmosphereSettings.MieG = -0.75f;
            mAtmosphereSettings.CameraAltitudeDisplacement = 0.0f;
            UpdateTerrainAtmosphereCB();
        }
        
        if (ImGui::Button("Dirty Atmosphere (City)"))
        {
            mAtmosphereSettings.PollutionLevel = 1.5f;
            mAtmosphereSettings.DensityMultiplier = 2.0f;
            mAtmosphereSettings.RayleighScattering = {0.004f, 0.01f, 0.025f};
            mAtmosphereSettings.MieScattering = {0.005f, 0.005f, 0.005f};
            mAtmosphereSettings.MieG = -0.85f;
            mAtmosphereSettings.CameraAltitudeDisplacement = 0.0f;
            UpdateTerrainAtmosphereCB();
        }
        
        if (ImGui::Button("Space View (High Altitude)"))
        {
            mAtmosphereSettings.AtmosphereMode = 1; // Ray Marching
            mAtmosphereSettings.PollutionLevel = 0.0f;
            mAtmosphereSettings.DensityMultiplier = 1.0f;
            mAtmosphereSettings.CameraAltitudeDisplacement = 50000.0f; // High altitude
            UpdateTerrainAtmosphereCB();
        }
    }
}
