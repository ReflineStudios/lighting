#include "core.h"

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "../vendor/tinyobjloader/tiny_obj_loader.h"
#include "../vendor/stb/stb_image.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/backends/imgui_impl_win32.h"
#include "../vendor/imgui/backends/imgui_impl_dx11.h"

#include <DirectXMath.h>


#define BT_THREADSAFE 1
//#define BT_USE_DOUBLE_PRECISION

#include <bullet/btBulletDynamicsCommon.h>

bool gAppShouldRun = true;

uint32_t gWindowWidth = 1920; // 1600;
uint32_t gWindowHeight = 1080;// 900;
HWND gWindow = nullptr;

IDXGISwapChain* gSwapChain = nullptr;
ID3D11Device* gDevice = nullptr;
ID3D11DeviceContext* gContext = nullptr;

ID3D11RenderTargetView* gBackBufferView = nullptr;

ID3D11DepthStencilView* gDepthBufferView = nullptr;

ID3D11DepthStencilState* gStencilStateDefault = nullptr;
ID3D11DepthStencilState* gStencilStateSkybox = nullptr;

ID3D11Buffer* gMVPBuffer = nullptr;
ID3D11Buffer* gLightBuffer = nullptr;

ID3D11RasterizerState* gRasterStateDefault = nullptr;
ID3D11RasterizerState* gRasterStateWireframe = nullptr;
ID3D11RasterizerState* gRasterStateSkybox = nullptr;

ID3D11PixelShader* gPixelShaderDefault = nullptr;
ID3D11PixelShader* gPixelShaderCollider = nullptr;
ID3D11PixelShader* gPixelShaderSkybox = nullptr;

ID3D11InputLayout* inputLayout;
ID3D11InputLayout* inputLayoutSkybox;

ID3D11VertexShader* vertexShaderDefault;
ID3D11VertexShader* vertexShaderSkybox;

uint32_t shaderCompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;


constexpr float TO_RADIANS = DirectX::XM_PI / 180.0f;
constexpr float TO_DEGREES = 180.0f / DirectX::XM_PI;

uint8_t gKeyboard[256];

///collision configuration contains default setup for memory, collision setup. Advanced users can create their own configuration.
btDefaultCollisionConfiguration* collisionConfiguration = nullptr;

///use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
btCollisionDispatcher* dispatcher = nullptr;

///btDbvtBroadphase is a good general purpose broadphase. You can also try out btAxis3Sweep.
btBroadphaseInterface* overlappingPairCache = nullptr;

///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
btSequentialImpulseConstraintSolver* solver = nullptr;

btDiscreteDynamicsWorld* dynamicsWorld = nullptr;

btCollisionShape* groundPhysShape = nullptr;
btCollisionShape* cubePhysShape = nullptr;

btRigidBody* groundPhysRigidBody = nullptr;
btRigidBody* cubePhysRigidBody = nullptr;

bool simulatePhysics = false;

constexpr uint32_t COLLIDER_MESH_INDEX = 4;

struct MeshVertex
{
    Vec3 pos; // xyz
    Vec3 normal;
    Vec2 textureCoords; // uv
};

struct SubMeshData
{
    ID3D11ShaderResourceView* texture;
    uint32_t indexCount;
};

struct MVPBuffer
{
    DirectX::XMMATRIX model;
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX proj;

    DirectX::XMMATRIX inverseModel;
};

static Transform sModelTransform
{
    { -0.330f, -0.540f, 2.070f },   // location
    { 150.0f * TO_RADIANS, 0.0f * TO_RADIANS, 0.0f * TO_RADIANS },   // rotation
    { 0.0f , 150.0f , 0.0f },   // rotation
    { 1.0f, 1.0f, 1.0f }    // scale
};

struct Camera
{
    Vec3 location = { -1.38f, -2.44f, -29.2f };
    Vec3 rotation = { 0.0f, 0.0f, 1.0f }; //imbardata, beccheggio, rollio, come negli aerei
    float FOV = 60.0f;
    float speed = 0.03f;
};

struct LightSettings
{
    Vec4 camPos;
    Vec4 pos = { 0.9f, 0.0f, 0.6f };
    Vec4 ambientColor = { 0.1f, 0.1f, 0.1f, 1.0f };
    Vec4 lightColor = { 1.0f, 1.0f, 1.0f };
    float ambientStrength = 0.1f;
    float specularStrength = 0.7f;
    float specularPow = 256;

    // attenuation
    float constant = 1.0f;
    float linear = 0.35f;
    float quadratic = 0.44f;
    Vec2 dummyPadding0;
};

struct SceneSettings
{
    Vec4 color = { 0.1f, 0.1f, 0.1f, 1.0f };
};

static Camera sCamera;
static LightSettings sLightSettings;
static SceneSettings sSceneSettings;

struct Mesh
{
    std::vector<SubMeshData> submeshes;
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    std::string debugName;
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;
};

static std::vector<Mesh> sMeshes;
static uint32_t sMeshIndex = 0;

