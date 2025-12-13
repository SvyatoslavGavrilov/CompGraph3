## Cursor AI Agent Prompt: Atmospheric Rendering Implementation with Terrain Integration

### 🎯 **MISSION OBJECTIVE**
Implement two atmospheric scattering methods (Hoffman-Preetham Approach and Ray Marching High Altitude) within the existing terrain project (`labor_4.cpp` + `Terrain.hlsl`) with real-time IMGUI parameter control. **Minimal changes to main file structure** while maintaining terrain rendering functionality.

---

### 📋 **PROJECT CONTEXT ANALYSIS**
**Existing Files:**
- `labor_4.cpp`: Main application file with terrain rendering, camera controls, and IMGUI setup
- `Terrain.hlsl`: Terrain shader (vertex/pixel shader combo)

**Constraints:**
- Preserve existing terrain rendering pipeline
- Integrate atmosphere as post-process or separate render pass
- Keep IMGUI integration clean and non-intrusive
- Minimize modifications to `labor_4.cpp` core structure

---

### 🔧 **IMPLEMENTATION STRATEGY (Step-by-Step)**

#### **STEP 1: ATMOSPHERE SHADER CREATION**
**Create `Atmosphere.hlsl` with dual-mode support:**
```hlsl
// Atmosphere.hlsl
cbuffer AtmosphereParams : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float3 CameraPos;
    float3 SunDirection;
    float3 PlanetCenter;
    float AtmosphereRadius;
    float PlanetRadius;
    float3 RayleighScattering;
    float3 MieScattering;
    float MieG;
    float SunIntensity;
    int AtmosphereMode; // 0 = Hoffman-Preetham, 1 = Ray Marching
    float DensityMultiplier;
    float PollutionLevel; // 0.0 = clean, 1.0 = dirty
};

struct VS_INPUT
{
    float3 Position : POSITION;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
};

// Common functions for both methods
float3 getPrimaryRayDirection(float2 uv)
{
    // Reconstruct view direction from screen UV
    float3 viewRay = normalize(mul(float4(uv, 1.0, 0.0), View).xyz);
    return viewRay;
}

// Hoffman-Preetham Approach (Ground Level)
PS_INPUT VS_HoffmanPreetham(VS_INPUT input)
{
    PS_INPUT output;
    output.Position = mul(float4(input.Position, 1.0), mul(View, Projection));
    output.WorldPos = input.Position;
    output.ViewDir = normalize(input.Position - CameraPos);
    return output;
}

float4 PS_HoffmanPreetham(PS_INPUT input) : SV_TARGET
{
    // Apply pollution to scattering coefficients
    float3 rayleigh = RayleighScattering * (1.0 + PollutionLevel * 2.0);
    float3 mie = MieScattering * (1.0 + PollutionLevel * 4.0);
    
    // Hoffman-Preetham calculation
    float3 viewDir = normalize(input.ViewDir);
    float cosTheta = dot(viewDir, normalize(SunDirection));
    
    // Extinction factor (Ft)
    float opticalDepth = exp(-length(input.WorldPos - PlanetCenter) / 8000.0);
    float3 extinction = exp(-(rayleigh + mie) * opticalDepth * DensityMultiplier);
    
    // Sky color calculation
    float3 skyColor = float3(0, 0, 0);
    
    // Rayleigh scattering contribution
    float rayleighPhase = 0.75 * (1.0 + cosTheta * cosTheta);
    skyColor += rayleigh * rayleighPhase * SunIntensity;
    
    // Mie scattering contribution (forward scattering)
    float miePhase = 0.75 * ((1.0 - MieG * MieG) / pow(1.0 + MieG * MieG - 2.0 * MieG * cosTheta, 1.5));
    skyColor += mie * miePhase * SunIntensity;
    
    // Apply extinction
    skyColor *= extinction;
    
    return float4(skyColor, 1.0);
}

// Ray Marching Approach (High Altitude)
PS_INPUT VS_RayMarching(VS_INPUT input)
{
    PS_INPUT output;
    output.Position = mul(float4(input.Position, 1.0), mul(View, Projection));
    output.WorldPos = input.Position;
    output.ViewDir = normalize(input.Position - CameraPos);
    return output;
}

// Phase function for Rayleigh scattering
float getRayleighPhase(float cosTheta)
{
    return 0.75 * (1.0 + cosTheta * cosTheta);
}

// Phase function for Mie scattering (Henyey-Greenstein)
float getMiePhase(float cosTheta, float g)
{
    float g2 = g * g;
    return 0.75 * ((1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

// Scale function for optical depth approximation
float scale(float cosTheta)
{
    float x = 1.0 - cosTheta;
    return exp(-0.00287 + x * (0.459 + x * (3.83 + x * (-6.80 + x * 5.25))));
}

float4 PS_RayMarching(PS_INPUT input) : SV_TARGET
{
    // Apply pollution to scattering coefficients
    float3 rayleigh = RayleighScattering * (1.0 + PollutionLevel * 2.0);
    float3 mie = MieScattering * (1.0 + PollutionLevel * 4.0);
    
    float3 viewDir = normalize(input.ViewDir);
    float3 startPos = CameraPos;
    float3 endPos = input.WorldPos;
    
    // Ray marching parameters
    int sampleCount = 32;
    float stepSize = length(endPos - startPos) / sampleCount;
    
    float3 totalScattering = float3(0, 0, 0);
    float opticalDepthR = 0.0;
    float opticalDepthM = 0.0;
    
    float3 currentPos = startPos;
    
    for (int i = 0; i < sampleCount; i++)
    {
        float height = length(currentPos - PlanetCenter);
        float density = exp(-(height - PlanetRadius) / 8000.0) * DensityMultiplier;
        
        // Sample light contribution
        float3 lightDir = normalize(SunDirection);
        float cosTheta = dot(viewDir, lightDir);
        
        // Optical depth calculation
        float sampleDistance = stepSize * density;
        opticalDepthR += sampleDistance * rayleigh;
        opticalDepthM += sampleDistance * mie;
        
        // Scattering calculation
        float3 tau = opticalDepthR + opticalDepthM;
        float3 attenuation = exp(-tau);
        
        // Phase functions
        float rayleighPhase = getRayleighPhase(cosTheta);
        float miePhase = getMiePhase(cosTheta, MieG);
        
        // Add scattering contribution
        totalScattering += attenuation * (rayleigh * rayleighPhase + mie * miePhase) * density * stepSize;
        
        currentPos += viewDir * stepSize;
    }
    
    // Apply sun intensity
    totalScattering *= SunIntensity;
    
    return float4(totalScattering, 1.0);
}

// Main shader entry point
PS_INPUT VS_Main(VS_INPUT input)
{
    if (AtmosphereMode == 0)
        return VS_HoffmanPreetham(input);
    else
        return VS_RayMarching(input);
}

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    if (AtmosphereMode == 0)
        return PS_HoffmanPreetham(input);
    else
        return PS_RayMarching(input);
}
```

