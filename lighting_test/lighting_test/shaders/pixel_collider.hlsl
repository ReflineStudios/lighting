struct VertexOutput
{
    float4 pos : SV_Position;
    float4 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 texCoords : TEX_COORDS;
};

float4 main(VertexOutput v) : SV_Target0
{
    return float4(0.4f, 0.9f, 0.2f, 1.0f);
}