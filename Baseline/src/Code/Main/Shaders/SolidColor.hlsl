cbuffer ObjectCB : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

cbuffer PassCB : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float padding0;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};

struct VSInput
{
    float3 PosL : POSITION;
};

struct PSInput
{
    float4 PosH : SV_POSITION;
};

PSInput VS(VSInput vin)
{
    PSInput vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
    return vout;
}

float4 PS(PSInput pin) : SV_Target
{
    return float4(0.2f, 0.6f, 0.95f, 1.0f);
}
