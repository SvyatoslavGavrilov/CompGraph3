# File Refactoring Instructions

## Overview

This document outlines the steps to refactor the baseline project structure to:
- **Eliminate back imports** (imports with `..` paths)
- **Flatten directory structure** into a single source folder
- **Remove empty build artifact directories**
- **Simplify navigation and maintenance**

---

## Current Problems

### 1. **Back Imports**
Current imports use relative paths going up directories:
```cpp
#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "../../Common/Camera.h"
```

### 2. **Complex Directory Structure**
```
baseline/src/
├── Code/
│   └── Main/           # Application source
├── Common/             # Shared utilities
└── Textures/           # Texture assets
```

### 3. **Empty Build Directories**
- `Baseline/x64/Debug/` (auto-generated)
- `TexColumns/x64/Debug/` (auto-generated)

---

## Refactoring Goal

**Target Structure:**
```
baseline/src/
├── [All source files]  # All .cpp, .h files in root
├── Shaders/            # Shader files
├── Textures/           # Texture assets
├── Models/             # 3D model files (optional)
└── Libs/               # Library files
```

---

## Step-by-Step Refactoring Instructions

### Phase 1: Preparation

#### Step 1.1: Backup Current Structure
```powershell
# Create a backup
Copy-Item -Path "baseline" -Destination "baseline_backup" -Recurse
```

#### Step 1.2: Clean Build Artifacts
Delete auto-generated directories (they will be recreated during build):
- `baseline/src/Code/Main/Baseline/x64/Debug/`
- `baseline/src/Code/Main/TexColumns/x64/Debug/`
- Any `.obj`, `.pdb`, `.exe` files in source directories (optional - build will recreate)

---

### Phase 2: File Reorganization

#### Step 2.1: Move Common Files to Main Directory

**Action:** Move all files from `baseline/src/Common/` to `baseline/src/Code/Main/`

**Files to Move:**
- `Camera.cpp`, `Camera.h`
- `d3dApp.cpp`, `d3dApp.h`
- `d3dUtil.cpp`, `d3dUtil.h`
- `d3dx12.h`
- `DDSTextureLoader.cpp`, `DDSTextureLoader.h`
- `GameTimer.cpp`, `GameTimer.h`
- `GeometryGenerator.cpp`, `GeometryGenerator.h`
- `MathHelper.cpp`, `MathHelper.h`
- `model.cpp`, `model.h`
- `UploadBuffer.h`
- All ImGui files (`imgui*.cpp`, `imgui*.h`, `imstb*.h`)
- All Assimp files (`assimp/` directory)
- All `.obj` model files (or move to separate `Models/` subdirectory)

**PowerShell Command:**
```powershell
# Navigate to baseline/src
cd baseline/src

# Move all Common files to Code/Main
Move-Item -Path "Common\*" -Destination "Code\Main\" -Force

# Remove empty Common directory
Remove-Item -Path "Common" -Force
```

#### Step 2.2: Flatten Code/Main Structure

**Action:** Move all files from `baseline/src/Code/Main/` to `baseline/src/`

**Files to Move:**
- `Baseline.cpp`
- `BaselineFrameResource.cpp`, `BaselineFrameResource.h`
- All moved Common files
- `Shaders/` directory (keep as subdirectory)
- `Libs/` directory (keep as subdirectory)
- Project files: `Baseline.sln`, `Baseline.vcxproj`, `Baseline.vcxproj.user`
- DLL files (or move to `Libs/`)

**PowerShell Command:**
```powershell
# Still in baseline/src
# Move all Main files to src root
Move-Item -Path "Code\Main\*" -Destination "." -Force

# Remove empty directories
Remove-Item -Path "Code\Main" -Force
Remove-Item -Path "Code" -Force
```

#### Step 2.3: Organize Models (Optional but Recommended)

**Action:** Create a `Models/` subdirectory and move all `.obj` and `.mtl` files there

**Files to Move:**
- `*.obj` files (all OBJ models)
- `*.mtl` files (material files)

**PowerShell Command:**
```powershell
# Create Models directory
New-Item -ItemType Directory -Path "Models" -Force

# Move model files
Move-Item -Path "*.obj" -Destination "Models\" -Force
Move-Item -Path "*.mtl" -Destination "Models\" -Force
Move-Item -Path "*.OBJ" -Destination "Models\" -Force
```