#### **STEP 2: MINIMAL CHANGES TO labor_4.cpp**

**Add these headers at the top:**
```cpp
#include <DirectXMath.h>
#include <vector>
```

**Add atmosphere-related member variables to the main class:**
```cpp
// Add to class members
Microsoft::WRL::ComPtr<ID3D11VertexShader> m_atmosphereVS;
Microsoft::WRL::ComPtr<ID3D11PixelShader> m_atmospherePS;
Microsoft::WRL::ComPtr<ID3D11Buffer> m_atmosphereConstantBuffer;
Microsoft::WRL::ComPtr<ID3D11Buffer> m_skyDomeVB;
Microsoft::WRL::ComPtr<ID3D11InputLayout> m_atmosphereInputLayout;

// Atmosphere parameters structure
struct AtmosphereParams {
    DirectX::XMMATRIX View;
    DirectX::XMMATRIX Projection;
    DirectX::XMFLOAT3 CameraPos;
    DirectX::XMFLOAT3 SunDirection;
    DirectX::XMFLOAT3 PlanetCenter;
    float AtmosphereRadius;
    float PlanetRadius;
    DirectX::XMFLOAT3 RayleighScattering;
    DirectX::XMFLOAT3 MieScattering;
    float MieG;
    float SunIntensity;
    int AtmosphereMode; // 0 = Hoffman-Preetham, 1 = Ray Marching
    float DensityMultiplier;
    float PollutionLevel;
};

// Current atmosphere settings
AtmosphereParams m_atmosphereSettings;
bool m_enableAtmosphere = true;
```

