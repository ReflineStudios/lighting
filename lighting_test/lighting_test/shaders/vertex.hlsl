cbuffer mvp : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    
    float4x4 inverseModel;
};

struct VertexOutput
{
    float4 pos : SV_Position;
    float4 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 texCoords : TEX_COORDS;
};

VertexOutput main(float3 pos : POS, float3 normal : NORMAL, float2 texCoords : TEX_COORDS)
{
    VertexOutput o;
    
    float4x4 finalMVP = mul(mul(model, view), proj);
    
    o.pos = mul(float4(pos.xyz, 1.0f), finalMVP);
    
    o.worldPos = mul(float4(pos.xyz, 1.0f), model);
    
    o.texCoords = texCoords;
    
    o.normal = normalize(mul(normal, float3x3(inverseModel[0].xyz, inverseModel[1].xyz, inverseModel[2].xyz))); //'normal matrix' used to transform normals (since normals are not affected by translation), otherwise lighting would be distorted with non-uniform scaling of the model in the world space
    
    return o;
}