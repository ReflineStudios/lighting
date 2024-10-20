

Texture2D depthMap : register(t0);
SamplerState sampler0;

float4 main(float4 pos : SV_Position) : SV_Target0
{
    float depthValue = depthMap.Sample(sampler0, pos.xy).r;
    

    //rendo pos.z più visible
    float proportionz = (pos.z * 1.0f) / 0.1f; //pos.z : x = 0.5 : 100
    return float4(float3(proportionz, proportionz, proportionz), 1.0f);


    //return float4(depthValue, depthValue, depthValue, 1.0f);
}