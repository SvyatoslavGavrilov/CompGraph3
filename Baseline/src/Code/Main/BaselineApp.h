#pragma once

#include "../../Common/Camera.h"
#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"

#include "FrameResource.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

constexpr int gNumFrameResources = 3;

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
};

struct RenderItem
{
    RenderItem() = default;
    RenderItem(const RenderItem&) = delete;
    RenderItem& operator=(const RenderItem&) = delete;

    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    int NumFramesDirty = gNumFrameResources;

    UINT ObjCBIndex = 0;
    MeshGeometry* Geo = nullptr;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class BaselineApp final : public D3DApp
{
public:
    explicit BaselineApp(HINSTANCE hInstance);
    BaselineApp(const BaselineApp&) = delete;
    BaselineApp& operator=(const BaselineApp&) = delete;
    ~BaselineApp() override;

    bool Initialize() override;

private:
    void OnResize() override;
    void Update(const GameTimer& gt) override;
    void DeferredDraw(const GameTimer& gt) override;
    void Draw(const GameTimer& gt) override;

    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;
    void OnMouseMove(WPARAM btnState, int x, int y) override;

    void MoveBackFwd(float step) override;
    void MoveLeftRight(float step) override;
    void MoveUpDown(float step) override;

    void OnKeyPressed(const GameTimer& gt, WPARAM key) override;
    void OnKeyReleased(const GameTimer& gt, WPARAM key) override;
    std::wstring GetCamSpeed() override;

    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildGeometry();
    void BuildRenderItems();
    void BuildFrameResources();
    void BuildPSOs();

    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

private:
    Camera mCamera;

    UINT mCbvSrvDescriptorSize = 0;

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;
    std::vector<RenderItem*> mOpaqueRitems;

    PassConstants mMainPassCB;
    POINT mLastMousePos = {};
};