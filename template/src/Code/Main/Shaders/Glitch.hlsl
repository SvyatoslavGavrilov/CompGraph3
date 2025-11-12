// Glitch.hlsl

Texture2D gSceneTexture : register(t0);
SamplerState gsamLinearClamp : register(s0);

cbuffer cbPostProcess : register(b0)
{
    float gGlitchIntensity;      // Интенсивность глитч эффекта (0.0-1.0)
    float gGlitchBlockSize;      // Размер блоков для искажения (например 0.1-0.5)
    float gTime;                 // Время для анимированного эффекта
    float gGlitchSpeed;          // Скорость глитч эффекта
    float gColorShift;           // Смещение цветовых каналов
    float gNoiseIntensity;       // Интенсивность шума
    float gGlitchThreshold;      // Порог активации глитч эффекта
    float gPadding;              // Выравнивание для cbuffer
};

struct VSOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VSOut VS(uint vid : SV_VertexID)
{
    VSOut output;
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };
    output.PosH = float4(positions[vid], 0.0f, 1.0f);
    output.TexC = float2((output.PosH.x + 1.0f) * 0.5f, (1.0f - output.PosH.y) * 0.5f);
    return output;
}

float4 PS(VSOut pin) : SV_TARGET
{
    float2 texCoord = pin.TexC;
    float4 color = gSceneTexture.Sample(gsamLinearClamp, texCoord);
    
    // Генерируем псевдо-случайные значения для глитч эффекта
    float2 blockCoord = floor(texCoord / gGlitchBlockSize);
    float randomValue = frac(sin(dot(blockCoord, float2(12.9898, 78.233))) * 43758.5453);
    
    // Определяем, должен ли этот блок иметь глитч эффект
    float glitchTrigger = step(gGlitchThreshold, randomValue);
    
    // Создаем анимированное смещение
    float animatedTime = gTime * gGlitchSpeed;
    float offset = sin(animatedTime + randomValue * 6.28318) * gGlitchIntensity;
    
    // Применяем смещение только к активным блокам
    float2 glitchOffset = float2(offset * glitchTrigger, 0.0f);
    float2 distortedCoords = texCoord + glitchOffset;
    
    // Сэмплируем с искаженными координатами
    float4 glitchColor = gSceneTexture.Sample(gsamLinearClamp, distortedCoords);
    
    // Смешиваем оригинальный цвет с глитч версией
    color = lerp(color, glitchColor, glitchTrigger * gGlitchIntensity);
    
    // Добавляем цветовое смещение (RGB shift)
    if (glitchTrigger > 0.5f)
    {
        float2 redOffset = float2(gColorShift * 0.01f, 0.0f);
        float2 blueOffset = float2(-gColorShift * 0.01f, 0.0f);
        
        float redChannel = gSceneTexture.Sample(gsamLinearClamp, texCoord + redOffset).r;
        float blueChannel = gSceneTexture.Sample(gsamLinearClamp, texCoord + blueOffset).b;
        
        color.r = redChannel;
        color.b = blueChannel;
    }
    
    // Добавляем цифровой шум
    float noise = frac(sin(dot(texCoord * animatedTime, float2(12.9898, 78.233))) * 43758.5453);
    noise = (noise - 0.5f) * 2.0f; // Нормализуем в диапазон [-1, 1]
    color.rgb += noise * gNoiseIntensity * glitchTrigger;
    
    return saturate(color);
}