void LoadMesh(const MeshVertex* vertData, const int vertCount, const uint32_t* indexData, const int indexCount, Mesh& outMesh)
{
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;

    D3D11_BUFFER_DESC vBufferDesc = {};
    vBufferDesc.ByteWidth = sizeof(MeshVertex) * vertCount;
    vBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vBufferDesc.StructureByteStride = sizeof(MeshVertex);

    D3D11_SUBRESOURCE_DATA vBufferDataPtr = {};
    vBufferDataPtr.pSysMem = (const void*)vertData;

    d3dcheck(gDevice->CreateBuffer(&vBufferDesc, &vBufferDataPtr, &vertexBuffer));

    D3D11_BUFFER_DESC iBufferDesc = {};
    iBufferDesc.ByteWidth = sizeof(uint32_t) * indexCount;
    iBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    iBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA iBufferDataPtr = {};
    iBufferDataPtr.pSysMem = (const void*)indexData;

    d3dcheck(gDevice->CreateBuffer(&iBufferDesc, &iBufferDataPtr, &indexBuffer));

    outMesh.vertexBuffer = vertexBuffer;
    outMesh.indexBuffer = indexBuffer;


    auto& teest = outMesh.submeshes.emplace_back();
    teest.indexCount = indexCount;
    teest.texture = nullptr;

    outMesh.debugName = "swag";
}


void LoadMesh(const std::string& meshPath, Mesh& outMesh)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    check(tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, meshPath.c_str()));

    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMeshData> submeshes;

# if 0

    uint32_t vertexCount = attrib.vertices.size() / 3;

    vertices.resize(vertexCount);
    for (uint32_t i = 0; i < vertexCount; i++)
    {
        vertices[i].pos.x = attrib.vertices[i * 3 + 0];
        vertices[i].pos.y = attrib.vertices[i * 3 + 1];
        vertices[i].pos.z = attrib.vertices[i * 3 + 2];
    }

    for (uint32_t i = 0; i < shapes.size(); i++)
    {
        uint32_t indexCount = shapes[i].mesh.indices.size();
        for (uint32_t j = 0; j < indexCount; j++)
        {
            auto index = shapes[i].mesh.indices[j];
            MeshVertex& vertex = vertices[index.vertex_index];

            // texture coordinates
            if (index.texcoord_index != -1)
            {
                vertex.textureCoords.x = attrib.texcoords[index.texcoord_index * 2 + 0];
                vertex.textureCoords.y = -attrib.texcoords[index.texcoord_index * 2 + 1];
            }

            indices.push_back(index.vertex_index);
        }

        SubMeshData& s = submeshes.emplace_back();
        s.indexCount = indexCount;
    }

#else

    for (const auto& shape : shapes)
    {
        for (const auto& boh : shape.mesh.indices)
        {
            MeshVertex& v = vertices.emplace_back();
            v.pos.x = attrib.vertices[3 * boh.vertex_index + 0];
            v.pos.y = attrib.vertices[3 * boh.vertex_index + 1];
            v.pos.z = attrib.vertices[3 * boh.vertex_index + 2];

            if (boh.normal_index != -1)
            {
                v.normal.x = attrib.normals[3 * boh.normal_index + 0];
                v.normal.y = attrib.normals[3 * boh.normal_index + 1];
                v.normal.z = attrib.normals[3 * boh.normal_index + 2];
            }

            if (boh.texcoord_index != -1)
            {
                v.textureCoords.x = attrib.texcoords[2 * boh.texcoord_index + 0];
                v.textureCoords.y = -attrib.texcoords[2 * boh.texcoord_index + 1];
            }

            indices.push_back(indices.size());
        }

        SubMeshData& s = submeshes.emplace_back();
        s.indexCount = shape.mesh.indices.size();
        s.texture = nullptr;
    }

#endif

    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;

    D3D11_BUFFER_DESC vBufferDesc = {};
    vBufferDesc.ByteWidth = sizeof(MeshVertex) * vertices.size();
    vBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vBufferDesc.StructureByteStride = sizeof(MeshVertex);

    D3D11_SUBRESOURCE_DATA vBufferDataPtr = {};
    vBufferDataPtr.pSysMem = (const void*)vertices.data();

    d3dcheck(gDevice->CreateBuffer(&vBufferDesc, &vBufferDataPtr, &vertexBuffer));

    D3D11_BUFFER_DESC iBufferDesc = {};
    iBufferDesc.ByteWidth = sizeof(uint32_t) * indices.size();
    iBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    iBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA iBufferDataPtr = {};
    iBufferDataPtr.pSysMem = (const void*)indices.data();

    d3dcheck(gDevice->CreateBuffer(&iBufferDesc, &iBufferDataPtr, &indexBuffer));

    outMesh.vertexBuffer = vertexBuffer;
    outMesh.indexBuffer = indexBuffer;
    outMesh.submeshes = std::move(submeshes);
    outMesh.debugName = meshPath;

    outMesh.vertices = vertices;
    outMesh.indices = indices;
}

void CreateTexture(const uint32_t width, const uint32_t height, const void* data, ID3D11Texture2D*& outTexture, ID3D11ShaderResourceView*& outResource) //, uint32_t arrSize = 1
{
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.ArraySize = 1;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.SampleDesc.Count = 1;

    D3D11_SUBRESOURCE_DATA texData = {};
    texData.pSysMem = data;
    texData.SysMemPitch = width * 4;

    d3dcheck(gDevice->CreateTexture2D(&texDesc, &texData, &outTexture));
    d3dcheck(gDevice->CreateShaderResourceView(outTexture, nullptr, &outResource));
}

void LoadTexture(const std::string& texturePath, ID3D11Texture2D*& outTexture, ID3D11ShaderResourceView*& outResource)
{
    int x, y, channels;
    void* img = stbi_load(texturePath.c_str(), &x, &y, &channels, 4);
    CreateTexture(static_cast<uint32_t>(x), static_cast<uint32_t>(y), img, outTexture, outResource);
    stbi_image_free(img);
}