**Result:** Cleaner source directory with assets organized

---

### Phase 3: Update Include Statements

#### Step 3.1: Update Baseline.cpp

**Current:**
```cpp
#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "../../Common/Camera.h"
```

**New:**
```cpp
#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "GeometryGenerator.h"
#include "Camera.h"
```

#### Step 3.2: Update BaselineFrameResource.h

**Current:**
```cpp
#include "../../Common/d3dUtil.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
```

**New:**
```cpp
#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
```

#### Step 3.3: Update Internal Includes in Common Files

**Check and update these files** (they may have relative includes):

- `d3dApp.h` - Check for includes of `d3dUtil.h`, `GameTimer.h`
- `d3dApp.cpp` - Check for includes
- `d3dUtil.h` - Check for includes
- `model.cpp` - Check for includes of `model.h`

**PowerShell Search Command:**
```powershell
# Find all files with back imports
Get-ChildItem -Recurse -Include *.cpp,*.h | Select-String -Pattern "\.\./" | Select-Object -Unique Path
```

**General Rule:** All includes should be either:
- Direct filename: `#include "d3dApp.h"` (for project files)
- Standard library: `#include <string>` (for STL/system headers)
- No `../` paths

---

### Phase 4: Update Project Configuration Files

#### Step 4.1: Update Baseline.vcxproj

**Current Path References:**
```xml
<ClInclude Include="..\..\Common\Camera.h" />
<ClInclude Include="..\..\Common\d3dApp.h" />
...
```

**Action:** Update all file paths to reflect new structure

**Find and Replace:**
- Search for: `..\..\Common\`
- Replace with: (empty - files are now in same directory)

**For files in subdirectories:**
- `Shaders\*.hlsl` → Keep as is (relative to project file location)
- `Models\*.obj` → Update paths if needed

#### Step 4.2: Update Include Directories in Project Settings

**In Visual Studio:**
1. Right-click project → **Properties**
2. Navigate to **Configuration Properties → C/C++ → General**
3. **Additional Include Directories:**
   - Remove: `..\..\Common`
   - Add (if needed): `$(ProjectDir)` or `.` for current directory
   - Keep: Standard DirectX paths

**Or edit `.vcxproj` directly:**
```xml
<AdditionalIncludeDirectories>$(ProjectDir);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
```

#### Step 4.3: Update Additional Dependencies (Libraries)

**Check Library Paths:**
- `Libs\assimp-vc143-mt.lib` - Should remain as `Libs\` or update path if Libs moved
- DirectX libraries remain system paths

---

### Phase 5: Update Shader Paths

#### Step 5.1: Check Shader Compilation Paths

**In Baseline.cpp, check shader compilation:**
```cpp
// Current (if it exists):
mShaders["pyramidVS"] = d3dUtil::CompileShader(L"Shaders\\Pyramid.hlsl", ...);

// Verify path is still correct after refactoring
// Should be: L"Shaders\\Pyramid.hlsl" (relative to executable directory)
// Or use: L"$(ProjectDir)Shaders\\Pyramid.hlsl" for absolute path
```

**Action:** Ensure shader paths in code match new directory structure

---

### Phase 6: Update Asset Paths (Textures, Models)

#### Step 6.1: Check Texture Loading Code

**Search for texture loading:**
```powershell
# Find texture path references
Get-ChildItem -Recurse -Include *.cpp,*.h | Select-String -Pattern "Textures"
```

**Common patterns:**
- `L"Textures\\texture.dds"` - Update if Textures moved
- `L"..\\Textures\\texture.dds"` - Update to `L"Textures\\texture.dds"`

#### Step 6.2: Update Model Loading Paths

**If models moved to Models/ subdirectory:**
- Update paths: `L"sponza.obj"` → `L"Models\\sponza.obj"`

---

### Phase 7: Clean Up and Verify

#### Step 7.1: Final Directory Structure Check

**Expected Structure:**
```
baseline/src/
├── Baseline.cpp
├── BaselineFrameResource.cpp
├── BaselineFrameResource.h
├── Baseline.sln
├── Baseline.vcxproj
├── Baseline.vcxproj.user
│
├── Camera.cpp, Camera.h
├── d3dApp.cpp, d3dApp.h
├── d3dUtil.cpp, d3dUtil.h
├── DDSTextureLoader.cpp, DDSTextureLoader.h
├── GameTimer.cpp, GameTimer.h
├── GeometryGenerator.cpp, GeometryGenerator.h
├── MathHelper.cpp, MathHelper.h
├── model.cpp, model.h
├── UploadBuffer.h
├── d3dx12.h
│
├── imgui*.cpp, imgui*.h
├── imstb*.h
│
├── assimp/              # Entire directory
│   ├── *.h, *.hpp
│   └── ...
│
├── Shaders/
│   ├── Debug.hlsl
│   ├── Default.hlsl
│   ├── GeometryPass.hlsl
│   ├── LightingPass.hlsl
│   ├── LightingUtil.hlsl
│   ├── Pyramid.hlsl
│   └── ShadowMap.hlsl
│
├── Models/              # (Optional - if created)
│   ├── *.obj
│   └── *.mtl
│
├── Textures/
│   ├── *.dds
│   ├── *.bmp
│   └── textures/
│
├── Libs/
│   └── assimp-vc143-mt.lib
│
└── [DLL files or move to Libs/]
```

#### Step 7.2: Verify All Includes

**PowerShell Script to Check:**
```powershell
# Find all remaining back imports
Get-ChildItem -Recurse -Include *.cpp,*.h | 
    Select-String -Pattern "include.*\.\./" | 
    Format-Table Path, LineNumber, Line -AutoSize
