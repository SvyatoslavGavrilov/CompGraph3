# Atmospheric Scattering Implementation Plan

## [[Baseline.cpp]] Atmospheric Integration

### Overview
This plan adds a complete atmospheric scattering system to the rendering pipeline. The implementation follows the Hoffman-Preetham approach for ground-level rendering with ray marching support for high-altitude views, based on the lecture material from [[Light Volume Scattering]].

### Prerequisites
- OpenGL 4.3+ or DirectX 11+ support
- GLM math library for vector/matrix operations
- ImGui for real-time parameter adjustment
- Existing rendering pipeline with camera controls

---

## Step 1: Add Atmospheric Constants and Parameters

```cpp
// Add after existing includes but before function definitions
#include <imgui.h> // For real-time parameter adjustment

// Atmospheric scattering constants
const float PI = 3.14159265359f;
const float DEG_TO_RAD = PI / 180.0f;

// [[Atmospheric Parameters]]
struct AtmosphereParameters {
    // Earth-like atmosphere defaults
    float planetRadius = 6371000.0f;    // Earth radius in meters
    float atmosphereRadius = 6471000.0f; // Atmosphere radius (100km above surface)
    
    // Rayleigh scattering coefficients (clear atmosphere)
    glm::vec3 rayleighCoeff = glm::vec3(5.5e-6f, 13.5e-6f, 33.1e-6f); // RGB in m^-1
    
    // Mie scattering coefficients (hazy atmosphere)
    glm::vec3 mieCoeff = glm::vec3(2.0e-5f); // Gray value for haze
    
    // Optical properties
    float rayleighScaleHeight = 8000.0f;  // Height where Rayleigh density falls to 1/e
    float mieScaleHeight = 1200.0f;       // Height where Mie density falls to 1/e
    float mieAnisotropy = 0.76f;          // Forward scattering bias (0.76 for Earth)
    
    // Sun properties
    glm::vec3 sunDirection = glm::normalize(glm::vec3(0.0f, -0.5f, -1.0f)); // Default sun position
    float sunIntensity = 20.0f;           // Sun brightness
    
    // Runtime adjustment parameters
    float atmosphereDensity = 1.0f;      // Global density multiplier (1.0 = Earth normal)
    float pollutionLevel = 0.0f;          // 0.0 = clean, 1.0 = heavily polluted
    float humidityLevel = 0.3f;           // 0.0 = dry, 1.0 = very humid
    
    // Recalculate coefficients based on pollution/humidity
    void updateCoefficients() {
        // More pollution increases Mie scattering (haze)
        float pollutionFactor = 1.0f + pollutionLevel * 4.0f;
        // More humidity increases both scattering types but affects Mie more
        float humidityFactor = 1.0f + humidityLevel * 2.0f;
        
        // Base Earth coefficients adjusted by density and pollution
        rayleighCoeff = glm::vec3(5.5e-6f, 13.5e-6f, 33.1e-6f) * atmosphereDensity * humidityFactor;
        mieCoeff = glm::vec3(2.0e-5f) * atmosphereDensity * pollutionFactor * humidityFactor;
        
        // Adjust scale heights based on conditions
        rayleighScaleHeight = 8000.0f * (1.0f - pollutionLevel * 0.3f);
        mieScaleHeight = 1200.0f * (1.0f + pollutionLevel * 0.5f);
    }
};

// Global atmosphere instance
AtmosphereParameters atmosphere;
```

---

## Step 2: Add Shader Programs and Uniform Locations

```cpp
// Add after existing shader declarations
GLuint atmosphereShaderProgram = 0;
GLuint skydomeShaderProgram = 0;

// Uniform locations for atmosphere shader
struct AtmosphereUniforms {
    GLint sunDirection;
    GLint planetRadius;
    GLint atmosphereRadius;
    GLint rayleighCoeff;
    GLint mieCoeff;
    GLint rayleighScaleHeight;
    GLint mieScaleHeight;
    GLint mieAnisotropy;
    GLint sunIntensity;
    GLint cameraHeight;
    GLint viewMatrix;
    GLint projectionMatrix;
};

// Uniform locations for skydome shader  
struct SkydomeUniforms {
    GLint sunDirection;
    GLint planetRadius;
    GLint atmosphereRadius;
    GLint rayleighCoeff;
    GLint mieCoeff;
    GLint rayleighScaleHeight;
    GLint mieScaleHeight;
    GLint mieAnisotropy;
    GLint sunIntensity;
    GLint cameraPosition;
    GLint viewMatrix;
    GLint projectionMatrix;
};

AtmosphereUniforms atmosphereUniforms;
SkydomeUniforms skydomeUniforms;

// Add after existing VAO/VBO declarations
GLuint skydomeVAO = 0;
GLuint skydomeVBO = 0;
GLuint skydomeIBO = 0;
int skydomeIndexCount = 0;
```

---

## Step 3: Add Skydome Mesh Generation Function

