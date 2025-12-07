# Baseline Project Structure

## Table of Contents

- [Overview](#overview)
- [Directory Structure](#directory-structure)
  - [Main Directory](#main-directory)
  - [Common Directory](#common-directory)
  - [Textures Directory](#textures-directory)
- [File Categories](#file-categories)
  - [Source Code Files](#source-code-files)
  - [Shader Files](#shader-files)
  - [3D Models](#3d-models)
  - [Texture Assets](#texture-assets)
  - [Build Artifacts](#build-artifacts)
  - [External Libraries](#external-libraries)

---

## Overview

The baseline project is a DirectX 12 graphics application organized into three main directories:

- **`src/Code/Main/`** - Main application source code, shaders, and build configuration
- **`src/Common/`** - Shared utilities, DirectX helpers, and 3D model loading libraries
- **`src/Textures/`** - Texture assets (DDS, BMP, PNG formats)

---

## Directory Structure

### Main Directory

**Path:** `baseline/src/Code/Main/`

#### Core Application Files

- `Baseline.cpp` - Main application entry point
- `Baseline.h` - Main application header (if exists)
- `BaselineFrameResource.cpp` - Frame resource management implementation
- `BaselineFrameResource.h` - Frame resource management header
- `Baseline.txt` - Application documentation/notes

#### Project Configuration

- `Baseline.sln` - Visual Studio solution file
- `Baseline.vcxproj` - Visual Studio project file
- `Baseline.vcxproj.user` - User-specific project settings
- `imgui.ini` - ImGui configuration file

#### Executables & Libraries

- `Baseline.exe` - Compiled application executable
- `Baseline.pdb` - Program database file (debug symbols)
- `TexColumns.exe` - Additional executable (TexColumns project)
- `TexColumns.pdb` - TexColumns debug symbols

#### DLL Dependencies

- `assimp-vc143-mt.dll` - Assimp library dynamic link library
- `D3DCompiler_42.dll` - DirectX shader compiler
- `D3DX9_42.dll` - DirectX utility library

#### Libraries

**Path:** `baseline/src/Code/Main/Libs/`
- `assimp-vc143-mt.lib` - Assimp static library

#### Shaders

**Path:** `baseline/src/Code/Main/Shaders/`

- `Debug.hlsl` - Debug shader
- `Default.hlsl` - Default rendering shader
- `GeometryPass.hlsl` - Geometry pass shader for deferred rendering
- `GeometryPass.txt` - Geometry pass shader metadata
- `LightingPass.hlsl` - Lighting pass shader for deferred rendering
- `LightingPass.txt` - Lighting pass shader metadata
- `LightingUtil.hlsl` - Lighting utility functions
- `Pyramid.hlsl` - Pyramid rendering shader
- `ShadowMap.hlsl` - Shadow mapping shader

#### Build Artifacts

**Path:** `baseline/src/Code/Main/Baseline/x64/Debug/`

##### Object Files (.obj)
- `Baseline.obj` - Compiled main application
- `BaselineFrameResource.obj`
- `Camera.obj`
- `d3dApp.obj`
- `d3dUtil.obj`
- `DDSTextureLoader.obj`
- `GameTimer.obj`
- `GeometryGenerator.obj`
- `imgui.obj`, `imgui_draw.obj`, `imgui_tables.obj`, `imgui_widgets.obj`
- `imgui_impl_dx12.obj`, `imgui_impl_win32.obj`
- `MathHelper.obj`
- `model.obj`

##### Build Logs
- `Baseline.exe.recipe` - Build recipe
- `Baseline.ilk` - Incremental linker file
- `Baseline.log` - Build log
- `vc143.idb` - Intermediate database
- `vc143.pdb` - Program database

##### TLog Directory

**Path:** `baseline/src/Code/Main/Baseline/x64/Debug/Baseline.tlog/`
- `Baseline.lastbuildstate`
- `CL.command.1.tlog` - C/C++ compiler commands
- `Cl.items.tlog` - Compiled items
- `CL.read.1.tlog` - Files read by compiler
- `CL.write.1.tlog` - Files written by compiler
- `link.command.1.tlog` - Linker commands
- `link.read.1.tlog` - Files read by linker
- `link.secondary.1.tlog` - Secondary linker inputs
- `link.write.1.tlog` - Files written by linker

#### TexColumns Build Artifacts

**Path:** `baseline/src/Code/Main/TexColumns/x64/Debug/`

Similar structure to Baseline build artifacts, including:
- Object files for TexColumns-specific code
- `TexColumnsApp.obj` - TexColumns application object
- Build logs and tlog directory

---

### Common Directory

**Path:** `baseline/src/Common/`

#### Core DirectX 12 Components

- `d3dApp.cpp` / `d3dApp.h` - DirectX 12 application framework
- `d3dUtil.cpp` / `d3dUtil.h` - DirectX utility functions
- `d3dx12.h` - DirectX 12 helper headers

#### Camera System

- `Camera.cpp` / `Camera.h` - Camera controller and view matrix management

#### Rendering Utilities

- `GeometryGenerator.cpp` / `GeometryGenerator.h` - Procedural geometry generation
- `MathHelper.cpp` / `MathHelper.h` - Mathematical helper functions
- `UploadBuffer.h` - GPU upload buffer management

#### Texture Loading

- `DDSTextureLoader.cpp` / `DDSTextureLoader.h` - DDS texture file loader

#### Model Loading

- `model.cpp` / `model.h` - 3D model loader and manager
- `assimp/` - [Assimp library](#assimp-library) (Open Asset Import Library)

#### ImGui Integration

- `imgui.h` - ImGui main header
- `imgui.cpp` - ImGui implementation
- `imgui_draw.cpp` - ImGui drawing functions
- `imgui_tables.cpp` - ImGui table widget
- `imgui_widgets.cpp` - ImGui widgets
- `imgui_internal.h` - ImGui internal APIs
- `imconfig.h` - ImGui configuration
- `imstb_rectpack.h` - STB rectangle packing
- `imstb_textedit.h` - STB text editing
- `imstb_truetype.h` - STB TrueType font rendering
- `imgui_impl_dx12.cpp` / `imgui_impl_dx12.h` - DirectX 12 backend
- `imgui_impl_win32.cpp` / `imgui_impl_win32.h` - Windows backend

#### Timing

- `GameTimer.cpp` / `GameTimer.h` - High-precision game timing

#### 3D Model Files

**OBJ Models:**
- `sponza.obj` / `sponza.mtl` - Sponza Palace scene (test scene)
- `sponza_ornament.obj` / `sponza_ornament_Internal.OBJ` - Sponza ornaments
- `arch_stones_01_Internal.OBJ` - Architectural stones
- `left.obj` / `right.obj` - Left/right geometry
- `plane.obj` / `plane2.obj` - Plane geometry
- `negr.obj` - Additional model
- `madoka.obj` - Character model

#### Assimp Library

**Path:** `baseline/src/Common/assimp/`

Assimp (Open Asset Import Library) provides model loading capabilities:

##### Core Headers
- `types.h` - Basic type definitions
- `defs.h` - Library definitions
- `config.h` - Build configuration
- `version.h` - Version information

##### Scene & Mesh
- `scene.h` - Scene graph structures
- `mesh.h` - Mesh data structures
- `material.h` / `material.inl` - Material definitions
- `texture.h` - Texture information
- `anim.h` - Animation data

##### Math Utilities
- `vector2.h` / `vector2.inl` - 2D vector
- `vector3.h` / `vector3.inl` - 3D vector
- `matrix3x3.h` / `matrix3x3.inl` - 3x3 matrix
- `matrix4x4.h` / `matrix4x4.inl` - 4x4 matrix
- `quaternion.h` / `quaternion.inl` - Quaternion operations
- `color4.h` / `color4.inl` - RGBA color
- `MathFunctions.h` - Mathematical functions

##### Import/Export
- `Importer.hpp` - Main importer class
- `Exporter.hpp` - Main exporter class
- `BaseImporter.h` - Base importer interface
- `postprocess.h` - Post-processing flags
- `cimport.h` / `cexport.h` - C-style import/export
- `importerdesc.h` - Importer descriptions

##### I/O System
- `IOStream.hpp` - Input/output stream interface
- `IOSystem.hpp` - I/O system interface
- `DefaultIOStream.h` - Default stream implementation
- `DefaultIOSystem.h` - Default I/O system
- `BlobIOSystem.h` - Blob-based I/O
- `MemoryIOWrapper.h` - Memory wrapper
- `ZipArchiveIOSystem.h` - ZIP archive support

##### Utilities
- `StringUtils.h` - String utilities
- `StringComparison.h` - String comparison
- `ByteSwapper.h` - Endianness swapping
- `fast_atof.h` - Fast string-to-float conversion
- `Hash.h` - Hash functions
- `SpatialSort.h` - Spatial sorting
- `SGSpatialSort.h` - Smoothing group spatial sort
- `SceneCombiner.h` - Scene combination utilities

##### Specialized Features
- `GltfMaterial.h` - glTF material support
- `ObjMaterial.h` - OBJ material support
- `pbrmaterial.h` - PBR material support
- `SkeletonMeshBuilder.h` - Skeletal mesh building
- `StandardShapes.h` - Standard geometric shapes
- `Subdivision.h` - Mesh subdivision

##### Logging
- `Logger.hpp` - Logger interface
- `DefaultLogger.hpp` - Default logger
- `NullLogger.hpp` - Null logger (no output)
- `LogStream.hpp` - Log stream
- `LogAux.h` - Logging auxiliary

##### Parsing & Processing
- `ParsingUtils.h` - Parsing utilities
- `RemoveComments.h` - Comment removal
- `LineSplitter.h` - Line splitting
- `XMLTools.h` - XML utilities
- `XmlParser.h` - XML parser
- `TinyFormatter.h` - Formatting utilities

##### Compiler Support

**Path:** `baseline/src/Common/assimp/Compiler/`
- `poppack1.h` - Structure packing control
- `pushpack1.h` - Structure packing control
- `pstdint.h` - Portable stdint definitions

##### Platform-Specific

**Path:** `baseline/src/Common/assimp/port/AndroidJNI/`
- `AndroidJNIIOSystem.h` - Android JNI I/O system
- `BundledAssetIOSystem.h` - Android bundled assets

##### Other Headers
- `aabb.h` - Axis-aligned bounding box
- `ai_assert.h` - Assertion macros
- `AssertHandler.h` - Assertion handler
- `Base64.hpp` - Base64 encoding
- `Bitmap.h` - Bitmap utilities
- `ColladaMetaData.h` - Collada metadata
- `commonMetaData.h` - Common metadata
- `CreateAnimMesh.h` - Animated mesh creation
- `Exceptional.h` - Exception handling
- `fast_atof.h` - Fast atof implementation
- `GenericProperty.h` - Generic properties
- `IOStreamBuffer.h` - I/O stream buffer
- `light.h` - Light definitions
- `camera.h` - Camera definitions
- `cfileio.h` - C file I/O
- `metadata.h` - Metadata structures
- `module.modulemap` - Swift/Objective-C module map
- `Profiler.h` - Profiling utilities
- `ProgressHandler.hpp` - Progress tracking
- `qnan.h` - Quiet NaN handling
- `revision.h` / `revision.h.in` - Revision information
- `SmallVector.h` - Small vector optimization
- `SmoothingGroups.h` / `SmoothingGroups.inl` - Smoothing groups
- `StreamReader.h` / `StreamWriter.h` - Stream I/O
- `Vertex.h` - Vertex structures

---

### Textures Directory

**Path:** `baseline/src/Textures/`

#### Root Texture Files

##### Diffuse Textures
- `bricks.dds`, `bricks2.dds`, `bricks3.dds` - Brick textures
- `brickwall_02_BaseColor.dds` - Brick wall color map
- `checkboard.dds` - Checkerboard pattern
- `eye.dds` - Eye texture
- `grass.dds` - Grass texture
- `head_diff.dds` - Head diffuse
- `jacket_diff.dds` - Jacket diffuse
- `pants_diff.dds` - Pants diffuse
- `stone.dds` - Stone texture
- `texture.dds` - Generic texture
- `tile.dds` - Tile texture
- `upBody_diff.dds` - Upper body diffuse
- `water1.dds` - Water texture
- `white1x1.dds` - White 1x1 placeholder
- `WireFence.dds` - Wire fence texture
- `WoodCrate01.dds`, `WoodCrate02.dds` - Wooden crate textures

##### Normal Maps
- `bricks_nmap.dds`, `bricks2_nmap.dds` - Brick normal maps
- `default_nmap.dds` - Default normal map
- `head_norm.dds` - Head normal map
- `jacket_norm.dds` - Jacket normal map
- `pants_norm.dds` - Pants normal map
- `tile_nmap.dds` - Tile normal map
- `upbody_norm.dds` - Upper body normal map

##### Cube Maps
- `desertcube1024.dds` - Desert skybox cube map
- `grasscube1024.dds` - Grass skybox cube map
- `snowcube1024.dds` - Snow skybox cube map
- `sunsetcube1024.dds` - Sunset skybox cube map

##### Special Textures
- `ice.dds` - Ice material
- `M_Skin.png` - Skin texture (PNG format)
- `tree0.bmp`, `tree1.bmp`, `tree2.bmp` - Tree billboards (BMP)
- `tree01S.dds`, `tree02S.dds`, `tree35S.dds` - Tree textures
- `treearray.dds`, `treeArray2.dds` - Tree texture arrays

#### Subdirectory: textures/

**Path:** `baseline/src/Textures/textures/`

Extended texture collection including:

##### Sponza Scene Textures
- `sponza_arch_*.dds` - Sponza arch textures (diffuse, normal)
- `sponza_ceiling_a_*.dds` - Ceiling textures
- `sponza_column_a/b/c_*.dds` - Column textures
- `sponza_curtain_*.dds` - Curtain textures (diffuse, normal, blue, green)
- `sponza_details_*.dds` - Detail textures
- `sponza_fabric_*.dds` - Fabric textures
- `sponza_flagpole_*.dds` - Flagpole textures
- `sponza_floor_a_*.dds` - Floor textures
- `sponza_roof_*.dds` - Roof textures
- `sponza_thorn_*.dds` - Thorn textures
- `spnza_bricks_a_*.dds` - Sponza brick textures

##### Additional Textures
- `background.dds`, `background_ddn.dds` - Background textures
- `chain_texture.dds`, `chain_texture_ddn.dds` - Chain textures
- `default.dds` - Default texture
- `DispMap.dds` - Displacement map
- `HeightMap.dds`, `HeightMap2/3/4.dds` - Height maps
- `lion.dds`, `lion_ddn.dds`, `lion2_ddn.dds` - Lion textures
- `mskin.dds`, `mskin_nm.dds` - Material skin textures
- `redbrick_*.dds` - Red brick textures (diffuse, normal, displacement)
- `rock.dds`, `rock_nmap.dds`, `rock_disp.dds` - Rock textures
- `rocks.dds`, `rocks_nmap.dds`, `rocks_disp.dds` - Rocks textures
- `stone_disp.dds`, `stone_nmap.dds` - Stone displacement and normal maps
- `texture_nm.dds` - Texture normal map
- `vase_*.dds` - Vase textures (various types)
- `vase_plant.dds`, `vase_hanging.dds`, `vase_round.dds` - Various vase textures

##### Texture Naming Conventions
- `*_diff.dds` - Diffuse/albedo maps
- `*_ddn.dds` - Normal maps (derived from "dot dot normal")
- `*_nmap.dds` - Normal maps
- `*_nm.dds` - Normal maps (shorter convention)
- `*_disp.dds` - Displacement maps
- `*cube1024.dds` - Cube maps (1024x1024 per face)

---

## File Categories

### Source Code Files

#### Application Core
- [[#Baseline.cpp|Baseline.cpp]] - Main application logic
- [[#BaselineFrameResource|BaselineFrameResource.cpp/h]] - Per-frame resource management

#### DirectX Framework
- [[#d3dApp|d3dApp.cpp/h]] - Application framework
- [[#d3dUtil|d3dUtil.cpp/h]] - Utility functions
- [[#UploadBuffer|UploadBuffer.h]] - Buffer management

#### Systems
- [[#Camera|Camera.cpp/h]] - Camera controller
- [[#GameTimer|GameTimer.cpp/h]] - Timing system
- [[#GeometryGenerator|GeometryGenerator.cpp/h]] - Geometry generation
- [[#MathHelper|MathHelper.cpp/h]] - Math utilities

#### Asset Loading
- [[#model|model.cpp/h]] - 3D model loader
- [[#DDSTextureLoader|DDSTextureLoader.cpp/h]] - Texture loader

### Shader Files

All shaders located in [[#Shaders|Shaders/]] directory:
- `Debug.hlsl` - Debug visualization
- `Default.hlsl` - Default rendering
- `GeometryPass.hlsl` - Deferred geometry pass
- `LightingPass.hlsl` - Deferred lighting pass
- `LightingUtil.hlsl` - Lighting utilities
- `Pyramid.hlsl` - Pyramid rendering
- `ShadowMap.hlsl` - Shadow mapping

### 3D Models

#### Scene Models
- [[#sponza|sponza.obj/mtl]] - Sponza Palace test scene
- [[#sponza_ornament|sponza_ornament.obj]] - Ornament elements

#### Geometry Primitives
- `plane.obj`, `plane2.obj` - Planes
- `left.obj`, `right.obj` - Left/right geometry
- `arch_stones_01_Internal.OBJ` - Architectural elements

#### Character Models
- `madoka.obj` - Character model
- `negr.obj` - Additional model

### Texture Assets

#### Formats
- **DDS** (DirectDraw Surface) - Primary texture format
- **BMP** - Used for tree billboards
- **PNG** - Used for `M_Skin.png`

#### Texture Types
- **Diffuse/Albedo** - Base color textures
- **Normal Maps** - Surface detail and lighting
- **Cube Maps** - Skybox and environment mapping
- **Displacement Maps** - Height-based surface deformation

### Build Artifacts

#### Visual Studio Output
- Object files (`.obj`) - Compiled source files
- Program databases (`.pdb`) - Debug information
- Build logs (`.log`, `.tlog`) - Compilation tracking
- Executables (`.exe`) - Final compiled programs

#### Build Directories
- `Baseline/x64/Debug/` - Main application build output
- `TexColumns/x64/Debug/` - TexColumns application build output

### External Libraries

#### Assimp
- **Library:** [[#Assimp Library|assimp/]] directory
- **DLL:** `assimp-vc143-mt.dll`
- **Static Lib:** `Libs/assimp-vc143-mt.lib`
- **Purpose:** 3D model file format support (OBJ, FBX, glTF, etc.)

#### DirectX
- `D3DCompiler_42.dll` - Shader compilation
- `D3DX9_42.dll` - DirectX utilities

#### ImGui
- Headers in [[#ImGui Integration|Common/]] directory
- **Purpose:** Immediate-mode GUI for debugging and configuration

---

## Quick Reference

### Key Entry Points
- **Main Application:** `src/Code/Main/Baseline.cpp`
- **Shaders:** `src/Code/Main/Shaders/`
- **Shared Code:** `src/Common/`
- **Assets:** `src/Textures/`

### Build Output
- **Debug Builds:** `src/Code/Main/Baseline/x64/Debug/`
- **Executable:** `src/Code/Main/Baseline.exe`

### Asset Locations
- **3D Models:** `src/Common/*.obj`
- **Textures:** `src/Textures/*.dds` and `src/Textures/textures/*.dds`
- **Materials:** `src/Common/*.mtl`

---

*Last Updated: Analysis of baseline project structure*

