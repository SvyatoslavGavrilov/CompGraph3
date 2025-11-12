// Distortion.hlsl

Texture2D gSceneTexture : register(t0);
SamplerState gsamLinearClamp : register(s0);

cbuffer cbPostProcess : register(b0)
{
    float gDistortionStrength;   // Сила искажения (например 0.01-0.1)
    float gDistortionFrequency;  // Частота волн (например 5.0-20.0)
    float gTime;                 // Время для анимированного эффекта
    float gNegativeMode;         // Режим негатива (0.0 = выключен, 1.0 = включен)
    float gScanlineIntensity;    // Интенсивность сканлайнов (0.0-1.0)
    float gScanlineFrequency;    // Частота сканлайнов (например 100.0-500.0)
    float gScanlineOffset;       // Смещение сканлайнов для анимации
    // Glitch эффект параметры
    float gGlitchIntensity;      // Интенсивность глитча (0.0-1.0)
    float gGlitchSpeed;          // Скорость глитча
    float gGlitchBlockSize;      // Размер блоков глитча
    float gGlitchOffset;         // Смещение глитча
    float gGlitchNoise;          // Шум глитча
    float gGlitchRGBShift;       // RGB сдвиг для глитча
    float gGlitchChromaticAberration; // Хроматическая аберрация
    float gPadding;              // Выравнивание для cbuffer
};

struct VSOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

// Функция для генерации псевдослучайного числа
float random(float2 st)
{
    return frac(sin(dot(st.xy, float2(12.9898, 78.233))) * 43758.5453123);
}

// Функция для генерации шума
float noise(float2 st)
{
    float2 i = floor(st);
    float2 f = frac(st);
    
    float a = random(i);
    float b = random(i + float2(1.0, 0.0));
    float c = random(i + float2(0.0, 1.0));
    float d = random(i + float2(1.0, 1.0));
    
    float2 u = f * f * (3.0 - 2.0 * f);
    
    return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

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
    
    // Рассчитываем смещение на основе вертикальной позиции
    float verticalPosition = texCoord.y;
    
    // Создаем волнообразное смещение с использованием синуса
    float distortion = sin(verticalPosition * gDistortionFrequency + gTime) * gDistortionStrength;
    
    // Применяем смещение к горизонтальной координате
    float2 distortedCoords = float2(
        texCoord.x + distortion,
        texCoord.y
    );
    
    // === GLITCH ЭФФЕКТ ===
    float2 glitchCoords = distortedCoords;
    
    if (gGlitchIntensity > 0.0)
    {
        // Генерируем случайные значения для глитча
        float glitchTime = gTime * gGlitchSpeed;
        float glitchNoise = noise(float2(glitchTime, texCoord.y * 10.0));
        
        // Блочный глитч эффект
        float2 blockCoord = floor(texCoord / gGlitchBlockSize) * gGlitchBlockSize;
        float blockNoise = noise(blockCoord + glitchTime);
        
        if (blockNoise > (1.0 - gGlitchIntensity))
        {
            // Смещение блоков
            float2 blockOffset = float2(
                (random(blockCoord + glitchTime) - 0.5) * gGlitchOffset,
                (random(blockCoord + glitchTime + 1.0) - 0.5) * gGlitchOffset * 0.5
            );
            glitchCoords += blockOffset;
            
            // RGB сдвиг для глитча
            if (gGlitchRGBShift > 0.0)
            {
                float rgbShift = (random(blockCoord + glitchTime + 2.0) - 0.5) * gGlitchRGBShift;
                glitchCoords.x += rgbShift;
            }
        }
        
        // Добавляем шум
        if (gGlitchNoise > 0.0)
        {
            float noiseValue = noise(texCoord * 100.0 + glitchTime) * gGlitchNoise;
            glitchCoords += float2(noiseValue - 0.5, (noise(texCoord * 50.0 + glitchTime) - 0.5) * gGlitchNoise * 0.5);
        }
    }
    
    // Сэмплируем текстуру со смещенными координатами
    float4 color = gSceneTexture.Sample(gsamLinearClamp, glitchCoords);
    
    // === ХРОМАТИЧЕСКАЯ АБЕРРАЦИЯ ===
    if (gGlitchChromaticAberration > 0.0 && gGlitchIntensity > 0.0)
    {
        float2 redOffset = float2(gGlitchChromaticAberration, 0.0);
        float2 greenOffset = float2(0.0, 0.0);
        float2 blueOffset = float2(-gGlitchChromaticAberration, 0.0);
        
        float4 redColor = gSceneTexture.Sample(gsamLinearClamp, glitchCoords + redOffset);
        float4 greenColor = gSceneTexture.Sample(gsamLinearClamp, glitchCoords + greenOffset);
        float4 blueColor = gSceneTexture.Sample(gsamLinearClamp, glitchCoords + blueOffset);
        
        color = float4(redColor.r, greenColor.g, blueColor.b, color.a);
    }
    
    // Применяем сканлайны
    float scanline = sin(texCoord.y * gScanlineFrequency + gScanlineOffset) * 0.5f + 0.5f;
    scanline = lerp(1.0f, scanline, gScanlineIntensity);
    color.rgb *= scanline;
    
    // Применяем режим негатива (оптимизированная версия)
    // Используем saturate для обеспечения корректного диапазона [0,1]
    color.rgb = lerp(color.rgb, saturate(1.0f - color.rgb), saturate(gNegativeMode));
    
    return color;
}