```cpp
// Add before main() function
void generateSkydome() {
    /* 
    [[Skydome Geometry]]
    Creates a hemisphere mesh for rendering the sky.
    Uses icosahedron subdivision for even vertex distribution.
    */
    
    const int subdivisions = 3; // Quality level (3 = good balance)
    const float radius = atmosphere.atmosphereRadius + 1000.0f; // Slightly larger than atmosphere
    
    // Icosahedron vertices (12 vertices)
    std::vector<glm::vec3> vertices = {
        {-0.525731f, 0.000000f, 0.850651f}, {0.525731f, 0.000000f, 0.850651f},
        {-0.525731f, 0.000000f, -0.850651f}, {0.525731f, 0.000000f, -0.850651f},
        {0.000000f, 0.850651f, 0.525731f}, {0.000000f, 0.850651f, -0.525731f},
        {0.000000f, -0.850651f, 0.525731f}, {0.000000f, -0.850651f, -0.525731f},
        {0.850651f, 0.525731f, 0.000000f}, {-0.850651f, 0.525731f, 0.000000f},
        {0.850651f, -0.525731f, 0.000000f}, {-0.850651f, -0.525731f, 0.000000f}
    };
    
    // Icosahedron triangles (20 faces)
    std::vector<glm::uvec3> triangles = {
        {0, 4, 1}, {0, 9, 4}, {9, 5, 4}, {4, 5, 8}, {4, 8, 1},
        {8, 10, 1}, {8, 3, 10}, {5, 3, 8}, {5, 2, 3}, {2, 7, 3},
        {7, 10, 3}, {7, 6, 10}, {7, 11, 6}, {11, 0, 6}, {0, 1, 6},
        {6, 1, 10}, {9, 0, 11}, {9, 11, 2}, {9, 2, 5}, {7, 2, 11}
    };
    
    // Normalize vertices to unit sphere
    for (auto& v : vertices) {
        v = glm::normalize(v);
    }
    
    // Subdivide triangles
    for (int i = 0; i < subdivisions; i++) {
        std::vector<glm::uvec3> newTriangles;
        size_t originalCount = triangles.size();
        
        for (size_t j = 0; j < originalCount; j++) {
            glm::uvec3 tri = triangles[j];
            glm::vec3 v0 = vertices[tri.x];
            glm::vec3 v1 = vertices[tri.y];
            glm::vec3 v2 = vertices[tri.z];
            
            // Calculate midpoints
            glm::vec3 mid01 = glm::normalize((v0 + v1) * 0.5f);
            glm::vec3 mid12 = glm::normalize((v1 + v2) * 0.5f);
            glm::vec3 mid20 = glm::normalize((v2 + v0) * 0.5f);
            
            // Add new vertices
            size_t idx01 = vertices.size(); vertices.push_back(mid01);
            size_t idx12 = vertices.size(); vertices.push_back(mid12);
            size_t idx20 = vertices.size(); vertices.push_back(mid20);
            
            // Create 4 new triangles
            newTriangles.push_back({tri.x, idx01, idx20});
            newTriangles.push_back({tri.y, idx12, idx01});
            newTriangles.push_back({tri.z, idx20, idx12});
            newTriangles.push_back({idx01, idx12, idx20});
        }
        
        triangles = newTriangles;
    }
    
    // Convert to hemisphere (only keep vertices with y >= 0)
    std::vector<glm::vec3> hemisphereVertices;
    std::vector<GLuint> hemisphereIndices;
    
    std::unordered_map<size_t, size_t> vertexMap;
    
    for (const auto& tri : triangles) {
        glm::vec3 v0 = vertices[tri.x];
        glm::vec3 v1 = vertices[tri.y];
        glm::vec3 v2 = vertices[tri.z];
        
        // Skip triangles completely below horizon
        if (v0.y < 0.0f && v1.y < 0.0f && v2.y < 0.0f) continue;
        
        // Clip triangles that cross horizon
        // Simple approach: only include vertices above horizon
        if (v0.y >= 0.0f || v1.y >= 0.0f || v2.y >= 0.0f) {
            // Get or create vertex indices
            auto getVertexIndex = [&](size_t srcIdx, const glm::vec3& pos) {
                if (vertexMap.find(srcIdx) == vertexMap.end()) {
                    hemisphereVertices.push_back(glm::vec3(pos.x, std::max(0.0f, pos.y), pos.z));
                    vertexMap[srcIdx] = hemisphereVertices.size() - 1;
                }
                return (GLuint)vertexMap[srcIdx];
            };
            
            hemisphereIndices.push_back(getVertexIndex(tri.x, v0));
            hemisphereIndices.push_back(getVertexIndex(tri.y, v1));
            hemisphereIndices.push_back(getVertexIndex(tri.z, v2));
        }
    }
    
    // Scale vertices to skydome radius
    for (auto& v : hemisphereVertices) {
        v *= radius;
    }
    
    // Create VAO/VBO/IBO
    glGenVertexArrays(1, &skydomeVAO);
    glBindVertexArray(skydomeVAO);
    
    glGenBuffers(1, &skydomeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, skydomeVBO);
    glBufferData(GL_ARRAY_BUFFER, hemisphereVertices.size() * sizeof(glm::vec3), 
                 hemisphereVertices.data(), GL_STATIC_DRAW);
    
    glGenBuffers(1, &skydomeIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skydomeIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, hemisphereIndices.size() * sizeof(GLuint),
                 hemisphereIndices.data(), GL_STATIC_DRAW);
    
    // Vertex attribute setup
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    
    skydomeIndexCount = (int)hemisphereIndices.size();
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    std::cout << "Skydome generated with " << hemisphereVertices.size() 
              << " vertices and " << skydomeIndexCount << " indices" << std::endl;
}
```

