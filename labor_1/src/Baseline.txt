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
    void BuildCubeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildRenderItems();
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdatePassCB(const GameTimer& gt);
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList);

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

    // Camera
    Camera mCamera;
    POINT mLastMousePos;
    bool mRightMouseDown = false;
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

    // Initialize camera
    mCamera.SetPosition(0.0f, 2.0f, -5.0f);
    mCamera.LookAt(XMVectorSet(0.0f, 2.0f, -5.0f, 0.0f), 
                   XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), 
                   XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    mCamera.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    mCamera.UpdateViewMatrix();

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
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];

    slotRootParameter[0].InitAsConstantBufferView(0); // Object constants (b0)
    slotRootParameter[1].InitAsConstantBufferView(1); // Pass constants (b1)

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter,
        0, nullptr,
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
    PassConstants passConstants;
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