```

**Action:** Fix any remaining `../` includes found

#### Step 7.3: Build and Test

1. Open `Baseline.sln` in Visual Studio
2. Clean solution: **Build → Clean Solution**
3. Rebuild: **Build → Rebuild Solution**
4. Fix any compilation errors related to:
   - Missing includes
   - Incorrect paths
   - Project configuration

#### Step 7.4: Runtime Path Verification

**Test that asset loading works:**
- Run the application
- Verify textures load correctly
- Verify models load correctly (if applicable)
- Verify shaders compile correctly

**Common Issues:**
- Assets not found → Check working directory in project settings
- Shaders not found → Update shader paths or working directory

---

## Automated Refactoring Script

### Complete PowerShell Refactoring Script

```powershell
# ============================================
# Baseline Project Refactoring Script
# ============================================
# WARNING: Backup your project before running!

$ErrorActionPreference = "Stop"

# Set paths
$basePath = "baseline/src"
$mainPath = "$basePath/Code/Main"
$commonPath = "$basePath/Common"
$texturesPath = "$basePath/Textures"

Write-Host "Starting refactoring..." -ForegroundColor Green

# Step 1: Move Common files to Main
Write-Host "Moving Common files..." -ForegroundColor Yellow
if (Test-Path $commonPath) {
    Move-Item -Path "$commonPath\*" -Destination $mainPath -Force
    Remove-Item -Path $commonPath -Force
}

# Step 2: Flatten Main to src root
Write-Host "Flattening directory structure..." -ForegroundColor Yellow
if (Test-Path $mainPath) {
    Move-Item -Path "$mainPath\*" -Destination $basePath -Force
    Remove-Item -Path "$basePath/Code/Main" -Force -ErrorAction SilentlyContinue
    Remove-Item -Path "$basePath/Code" -Force -ErrorAction SilentlyContinue
}

# Step 3: Create Models directory (optional)
Write-Host "Organizing model files..." -ForegroundColor Yellow
$modelsPath = "$basePath/Models"
if (-not (Test-Path $modelsPath)) {
    New-Item -ItemType Directory -Path $modelsPath -Force | Out-Null
}

# Move OBJ and MTL files
Get-ChildItem -Path $basePath -Filter "*.obj" | Move-Item -Destination $modelsPath -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $basePath -Filter "*.OBJ" | Move-Item -Destination $modelsPath -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $basePath -Filter "*.mtl" | Move-Item -Destination $modelsPath -Force -ErrorAction SilentlyContinue

# Step 4: Update includes in source files
Write-Host "Updating include statements..." -ForegroundColor Yellow

# Update Baseline.cpp
$baselineCpp = "$basePath/Baseline.cpp"
if (Test-Path $baselineCpp) {
    (Get-Content $baselineCpp) -replace '\.\.\/\.\.\/Common\/', '' | Set-Content $baselineCpp
    Write-Host "Updated Baseline.cpp" -ForegroundColor Cyan
}