---

## Step 4: Add Shader Source Code

```cpp
// Add after generateSkydome() function
const char* atmosphereVertexShaderSource = R"(
#version 430 core
layout (location = 0) in vec3 position;

uniform vec3 sunDirection;
uniform float planetRadius;
uniform float atmosphereRadius;
uniform vec3 rayleighCoeff;
uniform vec3 mieCoeff;
uniform float rayleighScaleHeight;
uniform float mieScaleHeight;
uniform float mieAnisotropy;
uniform float sunIntensity;
uniform float cameraHeight;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

out vec3 fragWorldPos;
out vec3 fragRayleighColor;
out vec3 fragMieColor;
out float fragOpticalDepth;

// [[Rayleigh Phase Function]]
// Describes angular distribution of Rayleigh scattered light
// Isotropic scattering (equal in all directions)
float rayleighPhaseFunction(float cosTheta) {
    return (3.0 / (16.0 * 3.14159265359)) * (1.0 + cosTheta * cosTheta);
}

// [[Mie Phase Function]]
// Describes angular distribution of Mie scattered light
// Forward-scattering dominant (g > 0)
float miePhaseFunction(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 / (4.0 * 3.14159265359)) * ((1.0 - g2) * (1.0 + cosTheta * cosTheta)) / 
           (pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5) * (2.0 + g2));
}

// [[Optical Depth Calculation]]
// Calculates the optical depth along a ray through the atmosphere
float calculateOpticalDepth(vec3 startPoint, vec3 direction, float rayLength, 
                           float scaleHeight, float planetRadius) {
    const int steps = 16; // Number of samples for integration
    float opticalDepth = 0.0;
    float stepSize = rayLength / float(steps);
    
    for (int i = 0; i < steps; i++) {
        float t = (float(i) + 0.5) * stepSize;
        vec3 samplePoint = startPoint + direction * t;
        float height = length(samplePoint) - planetRadius;
        
        // Atmospheric density decays exponentially with height
        float density = exp(-height / scaleHeight);
        opticalDepth += density * stepSize;
    }
    
    return opticalDepth;
}

void main() {
    fragWorldPos = position;
    
    // Transform vertex to view space
    vec4 viewPos = viewMatrix * vec4(position, 1.0);
    vec3 viewDir = normalize(viewPos.xyz);
    
    // Calculate optical depth from camera to vertex
    float cameraToVertexDistance = length(viewPos.xyz);
    float rayleighDepth = calculateOpticalDepth(vec3(0, cameraHeight, 0), viewDir, 
                                              cameraToVertexDistance, rayleighScaleHeight, 
                                              planetRadius);
    float mieDepth = calculateOpticalDepth(vec3(0, cameraHeight, 0), viewDir, 
                                          cameraToVertexDistance, mieScaleHeight, 
                                          planetRadius);
    
    fragOpticalDepth = rayleighDepth; // Store for fragment shader
    
    // Calculate extinction (light loss due to scattering and absorption)
    vec3 extinction = exp(-(rayleighCoeff * rayleighDepth + mieCoeff * mieDepth));
    
    // Calculate in-scattering from the sun
    float cosTheta = dot(normalize(viewDir), normalize(sunDirection));
    float rayleighPhase = rayleighPhaseFunction(cosTheta);
    float miePhase = miePhaseFunction(cosTheta, mieAnisotropy);
    
    // [[In-scattering Calculation]]
    // Light added to the ray from the sun's direction
    vec3 sunRayDir = normalize(sunDirection);
    float sunRayLength = sqrt(atmosphereRadius * atmosphereRadius - 
                            planetRadius * planetRadius);
    
    float sunRayleighDepth = calculateOpticalDepth(position, sunRayDir, 
                                                  sunRayLength, rayleighScaleHeight, 
                                                  planetRadius);
    float sunMieDepth = calculateOpticalDepth(position, sunRayDir, 
                                             sunRayLength, mieScaleHeight, 
                                             planetRadius);
    
    vec3 sunExtinction = exp(-(rayleighCoeff * sunRayleighDepth + 
                             mieCoeff * sunMieDepth));
    
    // Combine phase functions with extinction and sun intensity
    fragRayleighColor = rayleighPhase * rayleighCoeff * sunExtinction * sunIntensity;
    fragMieColor = miePhase * mieCoeff * sunExtinction * sunIntensity;
    
    // Apply camera height offset
    vec4 worldPos = vec4(position + vec3(0, cameraHeight, 0), 1.0);
    gl_Position = projectionMatrix * viewMatrix * worldPos;
}
)";

const char* atmosphereFragmentShaderSource = R"(
#version 430 core
in vec3 fragWorldPos;
in vec3 fragRayleighColor;
in vec3 fragMieColor;
in float fragOpticalDepth;

uniform vec3 rayleighCoeff;
uniform vec3 mieCoeff;
uniform float rayleighScaleHeight;
uniform float mieScaleHeight;
uniform float cameraHeight;
uniform float planetRadius;

out vec4 fragColor;

void main() {
    // Calculate final color with both Rayleigh and Mie scattering
    vec3 rayleighContribution = fragRayleighColor;
    vec3 mieContribution = fragMieColor;
    
    // Combine contributions and apply extinction
    vec3 finalColor = rayleighContribution + mieContribution;
    
    // [[Atmospheric Extinction]]
    // Apply the Beer-Lambert-Bouguer law for light attenuation
    float extinctionFactor = exp(-fragOpticalDepth * (length(rayleighCoeff) + length(mieCoeff)));
    finalColor *= extinctionFactor;
    
    // Add some ambient light to avoid completely black areas
    finalColor += vec3(0.05, 0.07, 0.1) * (1.0 - extinctionFactor);
    
    // Gamma correction
    finalColor = pow(finalColor, vec3(1.0/2.2));
    
    // Clamp to valid color range
    finalColor = clamp(finalColor, 0.0, 1.0);
    
    fragColor = vec4(finalColor, 1.0);
}
)";

const char* skydomeVertexShaderSource = R"(
#version 430 core
layout (location = 0) in vec3 position;

uniform vec3 sunDirection;
uniform float planetRadius;
uniform float atmosphereRadius;
uniform vec3 rayleighCoeff;
uniform vec3 mieCoeff;
uniform float rayleighScaleHeight;
uniform float mieScaleHeight;
uniform float mieAnisotropy;
uniform float sunIntensity;
uniform vec3 cameraPosition;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

out vec3 fragWorldPos;
out vec3 fragSunDirection;
out float fragCameraHeight;

void main() {
    fragWorldPos = position;
    fragSunDirection = sunDirection;
    fragCameraHeight = cameraPosition.y;
    
    // Transform vertex to clip space
    gl_Position = projectionMatrix * viewMatrix * vec4(position, 1.0);
}
)";

const char* skydomeFragmentShaderSource = R"(
#version 430 core
in vec3 fragWorldPos;
in vec3 fragSunDirection;
in float fragCameraHeight;

uniform vec3 rayleighCoeff;
uniform vec3 mieCoeff;
uniform float rayleighScaleHeight;
uniform float mieScaleHeight;
uniform float mieAnisotropy;
uniform float sunIntensity;
uniform float planetRadius;
uniform float atmosphereRadius;

out vec4 fragColor;

// [[Rayleigh Phase Function]]
float rayleighPhaseFunction(float cosTheta) {
    return (3.0 / (16.0 * 3.14159265359)) * (1.0 + cosTheta * cosTheta);
}

// [[Mie Phase Function]]
float miePhaseFunction(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 / (4.0 * 3.14159265359)) * ((1.0 - g2) * (1.0 + cosTheta * cosTheta)) / 
           (pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5) * (2.0 + g2));
}

// [[Optical Depth Function]]
float opticalDepth(vec3 pos, vec3 dir, float scaleHeight) {
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, pos);
    float c = dot(pos, pos) - atmosphereRadius * atmosphereRadius;
    float det = b * b - 4.0 * a * c;
    
    if (det < 0.0) return 0.0;
    
    float t = (-b - sqrt(det)) / (2.0 * a);
    if (t < 0.0) t = (-b + sqrt(det)) / (2.0 * a);
    
    return exp(-(length(pos + dir * t) - planetRadius) / scaleHeight) * t;
}

void main() {
    // Camera position in atmosphere coordinates
    vec3 cameraPos = vec3(0.0, fragCameraHeight, 0.0);
    
    // View direction from camera to fragment
    vec3 viewDir = normalize(fragWorldPos - cameraPos);
    
    // Calculate optical depths
    float rayleighDepth = opticalDepth(cameraPos, viewDir, rayleighScaleHeight);
    float mieDepth = opticalDepth(cameraPos, viewDir, mieScaleHeight);
    
    // Calculate extinction
    vec3 extinction = exp(-(rayleighCoeff * rayleighDepth + mieCoeff * mieDepth));
    
    // Calculate in-scattering
    vec3 sunDir = normalize(fragSunDirection);
    float cosTheta = dot(viewDir, sunDir);
    
    float rayleighPhase = rayleighPhaseFunction(cosTheta);
    float miePhase = miePhaseFunction(cosTheta, mieAnisotropy);
    
    // Optical depth to sun
    float sunRayleighDepth = opticalDepth(fragWorldPos, sunDir, rayleighScaleHeight);
    float sunMieDepth = opticalDepth(fragWorldPos, sunDir, mieScaleHeight);
    
    vec3 sunExtinction = exp(-(rayleighCoeff * sunRayleighDepth + mieCoeff * sunMieDepth));
    
    // Calculate final colors
    vec3 rayleighColor = rayleighPhase * rayleighCoeff * sunExtinction * sunIntensity;
    vec3 mieColor = miePhase * mieCoeff * sunExtinction * sunIntensity;
    
    vec3 finalColor = (rayleighColor + mieColor) * (1.0 - extinction);
    
    // Add some ambient light
    finalColor += vec3(0.05, 0.07, 0.1) * extinction;
    
    // Gamma correction
    finalColor = pow(finalColor, vec3(1.0/2.2));
    
    fragColor = vec4(clamp(finalColor, 0.0, 1.0), 1.0);
}
)";
```