void LoadSkyboxTexture(const std::vector<std::string>& texturePaths, ID3D11Texture2D*& outTexture, ID3D11ShaderResourceView*& outResource)
{
    int width, height, channels;
    stbi_image_free(stbi_load(texturePaths.at(0).c_str(), &width, &height, &channels, 4));

    std::vector<void*> surfaces;
    for (const auto& path : texturePaths)
    {
        int w, h, c;
        void* img = stbi_load(path.c_str(), &w, &h, &c, 4);
        surfaces.push_back(img);
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.ArraySize = 6;
    texDesc.Width = (UINT)width;
    texDesc.Height = (UINT)height;
    texDesc.MipLevels = 1;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    D3D11_SUBRESOURCE_DATA data[6];
    for (int i = 0; i < 6; i++)
    {
        data[i].pSysMem = surfaces[i];
        data[i].SysMemPitch = 4 * width;
        data[i].SysMemSlicePitch = 0;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    d3dcheck(gDevice->CreateTexture2D(&texDesc, data, &outTexture));
    d3dcheck(gDevice->CreateShaderResourceView(outTexture, &srvDesc, &outResource));

    for (auto& s : surfaces)
        stbi_image_free(s);
}

void Init()
{
#if defined( DEBUG ) || defined( _DEBUG )
    shaderCompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // create swapchain
    DXGI_SWAP_CHAIN_DESC swapchainDesc = {};
    swapchainDesc.BufferCount = 1;
    swapchainDesc.BufferDesc.Width = gWindowWidth;
    swapchainDesc.BufferDesc.Height = gWindowHeight;
    swapchainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapchainDesc.BufferDesc.RefreshRate.Numerator = 0;
    swapchainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.OutputWindow = gWindow;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.Windowed = true;
    swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    HRESULT swapchainHR = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, D3D11_CREATE_DEVICE_DEBUG, 0, 0,
        D3D11_SDK_VERSION, &swapchainDesc, &gSwapChain, &gDevice, 0, &gContext);

    d3dcheck(swapchainHR);

    // back buffer...
    ID3D11Texture2D* backBuffer;
    d3dcheck(gSwapChain->GetBuffer(0 /*first buffer (back buffer)*/, __uuidof(ID3D11Texture2D), (void**)&backBuffer));
    d3dcheck(gDevice->CreateRenderTargetView(backBuffer, nullptr, &gBackBufferView));
    backBuffer->Release();

    {
        // depth buffer
        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depthDesc.Width = gWindowWidth;
        depthDesc.Height = gWindowHeight;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.SampleDesc.Count = 1;

        ID3D11Texture2D* depthBuffer;
        d3dcheck(gDevice->CreateTexture2D(&depthDesc, nullptr, &depthBuffer));
        d3dcheck(gDevice->CreateDepthStencilView(depthBuffer, nullptr, &gDepthBufferView));
        depthBuffer->Release();

        D3D11_VIEWPORT viewport = {};
        viewport.Width = gWindowWidth;
        viewport.Height = gWindowHeight;
        viewport.MaxDepth = 1.0f;
        viewport.MinDepth = 0.0f;

        gContext->RSSetViewports(1, &viewport);
    }
    
    {
        D3D11_DEPTH_STENCIL_DESC dssDesc;
        ZeroMemory(&dssDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
        dssDesc.DepthEnable = true;
        dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dssDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

        d3dcheck(gDevice->CreateDepthStencilState(&dssDesc, &gStencilStateSkybox));
    }

    // vertex shader
    {
        ID3DBlob* vsBlob;
        d3dcheck(D3DCompileFromFile(L"shaders/vertex.hlsl", nullptr, nullptr, "main", "vs_5_0", shaderCompileFlags, 0, &vsBlob, nullptr));
        d3dcheck(gDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShaderDefault));

        D3D11_INPUT_ELEMENT_DESC inputs[3] = {};

        inputs[0].SemanticName = "POS";
        inputs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
        inputs[0].AlignedByteOffset = 0;

        inputs[1].SemanticName = "NORMAL";
        inputs[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
        inputs[1].AlignedByteOffset = 12;

        inputs[2].SemanticName = "TEX_COORDS";
        inputs[2].Format = DXGI_FORMAT_R32G32_FLOAT;
        inputs[2].AlignedByteOffset = 24;

        d3dcheck(gDevice->CreateInputLayout(inputs, sizeof(inputs) / sizeof(D3D11_INPUT_ELEMENT_DESC), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout));
        vsBlob->Release();
    }

    // default pixel
    {
        ID3DBlob* psBlob;
        d3dcheck(D3DCompileFromFile(L"shaders/pixel.hlsl", nullptr, nullptr, "main", "ps_5_0", shaderCompileFlags, 0, &psBlob, nullptr));
        d3dcheck(gDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &gPixelShaderDefault));
        psBlob->Release();
    }

    // collider pixel
    {
        ID3DBlob* psBlob;
        d3dcheck(D3DCompileFromFile(L"shaders/pixel_collider.hlsl", nullptr, nullptr, "main", "ps_5_0", shaderCompileFlags, 0, &psBlob, nullptr));
        d3dcheck(gDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &gPixelShaderCollider));
        psBlob->Release();
    }

    // vertex shader skybox
    {
        ID3DBlob* vsBlob;
        d3dcheck(D3DCompileFromFile(L"shaders/Skybox_VS.hlsl", nullptr, nullptr, "main", "vs_5_0", shaderCompileFlags, 0, &vsBlob, nullptr));
        d3dcheck(gDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShaderSkybox));

        D3D11_INPUT_ELEMENT_DESC inputs[1] = {};

        inputs[0].SemanticName = "Position";
        inputs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
        inputs[0].AlignedByteOffset = 0;

        d3dcheck(gDevice->CreateInputLayout(inputs, sizeof(inputs) / sizeof(D3D11_INPUT_ELEMENT_DESC), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayoutSkybox));
        vsBlob->Release();
    }

    // pixel shader skybox
    {
        ID3DBlob* psBlob;
        d3dcheck(D3DCompileFromFile(L"shaders/Skybox_PS.hlsl", nullptr, nullptr, "main", "ps_5_0", shaderCompileFlags, 0, &psBlob, nullptr));
        d3dcheck(gDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &gPixelShaderSkybox));
        psBlob->Release();
    }

    // sampler for textures
    ID3D11SamplerState* sampler;

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    d3dcheck(gDevice->CreateSamplerState(&samplerDesc, &sampler));

    // rasterizer state
    D3D11_RASTERIZER_DESC rasterDesc = {};

    rasterDesc.FrontCounterClockwise = false;
    rasterDesc.DepthBias = 0;
    rasterDesc.SlopeScaledDepthBias = 0.0f;
    rasterDesc.DepthBiasClamp = 0.0f;
    rasterDesc.DepthClipEnable = true;
    rasterDesc.ScissorEnable = false;
    rasterDesc.MultisampleEnable = false;
    rasterDesc.AntialiasedLineEnable = false;

    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_BACK;      // DEFAULT
    d3dcheck(gDevice->CreateRasterizerState(&rasterDesc, &gRasterStateDefault));

    rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    rasterDesc.CullMode = D3D11_CULL_NONE;      // WIREFRAME (colliders)
    d3dcheck(gDevice->CreateRasterizerState(&rasterDesc, &gRasterStateWireframe));


    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;      
    d3dcheck(gDevice->CreateRasterizerState(&rasterDesc, &gRasterStateSkybox));

    // mvp buffer
    D3D11_BUFFER_DESC mvpBufferDesc = {};
    mvpBufferDesc.ByteWidth = sizeof(MVPBuffer);
    mvpBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    mvpBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    mvpBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    d3dcheck(gDevice->CreateBuffer(&mvpBufferDesc, nullptr, &gMVPBuffer));

    // light settings buffer
    D3D11_BUFFER_DESC lightBufferDesc = {};
    lightBufferDesc.ByteWidth = sizeof(LightSettings);
    lightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    lightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    d3dcheck(gDevice->CreateBuffer(&lightBufferDesc, nullptr, &gLightBuffer));

    // bind everyting
    gContext->OMSetRenderTargets(1, &gBackBufferView, gDepthBufferView);

    gContext->VSSetShader(vertexShaderDefault, nullptr, 0);
    gContext->IASetInputLayout(inputLayout);

    gContext->VSSetConstantBuffers(0, 1, &gMVPBuffer);
    gContext->PSSetConstantBuffers(0, 1, &gLightBuffer);

    gContext->PSSetSamplers(0, 1, &sampler);

    gContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // load meshes
    LoadMesh("meshes/dbd.obj", sMeshes.emplace_back());
    LoadMesh("meshes/cube.obj", sMeshes.emplace_back());
    LoadMesh("meshes/lamp.obj", sMeshes.emplace_back());
    LoadMesh("meshes/negan.obj", sMeshes.emplace_back());
    LoadMesh("meshes/terrain.obj", sMeshes.emplace_back());

    // load textures
    ID3D11Texture2D* dummy;
    LoadTexture("textures/dbd/0.png", dummy, sMeshes[0].submeshes[0].texture);
    LoadTexture("textures/dbd/1.png", dummy, sMeshes[0].submeshes[1].texture);
    LoadTexture("textures/dbd/2.png", dummy, sMeshes[0].submeshes[2].texture);
    LoadTexture("textures/dbd/3.png", dummy, sMeshes[0].submeshes[3].texture);
    LoadTexture("textures/dbd/4.png", dummy, sMeshes[0].submeshes[4].texture);
    LoadTexture("textures/dbd/5.png", dummy, sMeshes[0].submeshes[5].texture);
    LoadTexture("textures/dbd/6.png", dummy, sMeshes[0].submeshes[6].texture);
    LoadTexture("textures/dbd/7.png", dummy, sMeshes[0].submeshes[7].texture);

    uint32_t whiteTexture = 0xffffffff;
    ID3D11ShaderResourceView* whiteTextRes;
    CreateTexture(1, 1, &whiteTexture, dummy, whiteTextRes);

    LoadTexture("textures/lamp/0.jpg", dummy, sMeshes[2].submeshes[0].texture);

    LoadTexture("textures/negan/0.png", dummy, sMeshes[3].submeshes[0].texture);

    sMeshes[1].submeshes[0].texture = whiteTextRes;
    sMeshes[4].submeshes[0].texture = whiteTextRes;

    {
        constexpr float side = 1.0f / 2.0f;
        MeshVertex skyboxVertices[] = {
        {{ -side,-side,-side }, {}, {} },
        {{ side,-side,-side }, {}, {} },
        {{ -side,side,-side }, {}, {} },
        {{ side,side,-side }, {}, {} },
        {{ -side,-side,side }, {}, {} },
        {{ side,-side,side }, {}, {} },
        {{ -side,side,side }, {}, {} },
        {{ side,side,side }, {}, {} }
        };

        uint32_t skyboxIndices[] = {
                0,2,1, 2,3,1,
                1,3,5, 3,7,5,
                2,6,3, 3,6,7,
                4,5,7, 4,7,6,
                0,4,2, 2,4,6,
                0,1,4, 1,5,4 };

        auto& boxMesh = sMeshes.emplace_back();

        LoadMesh(skyboxVertices, 8, skyboxIndices, 36, boxMesh);

        std::vector<std::string> surfaces = { "textures/sky/1.png", "textures/sky/0.png", "textures/sky/2.png", "textures/sky/3.png", "textures/sky/4.png", "textures/sky/5.png" };
        LoadSkyboxTexture(surfaces, dummy, boxMesh.submeshes[0].texture);
    }

    // set default mesh
    sMeshIndex = 1;

    // imgui
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(gWindow);
    ImGui_ImplDX11_Init(gDevice, gContext);


    // physics

    ///collision configuration contains default setup for memory, collision setup. Advanced users can create their own configuration.
    collisionConfiguration = new btDefaultCollisionConfiguration();

    ///use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
    dispatcher = new btCollisionDispatcher(collisionConfiguration);

    ///btDbvtBroadphase is a good general purpose broadphase. You can also try out btAxis3Sweep.
    overlappingPairCache = new btDbvtBroadphase();

    ///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
    solver = new btSequentialImpulseConstraintSolver;

    dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);

    dynamicsWorld->setGravity(btVector3(0, -10, 0));

    // physics - terrain TODO: semplificare

    if constexpr (COLLIDER_MESH_INDEX == 4)
    {
        btTriangleMesh* triangleMesh = new btTriangleMesh();

        const std::vector<MeshVertex>& vertices = sMeshes[COLLIDER_MESH_INDEX].vertices;
        const std::vector<uint32_t>& indices = sMeshes[COLLIDER_MESH_INDEX].indices;

        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            const MeshVertex& v1 = vertices[indices[i + 0]];
            const MeshVertex& v2 = vertices[indices[i + 1]];
            const MeshVertex& v3 = vertices[indices[i + 2]];

            triangleMesh->addTriangle
            (
                btVector3(v1.pos.x, v1.pos.y, v1.pos.z),
                btVector3(v2.pos.x, v2.pos.y, v2.pos.z),
                btVector3(v3.pos.x, v3.pos.y, v3.pos.z)
            );
        }

        btBvhTriangleMeshShape* triangleShape = new btBvhTriangleMeshShape(triangleMesh, false);

        btTransform t;
        t.setIdentity();
        t.setOrigin(btVector3(0, 0, 0));

        btDefaultMotionState* motion = new btDefaultMotionState(t);

        // Create the rigid body as before
        btRigidBody::btRigidBodyConstructionInfo info(0.0, motion, triangleShape, btVector3(0, 0, 0));
        info.m_friction = 3.0f;

        btRigidBody* meshRigidBody = new btRigidBody(info);

        triangleShape->setLocalScaling(btVector3(1, 1, 1));

        dynamicsWorld->addRigidBody(meshRigidBody);

        groundPhysShape = triangleShape;
        groundPhysRigidBody = meshRigidBody;

    }
    else
    {
        btCollisionShape* colShape = new btBoxShape({ 0.5f, 0.5f, 0.5f });
        //collisionShapes.push_back(colShape);

        /// Create Dynamic Objects
        btTransform startTransform;
        startTransform.setIdentity();
        startTransform.setOrigin({ 0, -2, 0 });

        btScalar mass(0.f);

        //rigidbody is dynamic if and only if mass is non zero, otherwise static
        bool isDynamic = (mass != 0.f);

        btVector3 localInertia(0, 0, 0);
        if (isDynamic)
            colShape->calculateLocalInertia(mass, localInertia);

        colShape->setLocalScaling(btVector3(1, 1, 1));

        //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, colShape, localInertia);
        rbInfo.m_restitution = 0.5f;
        btRigidBody* body = new btRigidBody(rbInfo);

        dynamicsWorld->addRigidBody(body);

        groundPhysShape = colShape;
        groundPhysRigidBody = body;
    }
}

