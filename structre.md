## Template Rendering Architecture

- **Entry Flow**: `WinMain` in `Template2/src/Code/Main/TexColumnsApp.cpp` boots `TexColumnsApp`, which inherits from `D3DApp` in `Template2/src/Common/d3dApp.h` and drives the Direct3D 12 message loop.
- **Rendering Model**: The app implements a deferred shading pipeline with a geometry pass (`BuildRootSignature`, `BuildPSOs`, `DrawRenderItems`), a shadow pass (`BuildShadowPassRootSignature`, `DrawSceneToShadowMap`), a lighting combine pass (`BuildLightingRootSignature`, `DeferredDraw`), and an optional post-process stage (`BuildPostProcessRootSignature`, `DeferredDraw` tail).
- **Resource Management**: `FrameResource` objects wrap per-frame command allocators, constant buffers, and light/shadow buffers to support triple buffering (`gNumFrameResources = 3`). Descriptor heaps collect SRVs for textures, G-buffer outputs, and per-light shadow maps.
- **Geometry & Materials**: Procedural primitives (`BuildShapeGeometry`) and Assimp-loaded meshes (`BuildCustomMeshGeometry`) share a single `MeshGeometry`; materials are created through `CreateMaterial` and stored in `mMaterials`.
- **Texture System**: `LoadAllTextures` enumerates `Textures/` and stages assets via `DirectX::CreateDDSTextureFromFile12`, caching descriptors in `TexOffsets` for material binding.
- **Lighting System**: `BuildLights` seeds ambient, point, spot, and directional lights; `UpdateLightCBs` updates GPU buffers, computes shadow matrices, and exposes ImGui controls per light.
- **Input & Camera**: `Camera` in `Template2/src/Common/Camera.{h,cpp}` provides movement; `OnKeyPressed`, `MoveBackFwd`, `MoveLeftRight`, `MoveUpDown`, and mouse handlers update camera orientation.
- **User Interface**: Dear ImGui is integrated via `imgui_impl_dx12`/`imgui_impl_win32` for runtime tweaking of transforms, materials, and lighting parameters.

## Include Inventory

```
../../Common/Camera.h
../../Common/GeometryGenerator.h
../../Common/MathHelper.h
../../Common/UploadBuffer.h
../../Common/d3dApp.h
../../Common/d3dUtil.h
./Compiler/poppack1.h
./Compiler/pushpack1.h
BaseImporter.h
Camera.h
Carbon/Carbon.h
Compiler/pstdint.h
D3Dcompiler.h
DDSTextureLoader.h
DirectXCollision.h
DirectXColors.h
DirectXMath.h
DirectXPackedVector.h
Exceptional.h
FrameResource.h
GameTimer.h
GeometryGenerator.h
IOStream.hpp
LogStream.hpp
Logger.hpp
MathFunctions.h
MathHelper.h
NullLogger.hpp
SmoothingGroups.inl
StringComparison.h
TargetConditionals.h
Windows.h
WindowsX.h
algorithm
android/asset_manager.h
android/asset_manager_jni.h
android/native_activity.h
array
assert.h
assimp/ByteSwapper.h
assimp/Compiler/pstdint.h
assimp/DefaultIOStream.h
assimp/DefaultIOSystem.h
assimp/DefaultLogger.hpp
assimp/Exceptional.h
assimp/GltfMaterial.h
assimp/Hash.h
assimp/IOStream.hpp
assimp/IOSystem.hpp
assimp/Importer.hpp
assimp/ParsingUtils.h
assimp/ProgressHandler.hpp
assimp/SGSpatialSort.h
assimp/StreamReader.h
assimp/StringComparison.h
assimp/StringUtils.h
assimp/TinyFormatter.h
assimp/aabb.h
assimp/ai_assert.h
assimp/anim.h
assimp/camera.h
assimp/cexport.h
assimp/color4.h
assimp/config.h
assimp/defs.h
assimp/importerdesc.h
assimp/light.h
assimp/material.h
assimp/matrix3x3.h
assimp/matrix4x4.h
assimp/mesh.h
assimp/metadata.h
assimp/postprocess.h
assimp/quaternion.h
assimp/scene.h
assimp/texture.h
assimp/types.h
assimp/vector2.h
assimp/vector3.h
cassert
cctype
cexport.h
chrono
cmath
color4.inl
comdef.h
crtdbg.h
cstdarg
cstddef
cstdint
cstdio
cstdlib
cstring
d3d11_1.h
d3d12.h
d3dApp.h
d3dUtil.h
d3dcompiler.h
d3dx12.h
defs.h
direct.h
dwmapi.h
dxgi1_4.h
dxgiformat.h
exception
filesystem
float.h
fstream
functional
imconfig.h
imgui.h
imgui_impl_dx12.h
imgui_impl_win32.h
imgui_internal.h
imgui_user.h
imgui_user.inl
imm.h
immintrin.h
imstb_rectpack.h
imstb_textedit.h
imstb_truetype.h
iomanip
iostream
limits
limits.h
list
locale
map
material.inl
math.h
matrix3x3.h
matrix3x3.inl
matrix4x4.h
matrix4x4.inl
memory
misc/freetype/imgui_freetype.h
model.h
new
nmmintrin.h
pugixml.hpp
quaternion.h
quaternion.inl
set
shellapi.h
signal.h
sstream
stb_sprintf.h
stb_truetype.h
stdarg.h
stddef.h
stdexcept
stdint.h
stdio.h
stdlib.h
string
string.h
sys/inttypes.h
sys/stat.h
sys/types.h
sys/wait.h
tchar.h
types.h
unistd.h
unordered_map
unordered_set
utility
vector
vector2.inl
vector3.inl
wchar.h
windows.h
windowsx.h
wrl.h
xinput.h
zlib.h

Total: 175
```