---

## Step 5: Add Shader Compilation and Setup Function

```cpp
// Add after shader source code
bool compileAtmosphereShaders() {
    /* 
    [[Shader Compilation]]
    Compiles and links the atmosphere and skydome shaders.
    Returns true on success, false on failure with error messages.
    */
    
    // Compile atmosphere vertex shader
    GLuint atmosphereVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(atmosphereVS, 1, &atmosphereVertexShaderSource, NULL);
    glCompileShader(atmosphereVS);
    
    // Check compilation
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(atmosphereVS, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(atmosphereVS, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }
    
    // Compile atmosphere fragment shader
    GLuint atmosphereFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(atmosphereFS, 1, &atmosphereFragmentShaderSource, NULL);
    glCompileShader(atmosphereFS);
    
    glGetShaderiv(atmosphereFS, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(atmosphereFS, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }
    
    // Link atmosphere shader program
    atmosphereShaderProgram = glCreateProgram();
    glAttachShader(atmosphereShaderProgram, atmosphereVS);
    glAttachShader(atmosphereShaderProgram, atmosphereFS);
    glLinkProgram(atmosphereShaderProgram);
    
    glGetProgramiv(atmosphereShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(atmosphereShaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return false;
    }
    
    // Clean up shaders
    glDeleteShader(atmosphereVS);
    glDeleteShader(atmosphereFS);
    
    // Compile skydome vertex shader
    GLuint skydomeVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(skydomeVS, 1, &skydomeVertexShaderSource, NULL);
    glCompileShader(skydomeVS);
    
    glGetShaderiv(skydomeVS, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(skydomeVS, 512, NULL, infoLog);
        std::cerr << "ERROR::SKYDOME::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }
    
    // Compile skydome fragment shader
    GLuint skydomeFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(skydomeFS, 1, &skydomeFragmentShaderSource, NULL);
    glCompileShader(skydomeFS);
    
    glGetShaderiv(skydomeFS, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(skydomeFS, 512, NULL, infoLog);
        std::cerr << "ERROR::SKYDOME::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }
    
    // Link skydome shader program
    skydomeShaderProgram = glCreateProgram();
    glAttachShader(skydomeShaderProgram, skydomeVS);
    glAttachShader(skydomeShaderProgram, skydomeFS);
    glLinkProgram(skydomeShaderProgram);
    
    glGetProgramiv(skydomeShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(skydomeShaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SKYDOME::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return false;
    }
    
    // Clean up shaders
    glDeleteShader(skydomeVS);
    glDeleteShader(skydomeFS);
    
    // Get uniform locations for atmosphere shader
    atmosphereUniforms.sunDirection = glGetUniformLocation(atmosphereShaderProgram, "sunDirection");
    atmosphereUniforms.planetRadius = glGetUniformLocation(atmosphereShaderProgram, "planetRadius");
    atmosphereUniforms.atmosphereRadius = glGetUniformLocation(atmosphereShaderProgram, "atmosphereRadius");
    atmosphereUniforms.rayleighCoeff = glGetUniformLocation(atmosphereShaderProgram, "rayleighCoeff");
    atmosphereUniforms.mieCoeff = glGetUniformLocation(atmosphereShaderProgram, "mieCoeff");
    atmosphereUniforms.rayleighScaleHeight = glGetUniformLocation(atmosphereShaderProgram, "rayleighScaleHeight");
    atmosphereUniforms.mieScaleHeight = glGetUniformLocation(atmosphereShaderProgram, "mieScaleHeight");
    atmosphereUniforms.mieAnisotropy = glGetUniformLocation(atmosphereShaderProgram, "mieAnisotropy");
    atmosphereUniforms.sunIntensity = glGetUniformLocation(atmosphereShaderProgram, "sunIntensity");
    atmosphereUniforms.cameraHeight = glGetUniformLocation(atmosphereShaderProgram, "cameraHeight");
    atmosphereUniforms.viewMatrix = glGetUniformLocation(atmosphereShaderProgram, "viewMatrix");
    atmosphereUniforms.projectionMatrix = glGetUniformLocation(atmosphereShaderProgram, "projectionMatrix");
    
    // Get uniform locations for skydome shader
    skydomeUniforms.sunDirection = glGetUniformLocation(skydomeShaderProgram, "sunDirection");
    skydomeUniforms.planetRadius = glGetUniformLocation(skydomeShaderProgram, "planetRadius");
    skydomeUniforms.atmosphereRadius = glGetUniformLocation(skydomeShaderProgram, "atmosphereRadius");
    skydomeUniforms.rayleighCoeff = glGetUniformLocation(skydomeShaderProgram, "rayleighCoeff");
    skydomeUniforms.mieCoeff = glGetUniformLocation(skydomeShaderProgram, "mieCoeff");
    skydomeUniforms.rayleighScaleHeight = glGetUniformLocation(skydomeShaderProgram, "rayleighScaleHeight");
    skydomeUniforms.mieScaleHeight = glGetUniformLocation(skydomeShaderProgram, "mieScaleHeight");
    skydomeUniforms.mieAnisotropy = glGetUniformLocation(skydomeShaderProgram, "mieAnisotropy");
    skydomeUniforms.sunIntensity = glGetUniformLocation(skydomeShaderProgram, "sunIntensity");
    skydomeUniforms.cameraPosition = glGetUniformLocation(skydomeShaderProgram, "cameraPosition");
    skydomeUniforms.viewMatrix = glGetUniformLocation(skydomeShaderProgram, "viewMatrix");
    skydomeUniforms.projectionMatrix = glGetUniformLocation(skydomeShaderProgram, "projectionMatrix");
    
    std::cout << "Atmosphere shaders compiled successfully" << std::endl;
    return true;
}
```

