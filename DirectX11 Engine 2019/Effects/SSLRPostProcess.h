#pragma once

#include "Engine Includes/MainInclude.h"

struct SSLRArgs {
    mfloat4x4 _mWorldView;
    mfloat4x4 _mProj;
    float1 _ViewAngleThreshold;
    float1 _EdgeDistThreshold;
    float1 _DepthBias;
    float1 _ReflScale;
    float1 _CameraNear;  // Camera's near clip plane
    float1 _CameraFar;   // Camera's far clip plane
};

class SSLRPostProcess {
private:
    struct xMatrices {
        mfloat4x4 _mWorldViewProj;
        mfloat4x4 _mWorldView;
    };

    struct xSSLRSettings {
        mfloat4x4 _mProj;
        float1 _ViewAngleThreshold;
        float1 _EdgeDistThreshold;
        float1 _DepthBias;
        float1 _ReflScale;
        float4 _ProjValues;
        float1 _PixelSize; // 2.f / Height
        uint1  _NumSteps;  // Width
        float2 _Alignment;
    };

    Sampler *_PointSampler;
    BlendState *bsAdditive;
    DepthStencilState *dssReadOnly;

    RenderBufferColor1Depth *rtSSLR;

    ConstantBuffer *cbMatrices;
    ConstantBuffer *cbSSLRSettings;

    Shader *shSSLRNew;
    Shader *shAlphaBlend1;

    // Saves last RT's size.
    // So we can do something later
    float fWidth;
    float fHeight;
public:
    SSLRPostProcess() {
        // Create blend state
        bsAdditive = new BlendState();
        D3D11_BLEND_DESC pBlendDesc;
        pBlendDesc.IndependentBlendEnable                = false;
        pBlendDesc.AlphaToCoverageEnable                 = false;
        pBlendDesc.RenderTarget[0].BlendEnable           = true;
        pBlendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        pBlendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_ONE;
        pBlendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        pBlendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_SRC_ALPHA;
        pBlendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
        pBlendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        pBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        bsAdditive->Create(pBlendDesc, {});

        // Create depth stencil state
        dssReadOnly = new DepthStencilState();
        D3D11_DEPTH_STENCIL_DESC pDSSDesc;

        const D3D11_DEPTH_STENCILOP_DESC DefaultStencilOP = {
            D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
            D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS
        };

        pDSSDesc.DepthEnable      = false;
        pDSSDesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;
        pDSSDesc.DepthFunc        = D3D11_COMPARISON_GREATER;
        pDSSDesc.StencilEnable    = false;
        pDSSDesc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
        pDSSDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
        pDSSDesc.FrontFace        = DefaultStencilOP;
        pDSSDesc.BackFace         = DefaultStencilOP;

        dssReadOnly->Create(pDSSDesc);

        // Create sampler
        _PointSampler = new Sampler();
        D3D11_SAMPLER_DESC pSamplerDesc = {};
        pSamplerDesc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        pSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        pSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        pSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

        _PointSampler->Create(pSamplerDesc);

        // Default screen size
        UINT Width = 1366;
        UINT Height = 768;

        // Create texture
        rtSSLR = new RenderBufferColor1Depth();
        rtSSLR->SetSize(Width, Height);
        //rtSSLR->EnableMSAA();
        rtSSLR->CreateColor0(DXGI_FORMAT_R16G16B16A16_FLOAT);
        rtSSLR->CreateDepth(32);

        // Create constant buffers
        cbMatrices = new ConstantBuffer();
        cbMatrices->CreateDefault(sizeof(xMatrices));
        cbMatrices->SetName("SSLR Matrices");

        cbSSLRSettings = new ConstantBuffer();
        cbSSLRSettings->CreateDefault(sizeof(xSSLRSettings));
        cbSSLRSettings->SetName("SSLR Settings");

        // Load shaders
        shSSLRNew = new Shader();
        shSSLRNew->LoadFile("shSSLR_NewVS.cso", Shader::Vertex);
        shSSLRNew->LoadFile("shSSLR_NewPS.cso", Shader::Pixel);
        shSSLRNew->ReleaseBlobs();

        shAlphaBlend1 = new Shader();
        shAlphaBlend1->LoadFile("shDirectTexcoordVS.cso", Shader::Vertex);
        shAlphaBlend1->LoadFile("shAlphaBlend1PS.cso", Shader::Pixel);
        shAlphaBlend1->ReleaseBlobs();
    }

