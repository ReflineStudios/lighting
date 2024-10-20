

cbuffer mvp : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 lightSpaceMatrix;
    float4x4 inverseModel;
};

float4 main( float3 pos : POSITION ) : SV_POSITION
{

    float4 worldPos = mul(float4(pos.xyz, 1.0f), model);

    return mul(worldPos, lightSpaceMatrix);

}