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
    float4 lightColor0;
    float ambientStrength0;
    float specularStrength0;
    float specularPow0;
    float constant0;
    float linear0;
    float quadratic0;
    float2 dummyPadding0;
};

float4 main(VertexOutput v) : SV_Target0
{   
    float3 camPos = float3(camPos0.xyz);
    float3 lightPos = float3(lightPos0.xyz);
    float3 ambientColor = float3(ambientColor0.xyz);
    
    float3 textureSample = albedo.Sample(sampler0, v.texCoords);
    float3 ambientLight = ambientColor * ambientStrength0;
    
    float3 lightDirection = normalize(float4(lightPos, 1.0f) - v.worldPos);
    
    float3 diffuse = max(dot(lightDirection, v.normal), 0.0f) * lightColor0;
    
    float3 viewDir = normalize(float4(camPos, 1.0f) - v.worldPos);
    float3 reflectDir = reflect(-lightDirection, v.normal);
    
    float spec = pow(max(dot(viewDir, reflectDir), 0.0f), specularPow0);
    float3 specular = specularStrength0 * spec * lightColor0.xyz;


    float distance = length(lightPos - v.worldPos);
    float attenuation = 1.0 / (constant0 + linear0 * distance + quadratic0 * pow(distance, 2));
    ambientLight *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    
    return float4(textureSample * (diffuse + ambientLight + specular), 1.0f);
}