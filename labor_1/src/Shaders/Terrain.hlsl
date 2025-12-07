//***************************************************************************************
// Terrain shader with heightmap sampling and texturing
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
    float3 cameraPosition;
    float padding;
};

// Textures
Texture2D heightmapTexture : register(t0);
Texture2D terrainTexture : register(t1);
SamplerState gSampler : register(s0);

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : WORLDPOS;
    float2 TexCoord : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // Calculate texture coordinates from world position
    float2 uv = float2(vin.PosL.x, vin.PosL.z) / terrainSize;
    uv = uv * 0.5f + 0.5f;  // Convert from [-1,1] to [0,1]
    
    // Sample height from heightmap texture
    float heightValue = heightmapTexture.SampleLevel(gSampler, uv, 0).r;
    float height = heightValue * heightScale;
    
    // Apply height offset
    float3 worldPos = float3(vin.PosL.x, height, vin.PosL.z);
    
    // Transform to world space
    float4 posW = mul(float4(worldPos, 1.0f), gWorld);
    
    // Transform to homogeneous clip space
    vout.PosH = mul(posW, gViewProj);
    
    // Pass through world position and texture coordinates
    vout.PosW = posW.xyz;
    vout.TexCoord = uv;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Sample terrain texture
    float4 texColor = terrainTexture.Sample(gSampler, pin.TexCoord);
    
    // Simple lighting based on height (darker at lower elevations)
    float heightFactor = (pin.PosW.y / heightScale) * 0.5f + 0.5f;
    texColor.rgb *= heightFactor;
    
    return texColor;
}
