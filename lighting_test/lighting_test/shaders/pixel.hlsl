Texture2D albedo;
SamplerState sampler0;

struct VertexOutput
{
    float4 pos : SV_Position;
    float4 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float2 texCoords : TEX_COORDS;
};

cbuffer LightSettings : register(b0)
{
    float4 camPos0;
    float4 lightPos0;
    float4 ambientColor0;
};

float4 main(VertexOutput v) : SV_Target0
{   
    float3 camPos = float3(camPos0.xyz);
    float3 lightPos = float3(lightPos0.xyz);
    float3 ambientColor = float3(ambientColor0.xyz);
    
    float3 textureSample = albedo.Sample(sampler0, v.texCoords);
    float3 ambientLight = ambientColor * 0.1f;
    
    float3 lightDirection = normalize(v.worldPos - float4(lightPos, 1.0f));
    
    float3 diffuse = max(dot(lightDirection, v.normal) * -1.0f, 0.0f) * ambientColor;
    
    float3 viewDir = normalize(v.worldPos - float4(camPos, 1.0f));
    float3 reflectDir = reflect(lightDirection, v.normal);
    
    float spec = pow(max(dot(viewDir, reflectDir) * -1.0f, 0.0f), 512);
    float3 specular = 0.7f * spec * ambientColor;
    
    return float4(textureSample * (diffuse + ambientLight + specular), 1.0f);
}