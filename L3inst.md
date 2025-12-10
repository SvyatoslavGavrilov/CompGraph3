# Cursor Prompt: Complete Atmospheric Scattering System with ImGui Controls

## Task Description
Implement a comprehensive atmospheric scattering system that simulates realistic sky appearance with real-time parameter controls using ImGui. The system should support both ground-level and high-altitude viewing, with dynamic sun positioning, volumetric fog, and adjustable "dirtiness" effects to simulate clean vs polluted atmospheres.

## Technical Requirements

### 1. Global Shader Settings Structure
Create a unified `AtmosphereSettings` structure that contains all necessary parameters for the atmospheric shader:

```cpp
struct AtmosphereSettings {
    // Physical constants
    float planetRadius = 6360.0f;           // Earth radius in km
    float atmosphereRadius = 6420.0f;       // Atmosphere radius in km
    
    // Scattering coefficients
    glm::vec3 rayleighScattering = glm::vec3(5.5e-6f, 13.5e-6f, 22.0e-6f); // RGB Rayleigh coefficients
    float rayleighScaleHeight = 8.4f;        // Scale height for Rayleigh scattering (km)
    
    glm::vec3 mieScattering = glm::vec3(2.0e-5f); // Base Mie scattering (gray)
    float mieScaleHeight = 1.25f;            // Scale height for Mie scattering (km)
    float mieG = 0.8f;                       // Mie scattering asymmetry parameter
    
    // Sun parameters
    glm::vec3 sunDirection = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)); // Default zenith
    float sunIntensity = 20.0f;              // Sun brightness (ESun parameter)
    
    // Fog parameters
    float fogDensity = 0.01f;                // Base fog density
    float heightFogFalloff = 0.001f;         // Height falloff for exponential height fog
    float fogHeightOffset = 0.0f;            // Height offset for fog base
    
    // Dirtiness/pollution controls
    float pollutionFactor = 0.0f;            // 0.0 = clean, 1.0 = heavily polluted
    float hazeDensity = 0.0f;                // Additional haze density
    float dustScattering = 0.0f;             // Dust/aerosol scattering contribution
    
    // Runtime optimization
    int rayMarchingSteps = 16;               // Number of steps for ray marching
    bool useAnalyticalApprox = true;         // Use Hoffman-Preetham for ground level
    
    // Derived values (updated automatically)
    float cameraHeight;
    bool cameraInSpace;
};
```

### 2. Atmosphere Shader Implementation
Implement the atmospheric scattering shader using techniques from NVIDIA GPU Gems Chapter 16:

- **Vertex Shader**: Calculate ray intersections with atmosphere boundaries, initial scattering values
- **Fragment Shader**: Perform ray marching for accurate light integration when needed, otherwise use analytical approximation
- **Scale Function**: Implement the approximation function for optical path length:
  ```glsl
  float scale(float cosTheta) {
      float x = 1.0 - cosTheta;
      return scaleDepth * exp(-0.00287 + x*(0.459 + x*(3.83 + x*(-6.80 + x*5.25))));
  }
  ```

- **Ray Marching**: For high-altitude cameras, implement proper ray marching through the atmosphere volume
- **Exponential Height Fog**: For ground-level scenes, implement exponential height fog as described in Unreal Engine documentation

### 3. ImGui Control Panel
Create a comprehensive ImGui interface that allows real-time adjustment of all atmospheric parameters:

```cpp
void AtmosphereSystem::renderImGuiControls() {
    if (ImGui::Begin("Atmosphere Controls")) {
        // Sun Controls
        ImGui::SeparatorText("☀️ Sun Controls");
        ImGui::SliderFloat("Sun Azimuth", &sunAzimuth, 0.0f, 360.0f, "%.1f°");
        ImGui::SliderFloat("Sun Altitude", &sunAltitude, -90.0f, 90.0f, "%.1f°");
        ImGui::SliderFloat("Sun Intensity", &settings.sunIntensity, 0.0f, 100.0f, "%.1f");
        
        // Atmospheric Scattering
        ImGui::SeparatorText("🌤️ Atmospheric Scattering");
        ImGui::SliderFloat3("Rayleigh Scattering", &settings.rayleighScattering[0], 0.0f, 50.0e-6f, "%.2e");
        ImGui::SliderFloat("Rayleigh Scale Height", &settings.rayleighScaleHeight, 1.0f, 20.0f, "%.1f km");
        
        ImGui::SliderFloat3("Mie Scattering", &settings.mieScattering[0], 0.0f, 100.0e-5f, "%.2e");
        ImGui::SliderFloat("Mie Scale Height", &settings.mieScaleHeight, 0.5f, 5.0f, "%.1f km");
        ImGui::SliderFloat("Mie G", &settings.mieG, 0.0f, 0.999f, "%.3f"); // Cannot be 1.0
        
        // Fog Controls
        ImGui::SeparatorText("🌫️ Fog Controls");
        ImGui::SliderFloat("Fog Density", &settings.fogDensity, 0.0f, 0.1f, "%.4f");
        ImGui::SliderFloat("Height Falloff", &settings.heightFogFalloff, 0.0f, 0.01f, "%.5f");
        ImGui::SliderFloat("Fog Height Offset", &settings.fogHeightOffset, -10.0f, 10.0f, "%.1f km");
        
        // Dirtiness/Pollution Controls
        ImGui::SeparatorText("🏭 Dirtiness/Pollution");
        ImGui::SliderFloat("Pollution Factor", &settings.pollutionFactor, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Haze Density", &settings.hazeDensity, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Dust Scattering", &settings.dustScattering, 0.0f, 50.0e-5f, "%.2e");
        
        // Presets
        ImGui::SeparatorText("🎯 Presets");
        if (ImGui::Button("Clear Sky")) {
            settings.pollutionFactor = 0.0f;
            settings.hazeDensity = 0.0f;
            settings.dustScattering = 0.0f;
            settings.mieScattering = glm::vec3(2.0e-5f);
        }
        if (ImGui::Button("Hazy Day")) {
            settings.pollutionFactor = 0.4f;
            settings.hazeDensity = 0.6f;
            settings.mieScattering = glm::vec3(8.0e-5f);
        }
        if (ImGui::Button("Heavy Pollution")) {
            settings.pollutionFactor = 0.9f;
            settings.hazeDensity = 1.0f;
            settings.dustScattering = 30.0e-5f;
            settings.mieScattering = glm::vec3(20.0e-5f);
        }
        
        // Performance Controls
        ImGui::SeparatorText("⚡ Performance");
        ImGui::SliderInt("Ray Marching Steps", &settings.rayMarchingSteps, 4, 64);
        ImGui::Checkbox("Use Analytical Approx", &settings.useAnalyticalApprox);
        
        // Real-time update
        updateAtmosphere();
    }
    ImGui::End();
}
```

