//***************************************************************************************
// Simple shader for rotating rainbow pyramid
//***************************************************************************************

// Constant data that varies per object (world matrix for rotation)
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
};

// Constant data that varies per frame
cbuffer cbPass : register(b1)
{
    float gTotalTime;
};

struct VertexIn
{
    float3 PosL    : POSITION;
    float4 Color    : COLOR;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float4 Color    : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // Transform to world space
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    
    // Transform to homogeneous clip space
    vout.PosH = mul(posW, gViewProj);
    
    // Pass through color
    vout.Color = vin.Color;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}