---

## Step 6: Add Rendering Functions

```cpp
// Add after compileAtmosphereShaders()
void renderSkydome(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, 
                  const glm::vec3& cameraPosition) {
    /* 
    [[Skydome Rendering]]
    Renders the atmospheric skydome using the precomputed mesh and shaders.
    The skydome represents the upper hemisphere of the atmosphere.
    */
    
    glUseProgram(skydomeShaderProgram);
    
    // Set uniforms
    glUniform3fv(skydomeUniforms.sunDirection, 1, glm::value_ptr(atmosphere.sunDirection));
    glUniform1f(skydomeUniforms.planetRadius, atmosphere.planetRadius);
    glUniform1f(skydomeUniforms.atmosphereRadius, atmosphere.atmosphereRadius);
    glUniform3fv(skydomeUniforms.rayleighCoeff, 1, glm::value_ptr(atmosphere.rayleighCoeff));
    glUniform3fv(skydomeUniforms.mieCoeff, 1, glm::value_ptr(atmosphere.mieCoeff));
    glUniform1f(skydomeUniforms.rayleighScaleHeight, atmosphere.rayleighScaleHeight);
    glUniform1f(skydomeUniforms.mieScaleHeight, atmosphere.mieScaleHeight);
    glUniform1f(skydomeUniforms.mieAnisotropy, atmosphere.mieAnisotropy);
    glUniform1f(skydomeUniforms.sunIntensity, atmosphere.sunIntensity);
    glUniform3fv(skydomeUniforms.cameraPosition, 1, glm::value_ptr(cameraPosition));
    glUniformMatrix4fv(skydomeUniforms.viewMatrix, 1, GL_FALSE, glm::value_ptr(viewMatrix));
    glUniformMatrix4fv(skydomeUniforms.projectionMatrix, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
    
    // Enable depth test but disable depth writing (skydome is always behind everything)
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    
    // Render skydome
    glBindVertexArray(skydomeVAO);
    glDrawElements(GL_TRIANGLES, skydomeIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Restore depth settings
    glDepthMask(GL_TRUE);
}

void renderAtmosphericEffects(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                             float cameraHeight) {
    /* 
    [[Atmospheric Effects Rendering]]
    Renders atmospheric scattering effects for objects in the scene.
    This function is typically called after rendering the main scene geometry
    but before UI elements, to apply atmospheric effects as a post-process.
    */
    
    glUseProgram(atmosphereShaderProgram);
    
    // Set uniforms
    glUniform3fv(atmosphereUniforms.sunDirection, 1, glm::value_ptr(atmosphere.sunDirection));
    glUniform1f(atmosphereUniforms.planetRadius, atmosphere.planetRadius);
    glUniform1f(atmosphereUniforms.atmosphereRadius, atmosphere.atmosphereRadius);
    glUniform3fv(atmosphereUniforms.rayleighCoeff, 1, glm::value_ptr(atmosphere.rayleighCoeff));
    glUniform3fv(atmosphereUniforms.mieCoeff, 1, glm::value_ptr(atmosphere.mieCoeff));
    glUniform1f(atmosphereUniforms.rayleighScaleHeight, atmosphere.rayleighScaleHeight);
    glUniform1f(atmosphereUniforms.mieScaleHeight, atmosphere.mieScaleHeight);
    glUniform1f(atmosphereUniforms.mieAnisotropy, atmosphere.mieAnisotropy);
    glUniform1f(atmosphereUniforms.sunIntensity, atmosphere.sunIntensity);
    glUniform1f(atmosphereUniforms.cameraHeight, cameraHeight);
    glUniformMatrix4fv(atmosphereUniforms.viewMatrix, 1, GL_FALSE, glm::value_ptr(viewMatrix));
    glUniformMatrix4fv(atmosphereUniforms.projectionMatrix, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
    
    // Set up blending for atmospheric effects
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // TODO: Render atmospheric effects (this would typically be done with a full-screen quad
    // or by rendering atmospheric volumes around objects)
    
    glDisable(GL_BLEND);
}
```

