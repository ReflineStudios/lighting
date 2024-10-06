TextureCube tex : register(t0);
SamplerState sam : register(s0);

struct VSOut
{
    float3 worldPos : Position;
    float4 pos : SV_Position;
};

float4 main(VSOut worldPos) : SV_TARGET
{
    return tex.Sample(sam, worldPos.worldPos);
}