    ~SSLRPostProcess() {
        bsAdditive->Release();
        dssReadOnly->Release();
        
        rtSSLR->Release();
        
        shAlphaBlend1->Release();
        shSSLRNew->Release();

        delete shSSLRNew;
        delete shAlphaBlend1;

        delete bsAdditive;
        delete dssReadOnly;
        delete _PointSampler;

        delete rtSSLR;
    }

    void Resize(UINT Width, UINT Height) {
        rtSSLR->Resize(Width, Height);
    }

    // RB must contain depth buffer
    // RB must contain diffuse buffer
    void Begin(RenderBufferBase* RB, sRenderBuffer* Normals, const SSLRArgs& args) {
        // Get depth buffer size
        fWidth  = RB->GetWidth();
        fHeight = RB->GetHeight();

        // Prepare constant buffers
        float fQ = args._CameraFar / (args._CameraNear - args._CameraFar);

        float4x4 dest;
        DirectX::XMStoreFloat4x4(&dest, DirectX::XMMatrixTranspose(args._mProj));

        xSSLRSettings* SSLRSett = (xSSLRSettings*)cbSSLRSettings->Map();
            SSLRSett->_ProjValues         = { args._CameraNear * fQ, fQ, 1.f / dest.m[0][0], 1.f / dest.m[1][1] };
            SSLRSett->_ViewAngleThreshold = args._ViewAngleThreshold;
            SSLRSett->_EdgeDistThreshold  = args._EdgeDistThreshold;
            SSLRSett->_DepthBias          = args._DepthBias;
            SSLRSett->_ReflScale          = args._ReflScale;
            SSLRSett->_PixelSize          = 2.f / fHeight;
            SSLRSett->_NumSteps           = fWidth;
            SSLRSett->_mProj              = args._mProj;
        cbSSLRSettings->Unmap();

        // Store state
        DepthStencilState::Push();

        // Bind and Clear RT
        const float Color[4] = { 0.f, 0.f, 0.f, 1.f };
        // Bind RT
        rtSSLR->Bind();
        rtSSLR->Clear(Color, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f);

        // Copy depth
        gDirectX->gContext->CopyResource(rtSSLR->GetDepthB()->pTexture2D, RB->GetDepthB()->pTexture2D);

        // Set new dss state
        dssReadOnly->Bind();

        // Bind shader
        shSSLRNew->Bind();

        // Bind resources
        cbSSLRSettings->Bind(Shader::Pixel, 0);
        cbMatrices->Bind(Shader::Vertex, 0);

        RB->BindResource(RB->GetColor0(), Shader::Pixel, 0); // SRV
        RB->BindResource(Normals        , Shader::Pixel, 1); // SRV
        RB->BindResource(RB->GetDepthB(), Shader::Pixel, 2); // SRV

        _PointSampler->Bind(Shader::Pixel, 0);
    }

    void BuildBuffer(const SSLRArgs& args) {
        // Prepare constant buffers

        xMatrices* Matrices = (xMatrices*)cbMatrices->Map();
            Matrices->_mWorldView     = args._mWorldView;
            Matrices->_mWorldViewProj = args._mWorldView * args._mProj;
        cbMatrices->Unmap();
    }

    // Before calling End.
    // 1) Render all meshes with reflective materials
    // 2) Bind RTV with the diffuse color from before
    // to render the final result.
    void End() {
        // Unbind
        LunaEngine::PSDiscardSRV<3>();
        LunaEngine::PSDiscardCB<1>();
        LunaEngine::VSDiscardCB<1>();

        // Bind shader 
        shAlphaBlend1->Bind();

        // Restore old state
        DepthStencilState::Pop();

        // Store old state
        BlendState::Push();

        // Bind resources
        rtSSLR->BindResources(Shader::Pixel, 0); // SRV
        _PointSampler->Bind(Shader::Pixel, 0);

        // Draw call
        gDirectX->gContext->Draw(6, 0);

        // Restore old states
        BlendState::Pop();
        DepthStencilState::Pop();

        LunaEngine::PSDiscardSRV<1>();
    }
};