---

## Step 7: Add ImGui Control Panel for Real-time Adjustment

```cpp
// Add after rendering functions
void renderAtmosphereControlPanel() {
    /* 
    [[Real-time Parameter Control]]
    Creates an ImGui control panel for adjusting atmospheric parameters in real-time.
    This allows artists and developers to see the effects of parameter changes immediately.
    */
    
    static bool showAtmospherePanel = true;
    
    if (!showAtmospherePanel) return;
    
    ImGui::Begin("Atmosphere Controls", &showAtmospherePanel, ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::Text("Atmospheric Conditions");
    ImGui::Separator();
    
    // Sun controls
    ImGui::Text("Sun Direction");
    static float sunElevation = -30.0f; // Degrees from horizon
    static float sunAzimuth = 180.0f;   // Degrees from north
    
    if (ImGui::SliderFloat("Elevation", &sunElevation, -90.0f, 90.0f, "%.1f°")) {
        // Convert spherical coordinates to Cartesian
        float elevationRad = sunElevation * DEG_TO_RAD;
        float azimuthRad = sunAzimuth * DEG_TO_RAD;
        atmosphere.sunDirection = glm::vec3(
            cos(elevationRad) * cos(azimuthRad),
            sin(elevationRad),
            cos(elevationRad) * sin(azimuthRad)
        );
    }
    
    if (ImGui::SliderFloat("Azimuth", &sunAzimuth, 0.0f, 360.0f, "%.1f°")) {
        float elevationRad = sunElevation * DEG_TO_RAD;
        float azimuthRad = sunAzimuth * DEG_TO_RAD;
        atmosphere.sunDirection = glm::vec3(
            cos(elevationRad) * cos(azimuthRad),
            sin(elevationRad),
            cos(elevationRad) * sin(azimuthRad)
        );
    }
    
    ImGui::SliderFloat("Sun Intensity", &atmosphere.sunIntensity, 0.1f, 100.0f, "%.1f");
    
    ImGui::Separator();
    
    // Atmospheric quality controls
    ImGui::Text("Atmospheric Quality");
    
    if (ImGui::SliderFloat("Density", &atmosphere.atmosphereDensity, 0.1f, 5.0f, "%.2f")) {
        atmosphere.updateCoefficients();
    }
    
    if (ImGui::SliderFloat("Pollution", &atmosphere.pollutionLevel, 0.0f, 1.0f, "%.2f")) {
        // More pollution increases Mie scattering (haze) and reduces visibility
        atmosphere.updateCoefficients();
    }
    
    if (ImGui::SliderFloat("Humidity", &atmosphere.humidityLevel, 0.0f, 1.0f, "%.2f")) {
        // More humidity increases scattering and creates more diffuse lighting
        atmosphere.updateCoefficients();
    }
    
    ImGui::Separator();
    
    // Preset buttons
    ImGui::Text("Presets");
    if (ImGui::Button("Clear Sky")) {
        atmosphere.atmosphereDensity = 1.0f;
        atmosphere.pollutionLevel = 0.0f;
        atmosphere.humidityLevel = 0.2f;
        atmosphere.sunIntensity = 20.0f;
        atmosphere.updateCoefficients();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Hazy Day")) {
        atmosphere.atmosphereDensity = 1.2f;
        atmosphere.pollutionLevel = 0.4f;
        atmosphere.humidityLevel = 0.5f;
        atmosphere.sunIntensity = 15.0f;
        atmosphere.updateCoefficients();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Heavy Pollution")) {
        atmosphere.atmosphereDensity = 1.5f;
        atmosphere.pollutionLevel = 0.8f;
        atmosphere.humidityLevel = 0.6f;
        atmosphere.sunIntensity = 10.0f;
        atmosphere.updateCoefficients();
    }
    
    ImGui::Separator();
    
    // Technical parameters (advanced)
    if (ImGui::TreeNode("Advanced Parameters")) {
        ImGui::SliderFloat("Rayleigh Scale Height", &atmosphere.rayleighScaleHeight, 1000.0f, 20000.0f, "%.0f m");
        ImGui::SliderFloat("Mie Scale Height", &atmosphere.mieScaleHeight, 500.0f, 5000.0f, "%.0f m");
        ImGui::SliderFloat("Mie Anisotropy", &atmosphere.mieAnisotropy, 0.0f, 0.99f, "%.3f");
        
        ImGui::Text("Rayleigh Coefficients (RGB):");
        ImGui::SliderFloat3("##rayleigh", glm::value_ptr(atmosphere.rayleighCoeff), 1e-6f, 1e-4f, "%.3e");
        
        ImGui::Text("Mie Coefficients (RGB):");
        ImGui::SliderFloat3("##mie", glm::value_ptr(atmosphere.mieCoeff), 1e-6f, 1e-4f, "%.3e");
        
        ImGui::TreePop();
    }
    
    ImGui::End();
}
```