void Update()
{
    // move camera
    constexpr uint8_t KEY_W = 0x57;
    constexpr uint8_t KEY_A = 0x41;
    constexpr uint8_t KEY_S = 0x53;
    constexpr uint8_t KEY_D = 0x44;
    constexpr uint8_t KEY_Q = 0x51;
    constexpr uint8_t KEY_E = 0x45;
    constexpr uint8_t KEY_SPACE = 0x20;
    constexpr uint8_t KEY_SHIFT = 0xA0;


    if (gKeyboard[KEY_W])
        sCamera.location.z += sCamera.speed;

    if (gKeyboard[KEY_S])
        sCamera.location.z -= sCamera.speed;

    if (gKeyboard[KEY_D])
        sCamera.location.x += sCamera.speed;

    if (gKeyboard[KEY_A])
        sCamera.location.x -= sCamera.speed;

    if (gKeyboard[KEY_E])
        sCamera.location.y += sCamera.speed;

    if (gKeyboard[KEY_Q])
        sCamera.location.y -= sCamera.speed;

    if (gKeyboard[KEY_SPACE])
        sCamera.location.y += sCamera.speed;

    if (gKeyboard[KEY_SHIFT])
        sCamera.location.y -= sCamera.speed;

    // physics

    if (simulatePhysics)
    {
        dynamicsWorld->stepSimulation(1.f / 60.f, 10);
        for (int j = dynamicsWorld->getNumCollisionObjects() - 1; j >= 0; j--)
        {
            btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[j];
            btRigidBody* body = btRigidBody::upcast(obj);

            if (body->getMass())
            {
                btTransform trans;
            
                if (body && body->getMotionState())
                {
                    body->getMotionState()->getWorldTransform(trans);
                }
                else
                {
                    trans = obj->getWorldTransform();
                }
            
                sModelTransform.location.x = trans.getOrigin().x();
                sModelTransform.location.y = trans.getOrigin().y();
                sModelTransform.location.z = trans.getOrigin().z();

                sModelTransform.rotation = trans.getRotation();

                static Vec3 prevRot = {};

                Vec3 newRot = {};
                sModelTransform.rotation.getEulerZYX(newRot.z, newRot.y, newRot.x);

                Vec3 deltaRot = { newRot.x - prevRot.x, newRot.y - prevRot.y, newRot.z - prevRot.z };


                sModelTransform.eulerRotation.x += deltaRot.x;
                sModelTransform.eulerRotation.y += deltaRot.y;
                sModelTransform.eulerRotation.z += deltaRot.z;
 
                prevRot = newRot;

            }
            //printf("world pos object %d = %f,%f,%f\n", j, float(trans.getOrigin().getX()), float(trans.getOrigin().getY()), float(trans.getOrigin().getZ()));
        }
    }
}

