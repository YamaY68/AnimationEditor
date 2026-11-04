cbuffer TransformBuffer : register(b0)
{
    row_major float4x4 gWorld;
    row_major float4x4 gView;
    row_major float4x4 gProj;
};

struct VS_INPUT
{
    float3 pos    : POSITION;
    float4 col    : COLOR;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD;
};

struct PS_INPUT
{
    float4 pos    : SV_POSITION;
    float4 col    : COLOR;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    float4 worldPos = mul(float4(input.pos, 1.0f), gWorld);
    float4 viewPos  = mul(worldPos, gView);
    output.pos      = mul(viewPos, gProj);
    output.col      = input.col;
    output.normal   = mul(input.normal, (float3x3)gWorld);
    output.uv       = input.uv;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    // ランバート拡散反射 + アンビエント
    float3 lightDir = normalize(float3(0.5f, 1.0f, -0.5f));
    float3 N = normalize(input.normal);
    float diffuse = max(dot(N, lightDir), 0.0f);
    
    float3 baseColor = float3(0.8f, 0.8f, 0.85f);
    float3 finalColor = baseColor * (diffuse * 0.7f + 0.3f);
    
    return float4(finalColor, 1.0f);
}