**Add initialization function (call from Initialize()):**
```cpp
void InitializeAtmosphere()
{
    // Create sky dome geometry (simple sphere)
    std::vector<DirectX::XMFLOAT3> vertices;
    const int stacks = 20;
    const int slices = 40;
    const float radius = 10000.0f; // Large sphere for sky dome
    
    for (int i = 0; i <= stacks; ++i) {
        float phi = i * DirectX::XM_PI / stacks;
        for (int j = 0; j <= slices; ++j) {
            float theta = j * 2.0f * DirectX::XM_PI / slices;
            float x = radius * sinf(phi) * cosf(theta);
            float y = radius * cosf(phi);
            float z = radius * sinf(phi) * sinf(theta);
            vertices.push_back({x, y, z});
        }
    }
    
    // Create vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(DirectX::XMFLOAT3) * vertices.size();
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();
    m_device->CreateBuffer(&vbDesc, &vbData, m_skyDomeVB.GetAddressOf());
    
    // Load atmosphere shaders
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob;
    D3DCompileFromFile(L"Atmosphere.hlsl", nullptr, nullptr, "VS_Main", "vs_5_0", 0, 0, vsBlob.GetAddressOf(), nullptr);
    D3DCompileFromFile(L"Atmosphere.hlsl", nullptr, nullptr, "PS_Main", "ps_5_0", 0, 0, psBlob.GetAddressOf(), nullptr);
    
    m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_atmosphereVS.GetAddressOf());
    m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_atmospherePS.GetAddressOf());
    
    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    m_device->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_atmosphereInputLayout.GetAddressOf());
    
    // Create constant buffer
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = sizeof(AtmosphereParams);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_device->CreateBuffer(&cbDesc, nullptr, m_atmosphereConstantBuffer.GetAddressOf());
    
    // Initialize default parameters
    m_atmosphereSettings.CameraPos = {0, 0, 0};
    m_atmosphereSettings.SunDirection = {0.5f, -1.0f, 0.5f};
    m_atmosphereSettings.PlanetCenter = {0, -6371000.0f, 0}; // Earth radius in meters
    m_atmosphereSettings.AtmosphereRadius = 6471000.0f;
    m_atmosphereSettings.PlanetRadius = 6371000.0f;
    m_atmosphereSettings.RayleighScattering = {0.0058f, 0.0135f, 0.0331f}; // RGB scattering coefficients
    m_atmosphereSettings.MieScattering = {0.000399f, 0.000399f, 0.000399f};
    m_atmosphereSettings.MieG = 0.8f;
    m_atmosphereSettings.SunIntensity = 20.0f;
    m_atmosphereSettings.AtmosphereMode = 0; // Default to Hoffman-Preetham
    m_atmosphereSettings.DensityMultiplier = 1.0f;
    m_atmosphereSettings.PollutionLevel = 0.3f; // Slightly polluted by default
}
```