---

## Step 8: Integration into Main Rendering Loop

```cpp
// Find the main rendering loop in Baseline.cpp (look for glfwSwapBuffers or similar)
// Add the following code before the final buffer swap

// Inside the main loop, after regular scene rendering but before UI:

// Get camera position and height
glm::vec3 cameraPosition = camera.getPosition();
float cameraHeight = cameraPosition.y - atmosphere.planetRadius;

// Update atmosphere coefficients based on current parameters
atmosphere.updateCoefficients();

// Render skydome first (background)
renderSkydome(viewMatrix, projectionMatrix, cameraPosition);

// Render atmospheric effects (fog, scattering) over the scene
// This should be done after main scene rendering but before UI elements
renderAtmosphericEffects(viewMatrix, projectionMatrix, cameraHeight);

// Render ImGui control panel
renderAtmosphereControlPanel();

// Continue with existing UI rendering and buffer swap
```

---

## Step 9: Initialization in Main Function

```cpp
// Find the main() function initialization section
// Add the following code after OpenGL context creation but before main loop

// Initialize atmosphere parameters
atmosphere.updateCoefficients();

// Generate skydome mesh
generateSkydome();

// Compile atmosphere shaders
if (!compileAtmosphereShaders()) {
    std::cerr << "Failed to compile atmosphere shaders. Exiting." << std::endl;
    return -1;
}

std::cout << "Atmosphere system initialized successfully" << std::endl;
```