void ImguiRender()
{
    ImGui::Begin("Settings");

    ImGui::PushID("physics");
    ImGui::Text("Physics");

    if (ImGui::Button("Start simulation"))
    {
        // "player" collider
        {
#if 1
            btCollisionShape* colShape = new btBoxShape(btVector3(sModelTransform.scale.x / 2.0f,
                sModelTransform.scale.y / 2.0f,
                sModelTransform.scale.z / 2.0f));
            //collisionShapes.push_back(colShape);

            /// Create Dynamic Objects
            btTransform startTransform;
            startTransform.setIdentity();

            btScalar mass(1.f);

            //rigidbody is dynamic if and only if mass is non zero, otherwise static
            bool isDynamic = (mass != 0.f);

            btVector3 localInertia(0, 0, 0);
            if (isDynamic)
                colShape->calculateLocalInertia(mass, localInertia);

            startTransform.setOrigin(btVector3(btScalar(sModelTransform.location.x), btScalar(sModelTransform.location.y), btScalar(sModelTransform.location.z)));
            startTransform.setRotation(sModelTransform.rotation);

            colShape->setLocalScaling(btVector3(sModelTransform.scale.x, sModelTransform.scale.y, sModelTransform.scale.z));

            //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
            btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
            btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, colShape, localInertia);
            rbInfo.m_restitution = 0.5f;
            btRigidBody* body = new btRigidBody(rbInfo);

            cubePhysShape = colShape;
            cubePhysRigidBody = body;

#else
            btTriangleMesh* triangleMesh = new btTriangleMesh();

             const std::vector<MeshVertex>& vertices = sMeshes[sMeshIndex].vertices;
             const std::vector<uint32_t>& indices = sMeshes[sMeshIndex].indices;

             for (uint32_t i = 0; i < indices.size(); i += 3)
             {
                 const MeshVertex& v1 = vertices[indices[i + 0]];
                 const MeshVertex& v2 = vertices[indices[i + 1]];
                 const MeshVertex& v3 = vertices[indices[i + 2]];

                 triangleMesh->addTriangle
                 (
                     btVector3(v1.pos.x, v1.pos.y, v1.pos.z),
                     btVector3(v2.pos.x, v2.pos.y, v2.pos.z),
                     btVector3(v3.pos.x, v3.pos.y, v3.pos.z)
                 );
             }

             btBvhTriangleMeshShape* triangleShape = new btBvhTriangleMeshShape(triangleMesh, false);

             btTransform t;
             t.setIdentity();
             t.setOrigin(btVector3(0, 0, 0));

             btScalar mass(1.f);

                 //rigidbody is dynamic if and only if mass is non zero, otherwise static
            bool isDynamic = (mass != 0.f);

            /*btVector3 localInertia(0, 0, 0);
                 if (isDynamic)
                     triangleShape->calculateLocalInertia(mass, localInertia);*/

             btDefaultMotionState* motion = new btDefaultMotionState(t);

             // Create the rigid body as before
             btRigidBody::btRigidBodyConstructionInfo info(0.0, motion, triangleShape, btVector3(0, 0, 0));
             info.m_friction = 3.0f;

             btRigidBody* meshRigidBody = new btRigidBody(info);

             triangleShape->setLocalScaling(btVector3(1, 1, 1));

             dynamicsWorld->addRigidBody(meshRigidBody);

             cubePhysShape = triangleShape;
             cubePhysRigidBody = meshRigidBody;
#endif
        }
        dynamicsWorld->addRigidBody(cubePhysRigidBody);
        simulatePhysics = true;
    }

    ImGui::PopID();

    ImGui::PushID("model");
    ImGui::Text("Model");
    
    if (ImGui::BeginCombo("Mesh", sMeshes[sMeshIndex].debugName.c_str()))
    {
        for (uint32_t i = 0; i < sMeshes.size(); i++)
            if (ImGui::Selectable(sMeshes[i].debugName.c_str(), i == sMeshIndex))
                sMeshIndex = i;

        ImGui::EndCombo();
    }
    
    ImGui::DragFloat3("Location", &sModelTransform.location.x, 0.03f);


    ImGui::DragFloat3("Rotation", &sModelTransform.eulerRotation.x, 0.5f);
    sModelTransform.rotation.setEulerZYX(sModelTransform.eulerRotation.z * TO_RADIANS, sModelTransform.eulerRotation.y * TO_RADIANS, sModelTransform.eulerRotation.x * TO_RADIANS);


    ImGui::DragFloat3("Scale", &sModelTransform.scale.x, 0.05f);
    ImGui::PopID();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushID("camera");
    ImGui::Text("Camera");
    ImGui::DragFloat3("Location", &sCamera.location.x, 0.03f);
    ImGui::DragFloat3("Direction", &sCamera.rotation.x, 0.03f);
    ImGui::DragFloat("FOV", &sCamera.FOV, 0.05f);
    ImGui::SliderFloat("Speed", &sCamera.speed, 0.03f, 0.1f);
    ImGui::PopID();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushID("light");
    ImGui::Text("Light settings");
    ImGui::DragFloat3("Position", &sLightSettings.pos.x, 0.03f);
    ImGui::ColorEdit3("Ambient color", &sLightSettings.ambientColor.x);
    ImGui::ColorEdit3("Light color", &sLightSettings.lightColor.x);
    ImGui::DragFloat("Ambient intensity", &sLightSettings.ambientStrength);
    ImGui::DragFloat("Specular intensity", &sLightSettings.specularStrength);
    ImGui::DragFloat("Specular power", &sLightSettings.specularPow);
    ImGui::PopID();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushID("background");
    ImGui::Text("Scene background settings");
    ImGui::ColorEdit3("Color", &sSceneSettings.color.x);
    //ImGui::DragFloat("Intensity", &sLightSettings.intensity, 0.001f, 0.0f, 1.0f);
    ImGui::PopID();

    ImGui::End();
}