## TexColumnsApp Structure

- **Globals**: Declares a shared `Camera`, frame-count constants, and the `RenderItem` struct describing per-object transforms, material bindings, and draw ranges.
- **Lifecycle Overrides**: Implements `Initialize`, `OnResize`, `Update`, `Draw`, `DeferredDraw`, and mouse/keyboard handlers from `D3DApp`, wiring the frame loop to Direct3D resources.
- **Initialization Helpers**: `LoadAllTextures`, `BuildRootSignature`, `BuildLightingRootSignature`, `BuildShadowPassRootSignature`, `BuildDescriptorHeaps`, `BuildShadersAndInputLayout`, `BuildPSOs`, `BuildFrameResources`.
- **Scene Construction**: `BuildShapeGeometry`, `BuildCustomMeshGeometry`, `RenderCustomMesh`, `BuildMaterials`, `BuildRenderItems`, and `SetLightShapes` populate meshes, materials, and render queues.
- **Frame Update Path**: `UpdateCamera`, `AnimateMaterials`, `UpdateObjectCBs`, `UpdateMaterialCBs`, `UpdateLightCBs`, and `UpdateMainPassCB` refresh constant buffers and shadow transforms per frame.
- **Rendering Passes**: `DrawSceneToShadowMap`, `DrawRenderItems`, and the body of `Draw`/`DeferredDraw` record command lists for G-buffer, lighting, and ImGui composition.
- **Post Processing**: `CreateSceneTexture`, `BuildScreenQuadGeometry`, and `mPostProcessRootSignature` enable full-screen effects with configurable distortion parameters.
- **Lighting Utilities**: `BuildLights`, `CreatePointLight`, `CreateSpotLight`, `CreateGBuffer`, and G-buffer members handle light definitions, shadow maps, and descriptor placement.

## Baseline Variant Highlights

- `TexColumnsApp` and its support files were renamed to `BaselineApp`, `BaselineFrameResource`, `Baseline.vcxproj`, and `Baseline.sln` to clarify the starter project's role.
- The Visual Studio project targets the 10.0.26100.0 Windows SDK with the `v143` toolset for compatibility with Visual Studio 2026 Community.
- `BaselineApp::BuildMaterials` defines four tinted materials (`plane_red`, `plane_green`, `plane_blue`, `plane_yellow`) that share the 1×1 white texture but differ in diffuse albedo.
- `BaselineApp::BuildRenderItems` now instantiates four tiled grid quads, positioning them in a 2×2 layout so the ground plane displays distinct colors per tile.
- All camera movement, deferred shading passes, lighting, ImGui integration, and root-signature/shader infrastructure from the template are preserved for future extensions.

