cbuffer mvp : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    
    float4x4 inverseModel;
};

struct VSOut
{
    float3 worldPos : Position;
    float4 pos : SV_Position;
};

VSOut main(float3 pos : Position)
{
    float4x4 finalMVP = mul(mul(model, view), proj);
    
    VSOut vso;
    vso.worldPos = pos;
    vso.pos = mul(float4(pos, 0.0f), finalMVP);
    // make sure that the depth after w divide will be 1.0 (so that the z-buffering will work)
    vso.pos.z = vso.pos.w;
    return vso;
}