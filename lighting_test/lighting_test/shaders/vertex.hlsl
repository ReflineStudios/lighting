cbuffer mvp : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 lightSpaceMatrix;
    float4x4 inverseModel;
};



struct VertexOutput
{
    float4 pos : SV_Position; // pixel location in screen space (normalized device coordinates)
    float4 worldPos : WORLD_POS; // pixel location in world space
    float3 normal : NORMAL;
    float2 texCoords : TEX_COORDS;
    float4 posLightSpace : POS_LIGHT_SPACE; // pixel location in light's clip space
};

VertexOutput main(float3 pos : POS, float3 normal : NORMAL, float2 texCoords : TEX_COORDS)
{
    VertexOutput o;
    
    float4x4 finalMVP = mul(mul(model, view), proj); //TODO: da spostare su CPU ed eseguire una singola volta
    
    o.pos = mul(float4(pos.xyz, 1.0f), finalMVP);
    
    o.worldPos = mul(float4(pos.xyz, 1.0f), model);
    
    o.texCoords = texCoords;
    
    o.normal = normalize(mul(normal, float3x3(inverseModel[0].xyz, inverseModel[1].xyz, inverseModel[2].xyz))); //float3x3 is the 'normal matrix' used to transform normals (since normals are not affected by translation), otherwise lighting would be distorted with non-uniform scaling of the model in the world space

    o.posLightSpace = mul(o.worldPos, lightSpaceMatrix); //transform world position to light's clip space to calculate shadow map

    return o;
}