### 4. Dirtiness Effect Implementation
The "dirtiness" effect should dynamically modify multiple parameters to create realistic polluted/hazy atmospheres:

```cpp
void AtmosphereSystem::applyDirtinessEffect() {
    // Increase Mie scattering based on pollution factor
    float pollutionFactor = settings.pollutionFactor;
    
    // Scale Mie scattering coefficients (pollution adds more aerosols)
    settings.mieScattering *= (1.0f + pollutionFactor * 4.0f);
    
    // Increase haze density
    settings.fogDensity += settings.hazeDensity * pollutionFactor * 0.05f;
    
    // Add dust scattering contribution (reddish tint for pollution)
    glm::vec3 dustColor(0.8f, 0.6f, 0.4f); // Reddish-brown dust color
    settings.mieScattering += dustColor * settings.dustScattering * pollutionFactor;
    
    // Reduce Rayleigh scattering dominance (pollution masks blue sky)
    settings.rayleighScattering *= (1.0f - pollutionFactor * 0.3f);
    
    // Adjust Mie asymmetry parameter for more forward scattering (haze)
    settings.mieG = glm::clamp(0.75f + pollutionFactor * 0.15f, 0.0f, 0.99f);
    
    // Increase overall extinction
    float extinctionFactor = 1.0f + pollutionFactor * 2.0f;
    settings.rayleighScaleHeight /= extinctionFactor;
    settings.mieScaleHeight /= extinctionFactor;
}
```

### 5. Dynamic Sun Position
Implement time-of-day simulation with smooth sun movement:

```cpp
void AtmosphereSystem::updateSunPosition(float timeOfDay) {
    // timeOfDay: 0-24 hours
    float angle = (timeOfDay / 24.0f) * glm::two_pi<float>();
    
    // Calculate sun direction based on time
    float altitude = glm::sin((timeOfDay - 12.0f) * (glm::pi<float>() / 12.0f)) * 0.8f; // -0.8 to 0.8
    float azimuth = angle;
    
    // Convert spherical to cartesian coordinates
    float x = glm::cos(altitude) * glm::sin(azimuth);
    float y = glm::sin(altitude);
    float z = glm::cos(altitude) * glm::cos(azimuth);
    
    settings.sunDirection = glm::normalize(glm::vec3(x, y, z));
    
    // Adjust sun intensity based on time of day
    float intensityFactor = glm::abs(glm::sin(altitude * 0.5f + glm::pi<float>() * 0.25f));
    settings.sunIntensity = 15.0f + intensityFactor * 25.0f;
    
    // Adjust atmospheric color based on sun position
    if (altitude < 0.0f) {
        // Night time - reduce scattering
        settings.rayleighScattering *= 0.1f;
        settings.mieScattering *= 0.1f;
    } else if (glm::abs(altitude) < 0.3f) {
        // Sunrise/sunset - enhance red scattering
        settings.rayleighScattering *= glm::vec3(0.8f, 0.6f, 1.2f);
    }
}
```

### 6. Integration Requirements
- The system must work with both forward and deferred rendering pipelines
- Implement proper depth testing and blending for atmospheric effects
- Support HDR rendering with proper exposure compensation
- Include temporal anti-aliasing for ray marching artifacts
- Optimize shader performance with level-of-detail techniques
- Provide fallback to analytical approximation for low-end hardware

### 7. Testing and Validation
- Create test scenes showing atmosphere from ground level, aircraft altitude, and space
- Verify color accuracy against reference photos for different times of day
- Test performance impact with different ray marching step counts
- Validate that "dirtiness" controls create realistic polluted/hazy conditions
- Ensure smooth transitions between different atmospheric states

### 8. Documentation
- Comment all shader code with physics explanations
- Document the relationship between physical parameters and visual results
- Provide usage examples for common atmospheric conditions (clear day, hazy day, sunset, pollution)
- Include performance guidelines for different hardware targets

## Expected Outcome
A complete, real-time atmospheric scattering system that can be controlled through an intuitive ImGui interface, allowing artists and developers to create realistic sky conditions ranging from crystal-clear mountain air to heavily polluted urban environments, with dynamic sun positioning and proper volumetric fog integration. The system should be physically-based but allow artistic control over all parameters.