# Update BaselineFrameResource.h
$frameResourceH = "$basePath/BaselineFrameResource.h"
if (Test-Path $frameResourceH) {
    (Get-Content $frameResourceH) -replace '\.\.\/\.\.\/Common\/', '' | Set-Content $frameResourceH
    Write-Host "Updated BaselineFrameResource.h" -ForegroundColor Cyan
}

# Update all other .cpp and .h files
Get-ChildItem -Path $basePath -Recurse -Include *.cpp,*.h | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    $original = $content
    $content = $content -replace '\.\.\/\.\.\/Common\/', ''
    $content = $content -replace '\.\.\/Common\/', ''
    
    if ($content -ne $original) {
        Set-Content -Path $_.FullName -Value $content -NoNewline
        Write-Host "Updated $($_.Name)" -ForegroundColor Cyan
    }
}

# Step 5: Update .vcxproj file
Write-Host "Updating project file..." -ForegroundColor Yellow
$vcxproj = "$basePath/Baseline.vcxproj"
if (Test-Path $vcxproj) {
    $projContent = Get-Content $vcxproj -Raw
    $projContent = $projContent -replace '\.\.\\\.\.\\Common\\', ''
    $projContent = $projContent -replace '\.\.\\Common\\', ''
    Set-Content -Path $vcxproj -Value $projContent -NoNewline
    Write-Host "Updated Baseline.vcxproj" -ForegroundColor Cyan
}

Write-Host "`nRefactoring complete!" -ForegroundColor Green
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Open Baseline.sln in Visual Studio"
Write-Host "2. Clean and rebuild the solution"
Write-Host "3. Fix any remaining path issues"
Write-Host "4. Update asset paths if needed (textures, models, shaders)"
```

**Usage:**
1. Save script as `refactor.ps1`
2. Navigate to project root: `cd G:\AsciChalenge\ITMO\CG3`
3. Review and backup first
4. Run: `.\refactor.ps1`

---

## Manual Checklist

Use this checklist for manual refactoring:

- [ ] **Backup created**
- [ ] **Common files moved** to Code/Main
- [ ] **Code/Main files moved** to src root
- [ ] **Empty directories removed** (Common, Code/Main, Code)
- [ ] **Models organized** (moved to Models/ subdirectory - optional)
- [ ] **Baseline.cpp includes updated** (remove `../../Common/`)
- [ ] **BaselineFrameResource.h includes updated** (remove `../../Common/`)
- [ ] **All .cpp files checked** for back imports
- [ ] **All .h files checked** for back imports
- [ ] **Baseline.vcxproj updated** (remove `..\..\Common\` paths)
- [ ] **Include directories updated** in project settings
- [ ] **Shader paths verified** (still correct after move)
- [ ] **Texture paths verified** (update if Textures moved)
- [ ] **Model paths updated** (if moved to Models/)
- [ ] **Solution cleaned and rebuilt**
- [ ] **Application tested** (run and verify assets load)

---

## Benefits After Refactoring

### ✅ Simplified Structure
- All source files in one location
- Easier to navigate
- Clearer organization

### ✅ No Back Imports
- Direct includes: `#include "d3dApp.h"`
- Easier to read and understand
- Better IDE support

### ✅ Reduced Nesting
- Flat structure for source code
- Logical subdirectories only for assets (Shaders, Textures, Models)

### ✅ Better Maintainability
- Easier to find files
- Clearer dependencies
- Simpler build configuration

---

## Troubleshooting

### Issue: Build errors after refactoring

**Solution:**
1. Check **Additional Include Directories** in project settings
2. Verify all includes are updated (search for `../`)
3. Ensure file paths in `.vcxproj` are correct

### Issue: Assets not loading at runtime

**Solution:**
1. Check **Working Directory** in project properties (Debugging → Working Directory)
2. Update asset paths in code if structure changed
3. Verify assets are in expected locations

### Issue: Shaders not compiling

**Solution:**
1. Check shader file paths in compilation code
2. Verify `Shaders/` directory is accessible
3. Update shader paths to be relative to executable or absolute

---

## Final Notes

- **Always backup** before refactoring
- **Test thoroughly** after each major change
- **Keep build artifacts** separate from source (use `.gitignore`)
- **Document** any path-dependent code (asset loading, shader compilation)

---

*This refactoring will result in a cleaner, more maintainable project structure while preserving all functionality.*