**Add rendering function (call before terrain rendering):**
```cpp
void RenderAtmosphere()
{
    if (!m_enableAtmosphere) return;
    
    // Update atmosphere parameters
    m_atmosphereSettings.CameraPos = m_camera.GetPosition();
    m_atmosphereSettings.View = m_camera.GetViewMatrix();
    m_atmosphereSettings.Projection = m_camera.GetProjectionMatrix();
    
    // Map constant buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_atmosphereConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &m_atmosphereSettings, sizeof(AtmosphereParams));
    m_context->Unmap(m_atmosphereConstantBuffer.Get(), 0);
    
    // Set up rendering state
    m_context->VSSetShader(m_atmosphereVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_atmospherePS.Get(), nullptr, 0);
    m_context->IASetInputLayout(m_atmosphereInputLayout.Get());
    
    // Set constant buffer
    m_context->VSSetConstantBuffers(0, 1, m_atmosphereConstantBuffer.GetAddressOf());
    m_context->PSSetConstantBuffers(0, 1, m_atmosphereConstantBuffer.GetAddressOf());
    
    // Set up vertex buffer
    UINT stride = sizeof(DirectX::XMFLOAT3);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_skyDomeVB.GetAddressOf(), &stride, &offset);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    
    // Disable depth write but enable depth test (sky dome should be behind everything)
    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // Don't write to depth buffer
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState;
    m_device->CreateDepthStencilState(&depthDesc, depthState.GetAddressOf());
    m_context->OMSetDepthStencilState(depthState.Get(), 1);
    
    // Render sky dome
    m_context->Draw(vertices.size(), 0);
    
    // Restore depth state
    m_context->OMSetDepthStencilState(nullptr, 1);
}
```

**Add IMGUI controls function:**
```cpp
void RenderAtmosphereGUI()
{
    if (ImGui::CollapsingHeader("Atmosphere Settings"))
    {
        ImGui::Checkbox("Enable Atmosphere", &m_enableAtmosphere);
        
        ImGui::Text("Rendering Mode:");
        const char* modes[] = { "Hoffman-Preetham (Ground Level)", "Ray Marching (High Altitude)" };
        ImGui::Combo("Atmosphere Mode", &m_atmosphereSettings.AtmosphereMode, modes, 2);
        
        ImGui::Separator();
        ImGui::Text("Environmental Parameters:");
        
        ImGui::SliderFloat("Pollution Level", &m_atmosphereSettings.PollutionLevel, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Density Multiplier", &m_atmosphereSettings.DensityMultiplier, 0.1f, 5.0f, "%.2f");
        ImGui::SliderFloat("Sun Intensity", &m_atmosphereSettings.SunIntensity, 0.0f, 100.0f, "%.1f");
        
        ImGui::Separator();
        ImGui::Text("Scattering Parameters:");
        
        ImGui::SliderFloat3("Rayleigh Scattering", &m_atmosphereSettings.RayleighScattering.x, 0.0f, 0.1f, "%.4f");
        ImGui::SliderFloat3("Mie Scattering", &m_atmosphereSettings.MieScattering.x, 0.0f, 0.01f, "%.4f");
        ImGui::SliderFloat("Mie G (Phase)", &m_atmosphereSettings.MieG, 0.0f, 0.99f, "%.2f");
        
        ImGui::Separator();
        ImGui::Text("Physical Parameters:");
        
        ImGui::SliderFloat("Planet Radius (km)", &m_atmosphereSettings.PlanetRadius, 6000000.0f, 7000000.0f, "%.0f");
        ImGui::SliderFloat("Atmosphere Height (km)", &m_atmosphereSettings.AtmosphereRadius, 6400000.0f, 7000000.0f, "%.0f");
        
        ImGui::Separator();
        ImGui::Text("Sun Direction:");
        ImGui::SliderFloat3("Sun Direction", &m_atmosphereSettings.SunDirection.x, -1.0f, 1.0f, "%.2f");
        
        // Normalize sun direction
        DirectX::XMVECTOR sunDir = DirectX::XMLoadFloat3(&m_atmosphereSettings.SunDirection);
        sunDir = DirectX::XMVector3Normalize(sunDir);
        DirectX::XMStoreFloat3(&m_atmosphereSettings.SunDirection, sunDir);
        
        // Presets for clean/dirty atmosphere
        if (ImGui::Button("Clean Atmosphere (Mountain)"))
        {
            m_atmosphereSettings.PollutionLevel = 0.1f;
            m_atmosphereSettings.DensityMultiplier = 0.8f;
            m_atmosphereSettings.RayleighScattering = {0.0065f, 0.015f, 0.035f};
            m_atmosphereSettings.MieScattering = {0.0002f, 0.0002f, 0.0002f};
        }
        
        if (ImGui::Button("Dirty Atmosphere (City)"))
        {
            m_atmosphereSettings.PollutionLevel = 0.8f;
            m_atmosphereSettings.DensityMultiplier = 1.5f;
            m_atmosphereSettings.RayleighScattering = {0.004f, 0.01f, 0.025f};
            m_atmosphereSettings.MieScattering = {0.0006f, 0.0006f, 0.0006f};
        }
        
        if (ImGui::Button("Space View"))
        {
            m_atmosphereSettings.AtmosphereMode = 1; // Ray Marching
            m_atmosphereSettings.PollutionLevel = 0.0f;
            m_atmosphereSettings.DensityMultiplier = 1.0f;
        }
    }
}
```

