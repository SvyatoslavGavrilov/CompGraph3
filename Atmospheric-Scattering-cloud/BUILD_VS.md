# Building with Visual Studio

This project has been refactored and now includes Visual Studio solution and project files.

## Project Structure

After refactoring:
- All source files are in `src/` (no nested subdirectories)
- All includes use direct filenames (no `tools/`, `GLType/`, or `Math/` prefixes)
- Shader files remain in `shaders/`
- Resources remain in `resources/`

## Building External Dependencies

The project depends on several external libraries that need to be built first:

### Required Libraries:
1. **GLFW** - Window and input handling
2. **GLEW** - OpenGL extension loading
3. **GLI** - Graphics library for image loading
4. **Zlib** - Compression library
5. **ImGui** - Already included as source files in the project

### Building External Libraries:

#### Option 1: Use CMake (Recommended)
1. Open a terminal in the `Atmospheric-Scattering-cloud` directory
2. Create a build directory: `mkdir build && cd build`
3. Run CMake: `cmake ..`
4. Build: `cmake --build . --config Release`
5. Copy the built libraries to the paths specified in `LightScattering.vcxproj`

#### Option 2: Build Libraries Individually
Each library in `external/` has its own build system. Refer to their documentation.

### Library Paths

The project expects libraries in:
- `external/glfw-3.1.2/lib-vc2022/` - GLFW libraries
- `external/glew-1.13.0/lib/Release/x64/` - GLEW libraries
- `external/gli/lib/` - GLI libraries
- `external/zlib/lib/` - Zlib libraries
- `external/imgui/lib/` - ImGui libraries (if building as library)

You may need to adjust these paths in `LightScattering.vcxproj` based on where your libraries are built.

## Building the Project

1. Open `LightScattering.sln` in Visual Studio 2019 or later
2. Select configuration: Debug or Release
3. Select platform: x64
4. Build Solution (F7 or Build → Build Solution)

## Configuration

### Include Directories
The project includes:
- `src/` - Main source files
- `external/` - External library headers
- `external/glfw-3.1.2/include/GLFW/` - GLFW headers
- `external/glm/` - GLM math library
- `external/glew-1.13.0/include/` - GLEW headers
- `external/gli/` - GLI headers
- `external/zlib/` - Zlib headers
- `external/imgui/` - ImGui headers
- `external/glsw/` - GLSL Shader Wrangler headers

### Preprocessor Definitions
- `TW_STATIC`, `TW_NO_LIB_PRAGMA`, `TW_NO_DIRECT3D` - AntTweakBar settings
- `GLEW_STATIC` - Use static GLEW library
- `_CRT_SECURE_NO_WARNINGS` - Disable security warnings

### Working Directory
The working directory is set to the project directory (`$(ProjectDir)`) so that:
- Shaders are loaded from `shaders/`
- Resources are loaded from `resources/`

## Troubleshooting

### Missing Libraries
If you get linker errors about missing libraries:
1. Ensure all external libraries are built
2. Check library paths in `LightScattering.vcxproj`
3. Verify library names match the actual files

### Include Errors
If you get include errors:
1. Verify all include directories are correct in project settings
2. Check that external library headers are present
3. Ensure source files use direct includes (no `tools/`, `GLType/` prefixes)

### Shader Loading Errors
If shaders fail to load:
1. Check that `shaders/` directory exists relative to executable
2. Verify working directory is set correctly in project settings
3. Ensure GLSW (GLSL Shader Wrangler) is properly initialized

## Notes

- The project uses C++17 standard
- Multi-processor compilation is enabled (`/MP`)
- ImGui is included as source files (not as a library)
- GLSW (GLSL Shader Wrangler) is included as source files