---

## Step 10: Cleanup Function

```cpp
// Add before the end of main() or in a cleanup function
// Clean up atmosphere resources
glDeleteProgram(atmosphereShaderProgram);
glDeleteProgram(skydomeShaderProgram);
glDeleteVertexArrays(1, &skydomeVAO);
glDeleteBuffers(1, &skydomeVBO);
glDeleteBuffers(1, &skydomeIBO);
```

---

# [[Refactoring Guide]] Future Improvements

## File Structure Plan
Once the single-file implementation is working, refactor into separate files:

```
src/
├── rendering/
│   ├── AtmosphereSystem.cpp/hpp          // Main atmosphere system
│   ├── SkydomeRenderer.cpp/hpp           // Skydome mesh and rendering
│   ├── AtmosphericScattering.cpp/hpp     // Scattering calculations
│   └── AtmosphereUI.cpp/hpp              // ImGui control panel
├── shaders/
│   ├── atmosphere.vert
│   ├── atmosphere.frag
│   ├── skydome.vert
│   └── skydome.frag
└── utils/
    └── MathUtils.cpp/hpp                 // Additional math functions
```

## Key Refactoring Steps
1. **Extract Atmosphere System**: Move all atmosphere-related code to `AtmosphereSystem.cpp/hpp`
2. **Separate Rendering Logic**: Create `SkydomeRenderer` and `AtmosphericScattering` classes
3. **Externalize Shaders**: Move shader source code to external files
4. **Create UI Component**: Extract ImGui controls to separate module
5. **Add Configuration**: Create JSON/XML configuration for preset atmospheres

## Performance Optimizations
- Implement texture lookups for precomputed scattering integrals
- Add level-of-detail for skydome based on distance
- Implement temporal anti-aliasing for atmospheric effects
- Add compute shader support for high-altitude rendering

## Advanced Features to Add
- Dynamic weather transitions
- Volumetric clouds integration
- Multiple scattering approximation
- Night sky with stars and moon
- Atmospheric shadow casting

---

# [[Implementation Notes]] Key Concepts

## [[Rayleigh vs Mie Scattering]]
- **Rayleigh scattering**: Dominant for clear skies, wavelength-dependent (blue scatters more)
- **Mie scattering**: Dominant for hazy conditions, less wavelength-dependent, forward-scattering dominant
- The ratio of these coefficients determines sky color and haze appearance

## [[Hoffman-Preetham Approach]]
The implementation follows the Hoffman-Preetham method described in the lecture:
- Precomputes expensive integrals at vertices
- Uses linear interpolation in fragment shader
- Assumes constant atmospheric density (good for ground-level views)
- Efficient for real-time rendering

## [[Real-time Parameter Adjustment]]
The ImGui panel allows real-time adjustment of:
- **Pollution level**: Increases Mie scattering, creates haze
- **Humidity**: Affects both scattering types, creates more diffuse lighting
- **Atmospheric density**: Global multiplier for all scattering effects
- **Sun position**: Changes lighting direction and sky colors

## [[Optical Depth Calculation]]
The optical depth calculation is crucial for realistic atmospheric effects:
- Uses numerical integration along view rays
- Accounts for exponential density decay with height
- Determines both extinction and in-scattering effects

This implementation provides a solid foundation that can be extended with more advanced techniques like ray marching for high-altitude views or volumetric fog as described in the lecture materials.