**Integration points in existing code:**
1. **In Initialize() function:**
   ```cpp
   // After terrain initialization
   InitializeAtmosphere();
   ```

2. **In Render() function (before terrain rendering):**
   ```cpp
   // Clear render target and depth stencil
   m_context->ClearRenderTargetView(m_renderTargetView.Get(), DirectX::Colors::SkyBlue);
   m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
   
   // Render atmosphere first (background)
   RenderAtmosphere();
   
   // Then render terrain (as existing code)
   // ... existing terrain rendering code ...
   ```

3. **In RenderUI() function (add to existing IMGUI):**
   ```cpp
   // Existing IMGUI code...
   
   // Add atmosphere controls
   RenderAtmosphereGUI();
   
   // Existing IMGUI code...
   ```

#### **STEP 3: PERFORMANCE OPTIMIZATION**

**Add these optimizations to maintain performance:**
```cpp
// In RenderAtmosphere() function, add adaptive sampling
if (m_atmosphereSettings.AtmosphereMode == 1) // Ray Marching
{
    // Reduce samples when camera is far from atmosphere
    float cameraHeight = length(m_camera.GetPosition() - m_atmosphereSettings.PlanetCenter);
    int maxSamples = cameraHeight > m_atmosphereSettings.AtmosphereRadius ? 16 : 32;
    // Pass this to shader via constant buffer
}

// Add culling for sky dome when not visible
if (m_camera.GetPosition().y > m_atmosphereSettings.AtmosphereRadius * 0.9f)
{
    // Skip rendering if camera is above atmosphere
    return;
}
```

#### **STEP 4: INTEGRATION WITH TERRAIN SHADER**

**Modify `Terrain.hlsl` to account for atmospheric extinction:**
```hlsl
// Add to Terrain.hlsl constant buffer
cbuffer AtmosphereIntegration : register(b1)
{
    float3 CameraPos;
    float3 SunDirection;
    float AtmosphereRadius;
    float PlanetRadius;
    float PollutionLevel;
    float DensityMultiplier;
    int AtmosphereMode; // 0 = Hoffman-Preetham, 1 = Ray Marching (use same mode)
};

// Add extinction calculation function
float3 CalculateAtmosphericExtinction(float3 worldPos, float3 viewDir)
{
    if (AtmosphereMode == 0)
    {
        // Hoffman-Preetham extinction
        float height = length(worldPos - float3(0, -PlanetRadius, 0));
        float opticalDepth = exp(-(height - PlanetRadius) / 8000.0) * DensityMultiplier;
        float3 rayleigh = float3(0.0058, 0.0135, 0.0331) * (1.0 + PollutionLevel * 2.0);
        float3 mie = float3(0.000399, 0.000399, 0.000399) * (1.0 + PollutionLevel * 4.0);
        return exp(-(rayleigh + mie) * opticalDepth);
    }
    else
    {
        // Simplified extinction for Ray Marching mode
        float3 toCamera = normalize(CameraPos - worldPos);
        float cosTheta = dot(toCamera, normalize(SunDirection));
        float extinctionFactor = exp(-length(worldPos - CameraPos) / 10000.0);
        return float3(extinctionFactor, extinctionFactor, extinctionFactor);
    }
}

// In pixel shader, apply extinction to final color
float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    // ... existing terrain shading code ...
    
    float3 extinction = CalculateAtmosphericExtinction(input.WorldPos, input.ViewDir);
    finalColor.rgb *= extinction;
    
    return float4(finalColor.rgb, 1.0);
}
```

