//***************************************************************************************
// Simple shader for rotating rainbow cube
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
    float4 Color   : COLOR;
    float3 NormalL : NORMAL;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float4 Color   : COLOR;
    float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // Transform to world space
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    
    // Transform to homogeneous clip space
    vout.PosH = mul(posW, gViewProj);
    
    // Transform normal to world space
    // Extract the 3x3 rotation part from the 4x4 world matrix
    // For rotation matrices, this is sufficient (inverse transpose equals the matrix itself)
    float3x3 world3x3 = float3x3(gWorld[0].xyz, gWorld[1].xyz, gWorld[2].xyz);
    float3 normalW = mul(vin.NormalL, world3x3);
    vout.NormalW = normalize(normalW);
    
    // Pass through color
    vout.Color = vin.Color;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Simple lighting based on normal direction for better visualization
    // Light direction (pointing from top-right)
    float3 lightDir = normalize(float3(0.5f, 1.0f, -0.5f));
    
    // Calculate diffuse lighting (Lambertian)
    float NdotL = max(dot(pin.NormalW, lightDir), 0.3f); // 0.3 ambient + diffuse
    
    // Apply lighting to color
    float3 litColor = pin.Color.rgb * NdotL;
    
    return float4(litColor, pin.Color.a);
}