void Render()
{
    //float clearColor[] = { sSceneSettings.color.x, sSceneSettings.color.y, sSceneSettings.color.z, sSceneSettings.color.w };
    gContext->ClearRenderTargetView(gBackBufferView, &sSceneSettings.color.x);
    gContext->ClearDepthStencilView(gDepthBufferView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0.0f);

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    ImguiRender();

    // camera
    MVPBuffer mvp;

    mvp.model = DirectX::XMMatrixIdentity();
    mvp.inverseModel = DirectX::XMMatrixIdentity();

    mvp.view = XMMatrixTranspose(DirectX::XMMatrixLookToLH
    (
        { sCamera.location.x, sCamera.location.y, sCamera.location.z },
        { sCamera.rotation.x, sCamera.rotation.y, sCamera.rotation.z },
        { 0.0f, 1.0f, 0.0f }
    ));
    
    mvp.proj = XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovLH(sCamera.FOV * TO_RADIANS, (float)gWindowWidth / (float)gWindowHeight, 0.01f, 1000.0f));


    // lighting
    sLightSettings.camPos = { sCamera.location.x, sCamera.location.y, sCamera.location.z };

    D3D11_MAPPED_SUBRESOURCE lightMap;
    d3dcheck(gContext->Map(gLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &lightMap));
    memcpy(lightMap.pData, &sLightSettings, sizeof(LightSettings));
    gContext->Unmap(gLightBuffer, 0);

    gContext->VSSetShader(vertexShaderSkybox, nullptr, 0);
    gContext->OMSetDepthStencilState(gStencilStateSkybox, 0);
    gContext->IASetInputLayout(inputLayoutSkybox);

    // skybox
    {

        // update mvp
        DirectX::XMMATRIX model = DirectX::XMMatrixScaling(1000, 1000, 1000)
                                * DirectX::XMMatrixTranslation(sCamera.location.x, sCamera.location.y, sCamera.location.z);

        mvp.model = DirectX::XMMatrixTranspose(model);
        mvp.inverseModel = DirectX::XMMatrixInverse(nullptr, model);

        D3D11_MAPPED_SUBRESOURCE mvpMap;
        d3dcheck(gContext->Map(gMVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mvpMap));
        memcpy(mvpMap.pData, &mvp, sizeof(MVPBuffer));
        gContext->Unmap(gMVPBuffer, 0);

        // bind everything
        uint32_t strides[] = { sizeof(MeshVertex) };
        uint32_t offsets[] = { 0 };
        gContext->IASetVertexBuffers(0, 1, &sMeshes[5].vertexBuffer, strides, offsets);

        gContext->IASetIndexBuffer(sMeshes[5].indexBuffer, DXGI_FORMAT_R32_UINT, 0);

        gContext->PSSetShader(gPixelShaderSkybox, nullptr, 0);
        gContext->RSSetState(gRasterStateSkybox);

        // render
        uint32_t indexOffset = 0;
        for (SubMeshData s : sMeshes[5].submeshes)
        {
            gContext->PSSetShaderResources(0, 1, &s.texture);
            gContext->DrawIndexed(s.indexCount, indexOffset, 0);
            indexOffset += s.indexCount;
        }
    }

    gContext->VSSetShader(vertexShaderDefault, nullptr, 0);
    gContext->IASetInputLayout(inputLayout);
    gContext->OMSetDepthStencilState(nullptr, 0);


    // default scene
    {
        // update mvp
        DirectX::XMMATRIX model =
            DirectX::XMMatrixScaling(sModelTransform.scale.x, sModelTransform.scale.y, sModelTransform.scale.z)
            *
            DirectX::XMMatrixRotationQuaternion(sModelTransform.rotation.get128())
            *
            DirectX::XMMatrixTranslation(sModelTransform.location.x, sModelTransform.location.y, sModelTransform.location.z);

        mvp.model = DirectX::XMMatrixTranspose(model);
        mvp.inverseModel = DirectX::XMMatrixInverse(nullptr, model);

        D3D11_MAPPED_SUBRESOURCE mvpMap;
        d3dcheck(gContext->Map(gMVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mvpMap));
        memcpy(mvpMap.pData, &mvp, sizeof(MVPBuffer));
        gContext->Unmap(gMVPBuffer, 0);

        // bind everything
        uint32_t strides[] = { sizeof(MeshVertex) };
        uint32_t offsets[] = { 0 };
        gContext->IASetVertexBuffers(0, 1, &sMeshes[sMeshIndex].vertexBuffer, strides, offsets);

        gContext->IASetIndexBuffer(sMeshes[sMeshIndex].indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        gContext->PSSetShader(gPixelShaderDefault, nullptr, 0);
        gContext->RSSetState(gRasterStateDefault);

        // render
        uint32_t indexOffset = 0;
        for (SubMeshData s : sMeshes[sMeshIndex].submeshes)
        {
            gContext->PSSetShaderResources(0, 1, &s.texture);
            gContext->DrawIndexed(s.indexCount, indexOffset, 0);
            indexOffset += s.indexCount;
        }
    }

    // collider
    {
        // update mvp
        btTransform tranform = groundPhysRigidBody->getWorldTransform();
        btVector3 scaling = groundPhysShape->getLocalScaling();

        DirectX::XMMATRIX model =
            DirectX::XMMatrixScaling(scaling.x(), scaling.y(), scaling.z())
            *
            DirectX::XMMatrixRotationQuaternion(tranform.getRotation().get128())
            *
            DirectX::XMMatrixTranslation(tranform.getOrigin().x(), tranform.getOrigin().y(), tranform.getOrigin().z());

        mvp.model = DirectX::XMMatrixTranspose(model);
        mvp.inverseModel = DirectX::XMMatrixInverse(nullptr, model);

        D3D11_MAPPED_SUBRESOURCE mvpMap;
        d3dcheck(gContext->Map(gMVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mvpMap));
        memcpy(mvpMap.pData, &mvp, sizeof(MVPBuffer));
        gContext->Unmap(gMVPBuffer, 0);

        // bind everything
        uint32_t strides[] = { sizeof(MeshVertex) };
        uint32_t offsets[] = { 0 };
        gContext->IASetVertexBuffers(0, 1, &sMeshes[COLLIDER_MESH_INDEX].vertexBuffer, strides, offsets);

        gContext->IASetIndexBuffer(sMeshes[COLLIDER_MESH_INDEX].indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        gContext->PSSetShader(gPixelShaderCollider, nullptr, 0);
        gContext->RSSetState(gRasterStateWireframe);

        // render
        uint32_t indexOffset = 0;
        for (SubMeshData s : sMeshes[COLLIDER_MESH_INDEX].submeshes)
        {
            //gContext->PSSetShaderResources(0, 1, &s.texture);
            gContext->DrawIndexed(s.indexCount, indexOffset, 0);
            indexOffset += s.indexCount;
        }
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    gSwapChain->Present(1, 0);
}

LRESULT WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam);

    switch (Msg)
    {
        case WM_KEYDOWN:
            gKeyboard[(uint8_t)wParam] = true;
            break;

        case WM_KEYUP:
            gKeyboard[(uint8_t)wParam] = false;
            break;

        case WM_SIZE:
        {
            gWindowWidth = LOWORD(lParam);
            gWindowHeight = HIWORD(lParam);

            if (gSwapChain)
            {
                // release refs...
                gBackBufferView->Release();
                gDepthBufferView->Release();

                // resize back and front buffers...
                gSwapChain->ResizeBuffers(1, gWindowWidth, gWindowHeight, DXGI_FORMAT_UNKNOWN, 0);

                // get updated ref to back buffer
                ID3D11Texture2D* backBuffer;
                d3dcheck(gSwapChain->GetBuffer(0 /*first buffer (back buffer)*/, __uuidof(ID3D11Texture2D), (void**)&backBuffer));
                d3dcheck(gDevice->CreateRenderTargetView(backBuffer, nullptr, &gBackBufferView));
                backBuffer->Release();

                // create new depth buffer
                D3D11_TEXTURE2D_DESC depthDesc = {};
                depthDesc.Usage = D3D11_USAGE_DEFAULT;
                depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
                depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                depthDesc.Width = gWindowWidth;
                depthDesc.Height = gWindowHeight;
                depthDesc.MipLevels = 1;
                depthDesc.ArraySize = 1;
                depthDesc.SampleDesc.Count = 1;

                ID3D11Texture2D* depthBuffer;
                d3dcheck(gDevice->CreateTexture2D(&depthDesc, nullptr, &depthBuffer));
                d3dcheck(gDevice->CreateDepthStencilView(depthBuffer, nullptr, &gDepthBufferView));
                depthBuffer->Release();

                // update viwpoert
                D3D11_VIEWPORT viewport = {};
                viewport.Width = gWindowWidth;
                viewport.Height = gWindowHeight;
                viewport.MaxDepth = 1.0f;
                viewport.MinDepth = 0.0f;

                gContext->RSSetViewports(1, &viewport);

                // set updated render targets
                gContext->OMSetRenderTargets(1, &gBackBufferView, gDepthBufferView);
            }
        }
        break;

        case WM_CLOSE:
            gAppShouldRun = false;
            break;
        default: 
            break;
    }

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    WNDCLASSEXA wndClass = {};
    wndClass.cbSize = sizeof(WNDCLASSEXA);
    wndClass.hInstance = hInst;
    wndClass.lpfnWndProc = WndProc;
    wndClass.lpszClassName = "boh";

    check(RegisterClassExA(&wndClass));

    gWindow = CreateWindowExA(0, "boh", "lighting test", WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX | WS_SYSMENU,
                100, 100, gWindowWidth, gWindowHeight, nullptr, nullptr, hInst, nullptr);

    check(gWindow);

    ShowWindow(gWindow, SW_SHOW);

    Init();

    while (gAppShouldRun)
    {
        MSG msg;
        while (PeekMessageA(&msg, gWindow, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        Update();
        Render();
    }

    return 0;
}
