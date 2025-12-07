//***************************************************************************************
// Terrain shader with tessellation and heightmap sampling
//***************************************************************************************

// Constant data that varies per object (world matrix)
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
};

// Constant data that varies per frame
cbuffer cbPass : register(b1)
{
    float gTotalTime;
    float heightScale;
    float terrainSize;
    uint heightmapWidth;
    uint heightmapHeight;
    float tileSize;
    float padding0;               // Padding to align to 16-byte boundary
    float3 cameraPosition;
    float minTessellationFactor;  // Minimum tessellation factor
    float maxTessellationFactor;  // Maximum tessellation factor
    float tessellationDistance;   // Distance for tessellation calculation
    uint showLODEdges;            // Flag to show LOD tile edges
    float3 padding1;              // Padding for alignment
};

// Textures
Texture2D heightmapTexture : register(t0);
Texture2D terrainTexture : register(t1);
SamplerState gSampler : register(s0);

struct VertexIn
{
    float3 PosL : POSITION;
};

struct HullOut
{
    float3 PosL : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct DomainOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : WORLDPOS;
    float2 TexCoord : TEXCOORD;
};

// Patch constant output for tessellation
struct PatchTess
{
    float EdgeTess[3] : SV_TessFactor;
    float InsideTess : SV_InsideTessFactor;
};

// Simple vertex shader - just passes through the position
HullOut VS(VertexIn vin)
{
    HullOut vout;
    vout.PosL = vin.PosL;
    
    // Calculate texture coordinates from world position
    float2 uv = float2(vin.PosL.x, vin.PosL.z) / terrainSize;
    uv = uv * 0.5f + 0.5f;  // Convert from [-1,1] to [0,1]
    vout.TexCoord = uv;
    
    return vout;
}

// Constant hull shader - computes tessellation factors based on distance to camera
PatchTess ConstantHS(InputPatch<HullOut, 3> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
    
    // Calculate center of the patch in world space
    float3 patchCenter = (patch[0].PosL + patch[1].PosL + patch[2].PosL) / 3.0f;
    float4 patchCenterW = mul(float4(patchCenter, 1.0f), gWorld);
    
    // Calculate distance from camera to patch center
    float distToCamera = distance(patchCenterW.xyz, cameraPosition);
    
    // Calculate tessellation factor based on distance
    // Closer patches get more tessellation (higher factor)
    float tessFactor = maxTessellationFactor;
    if (distToCamera > 0.0f)
    {
        // Scale tessellation based on distance
        float distFactor = saturate(1.0f - (distToCamera / tessellationDistance));
        tessFactor = lerp(minTessellationFactor, maxTessellationFactor, distFactor);
    }
    
    // Set edge tessellation factors
    pt.EdgeTess[0] = tessFactor;
    pt.EdgeTess[1] = tessFactor;
    pt.EdgeTess[2] = tessFactor;
    
    // Set inside tessellation factor
    pt.InsideTess = tessFactor;
    
    return pt;
}

// Hull shader - passes through control points
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
HullOut HS(InputPatch<HullOut, 3> p, uint i : SV_OutputControlPointID, uint patchId : SV_PrimitiveID)
{
    HullOut hout;
    hout.PosL = p[i].PosL;
    hout.TexCoord = p[i].TexCoord;
    return hout;
}

// Domain shader - samples heightmap and applies height displacement
[domain("tri")]
DomainOut DS(PatchTess patchTess, float3 baryCoords : SV_DomainLocation, const OutputPatch<HullOut, 3> tri)
{
    DomainOut dout;
    
    // Interpolate position using barycentric coordinates
    float3 posL = baryCoords.x * tri[0].PosL + 
                  baryCoords.y * tri[1].PosL + 
                  baryCoords.z * tri[2].PosL;
    
    // Interpolate texture coordinates
    float2 texCoord = baryCoords.x * tri[0].TexCoord + 
                      baryCoords.y * tri[1].TexCoord + 
                      baryCoords.z * tri[2].TexCoord;
    
    // Clamp texture coordinates to valid range
    texCoord = saturate(texCoord);
    
    // Sample height from heightmap texture
    // Use SampleLevel for deterministic sampling in domain shader
    float heightValue = heightmapTexture.SampleLevel(gSampler, texCoord, 0).r;
    
    // Apply height with increased scale for more visible height changes
    // Multiply by a larger factor to make height variations more pronounced
    float height = heightValue * heightScale * 3.0f;  // 3x multiplier for more visible height
    
    // Apply height offset to position
    float3 worldPos = float3(posL.x, height, posL.z);
    
    // Transform to world space
    float4 posW = mul(float4(worldPos, 1.0f), gWorld);
    
    // Transform to homogeneous clip space
    dout.PosH = mul(posW, gViewProj);
    
    // Pass through world position and texture coordinates
    dout.PosW = posW.xyz;
    dout.TexCoord = texCoord;
    
    return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
    // LOD edge detection in world space
    bool isLODEdge = false;
    
    if (showLODEdges != 0)
    {
        // Calculate grid position in world space
        // Use tileSize to determine grid cells
        float gridSize = tileSize;
        float2 worldPosXZ = float2(pin.PosW.x, pin.PosW.z);
        
        // Calculate grid coordinates
        float2 gridCoord = floor(worldPosXZ / gridSize);
        float2 gridPos = gridCoord * gridSize;
        float2 localPos = worldPosXZ - gridPos;
        
        // Check if we're near a grid edge (within a small threshold)
        const float edgeThreshold = 0.1f;  // Distance threshold for edge detection
        
        // Check X and Z edges
        bool nearXEdge = (localPos.x < edgeThreshold) || (localPos.x > (gridSize - edgeThreshold));
        bool nearZEdge = (localPos.z < edgeThreshold) || (localPos.z > (gridSize - edgeThreshold));
        
        // Also check for texture coordinate discontinuities (tile boundaries)
        float2 texCoordDdx = ddx(pin.TexCoord);
        float2 texCoordDdy = ddy(pin.TexCoord);
        float edgeFactor = length(texCoordDdx) + length(texCoordDdy);
        const float derivativeThreshold = 0.1f;
        bool isTextureEdge = edgeFactor > derivativeThreshold;
        
        // Check proximity to texture coordinate boundaries
        float2 edgeDist = min(pin.TexCoord, 1.0f - pin.TexCoord);
        float minEdgeDist = min(edgeDist.x, edgeDist.y);
        const float texEdgeThreshold = 0.02f;
        bool isTexBoundary = minEdgeDist < texEdgeThreshold;
        
        // Combine all edge detection methods
        isLODEdge = nearXEdge || nearZEdge || isTextureEdge || isTexBoundary;
    }
    
    // If on LOD edge and flag is enabled, render bright red
    if (isLODEdge && showLODEdges != 0)
    {
        return float4(1.0f, 0.0f, 0.0f, 1.0f);  // Bright bold red
    }
    
    // Sample terrain texture
    float4 texColor = terrainTexture.Sample(gSampler, pin.TexCoord);
    
    // Simple lighting based on height (darker at lower elevations)
    // Use the actual heightScale without the 3x multiplier for lighting calculation
    float heightFactor = (pin.PosW.y / (heightScale * 3.0f)) * 0.5f + 0.5f;
    texColor.rgb *= heightFactor;
    
    return texColor;
}