#### **STEP 5: COMPILATION AND SHADER MANAGEMENT**

**Add shader compilation and error handling:**
```cpp
// Helper function for shader compilation
bool CompileShaderFromFile(const wchar_t* fileName, const char* entryPoint, const char* profile, ID3DBlob** blob)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DCompileFromFile(fileName, nullptr, nullptr, entryPoint, profile, flags, 0, blob, &errorBlob);

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }
    if (errorBlob) errorBlob->Release();
    return true;
}
```

#### **STEP 6: TESTING AND VALIDATION**

**Add debug visualization modes:**
```cpp
// In IMGUI controls, add debug options
static int debugMode = 0;
const char* debugModes[] = { "Normal", "Rayleigh Only", "Mie Only", "Optical Depth" };
ImGui::Combo("Debug Mode", &debugMode, debugModes, 4);

// Pass debugMode to shader via constant buffer
m_atmosphereSettings.DebugMode = debugMode;
```

---

### 🎨 **VISUAL TARGETS**

| **Mode** | **Clean Atmosphere** | **Dirty Atmosphere** | **Space View** |
|----------|---------------------|---------------------|---------------|
| **Hoffman-Preetham** | Deep blue sky, sharp sun disk | Hazy white sky, diffused sun glow | Not applicable |
| **Ray Marching** | Earth with blue atmospheric rim | Earth with thick white atmospheric rim | Space view with atmospheric scattering |

---

### ⚠️ **CRITICAL IMPLEMENTATION NOTES**

1. **File Management**: Create `Atmosphere.hlsl` in the same directory as `Terrain.hlsl`
2. **Minimal Main File Changes**: Only add 3 function calls to existing `labor_4.cpp`:
   - `InitializeAtmosphere()` in initialization
   - `RenderAtmosphere()` before terrain rendering
   - `RenderAtmosphereGUI()` in UI rendering
3. **Performance**: Sky dome uses triangle strip for efficiency, adaptive sampling for ray marching
4. **Integration**: Terrain shader modified to account for atmospheric extinction
5. **Parameter Mapping**: Pollution level automatically adjusts scattering coefficients for realistic transitions

---

### 📦 **FILE STRUCTURE AFTER IMPLEMENTATION**

```
Project/
├── labor_4.cpp          # Minimal modifications (only 3 function calls added)
├── Terrain.hlsl         # Small additions for atmospheric extinction
├── Atmosphere.hlsl      # NEW FILE - Complete atmosphere shader implementation
└── [Other existing files remain unchanged]
```

---

### ✅ **SUCCESS CRITERIA**

1. **Mode Switching**: Toggle between Hoffman-Preetham and Ray Marching with IMGUI checkbox
2. **Real-time Parameters**: Adjust pollution, density, sun intensity and see immediate visual changes
3. **Terrain Integration**: Terrain correctly shows atmospheric extinction (distant objects fade to sky color)
4. **Performance**: Maintains 60+ FPS on modern hardware
5. **Visual Quality**: Clean atmosphere shows blue sky, dirty atmosphere shows hazy white sky, space view shows atmospheric rim lighting

This implementation provides a complete atmospheric rendering system while respecting the constraint of minimal changes to the main file structure. The atmosphere integrates seamlessly with the existing terrain rendering pipeline and provides intuitive real-time controls through IMGUI.