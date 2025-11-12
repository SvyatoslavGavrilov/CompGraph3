// ChromaticAberration.hlsl

Texture2D gSceneTexture : register(t0); // The input scene texture
SamplerState gsamLinearClamp : register(s0); // Sampler

cbuffer cbPostProcess : register(b0)
{
    float gChromaticAberrationOffset;
    // Add other parameters if needed, e.g., float2 gScreenDimensions;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    //Light gLights[MaxLights];
};

struct VSOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

// Vertex shader for a full-screen triangle (re-use or adapt from LightingPass.hlsl)
VSOut VS(uint vid : SV_VertexID)
{
    VSOut output;
    // Full-screen triangle vertices
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };
    output.PosH = float4(positions[vid], 0.0f, 1.0f);
    // Generate texture coordinates to cover the full screen
    output.TexC = float2((output.PosH.x + 1.0f) * 0.5f, (1.0f - output.PosH.y) * 0.5f);
    return output;
}

float4 PS(VSOut pin) : SV_TARGET
{
    float2 texCoord = pin.TexC;
    
    // ������������ �������� �� ������ ������������ �������
    float verticalPosition = texCoord.y;
    
    // ������� ������������� �������� � �������������� ������
    float distortion = sin(verticalPosition * 1 + gTotalTime);
    
    // ��������� �������� � �������������� ����������
    float2 distortedCoords = float2(
        texCoord.x + distortion,
        texCoord.y
    );
    
    // ���������� �������� �� ���������� ������������
    float4 color = gSceneTexture.Sample(gsamLinearClamp, distortedCoords);
    
    float TheOne = 1 - gChromaticAberrationOffset;
    
    return float4(TheOne - color.r, TheOne - color.g, TheOne - color.b, 1);
}