#include "RendererDeferred.h"

void RendererDeferred::Init() {
#pragma region Shaders
    // Load shaders
    shSurface = new Shader();
    shSurface->SetLayoutGenerator(LgMesh);
    shSurface->LoadFile("shSurfaceVS.cso", Shader::Vertex);
    shSurface->LoadFile("shSurfacePS.cso", Shader::Pixel);

    shVertexOnly = new Shader();
    shVertexOnly->SetLayoutGenerator(LayoutGenerator::LgMesh);
    shVertexOnly->LoadFile("shSimpleVS.cso", Shader::Vertex); // Change to shSurfaceVS
    shVertexOnly->SetNullShader(Shader::Pixel);

    shPostProcess = new Shader();
    shPostProcess->LoadFile("shPostProcessVS.cso", Shader::Vertex);
    shPostProcess->LoadFile("shPostProcessPS.cso", Shader::Pixel);

    shGUI = new Shader();
    shGUI->AttachShader(shPostProcess, Shader::Vertex);
    shGUI->LoadFile("shGUIPS.cso", Shader::Pixel);

    shCombinationPass = new Shader();
    shCombinationPass->AttachShader(shPostProcess, Shader::Vertex);
    shCombinationPass->LoadFile("shCombinationPassPS.cso", Shader::Pixel);


    // Release blobs
    shSurface->ReleaseBlobs();
    shVertexOnly->ReleaseBlobs();
    shPostProcess->ReleaseBlobs();
    shGUI->ReleaseBlobs();
#pragma endregion

#pragma region Render Targets
    rtGBuffer = new RenderTarget2DColor4DepthMSAA(Width(), Height(), 1, "GBuffer");
    rtGBuffer->Create(32);                                     // Depth
    rtGBuffer->CreateList(0, DXGI_FORMAT_R16G16B16A16_FLOAT,   // Direct
                             DXGI_FORMAT_R16G16B16A16_FLOAT,   // Normals
                             DXGI_FORMAT_R16G16B16A16_FLOAT,   // Ambient
                             DXGI_FORMAT_R16G16B16A16_FLOAT);  // Emission
    
    rtCombinedGBuffer = new RenderTarget2DColor4DepthMSAA(Width(), Height(), 1, "GBuffer combined");
    rtCombinedGBuffer->Create(32);                                     // Depth
    rtCombinedGBuffer->CreateList(0, DXGI_FORMAT_R16G16B16A16_FLOAT,   // Color
                                     DXGI_FORMAT_R16G16B16A16_FLOAT,   // Normals
                                     DXGI_FORMAT_R16G16B16A16_FLOAT,   // 
                                     DXGI_FORMAT_R16G16B16A16_FLOAT);  // 

    rtDepth = new RenderTarget2DDepthMSAA(Width(), Height(), 1, "World light shadowmap");
    rtDepth->Create(32);

    rtFinalPass = new RenderTarget2DColor1(Width(), Height(), 1, "Final Pass");
    rtFinalPass->CreateList(0, DXGI_FORMAT_R8G8B8A8_UNORM);

    rtTransparency = new RenderTarget2DColor2DepthMSAA(Width(), Height(), 1, "Transparency");
    rtTransparency->Create(32);                                     // Depth
    rtTransparency->CreateList(0, DXGI_FORMAT_R8G8B8A8_UNORM,       // Color
                                  DXGI_FORMAT_R16G16B16A16_FLOAT);  // Normals
#pragma endregion

#pragma region Default Textures
    s_material.mCubemap = new CubemapTexture();
    s_material.mCubemap->CreateFromDDS("../Textures/Cubemaps/environment.dds", false);
    
    s_material.tex.checkboard = new Texture();
    s_material.tex.checkboard->Load("../Textures/TileInverse.png", DXGI_FORMAT_R8G8B8A8_UNORM);
    s_material.tex.checkboard->SetName("Default texture");
    
    s_material.tex.bluenoise_rg_512 = new Texture();
    s_material.tex.bluenoise_rg_512->Load("../Textures/Noise/Blue/LDR_RG01_0.png", DXGI_FORMAT_R16G16_UNORM);
    s_material.tex.bluenoise_rg_512->SetName("Bluenoise RG");
#pragma endregion

#pragma region Samplers
    {
        D3D11_SAMPLER_DESC pDesc;
        ZeroMemory(&pDesc, sizeof(D3D11_SAMPLER_DESC));
        pDesc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        pDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        pDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        pDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        pDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        pDesc.MaxLOD = D3D11_FLOAT32_MAX;
        pDesc.MinLOD = 0;
        pDesc.MipLODBias = 0;
        pDesc.MaxAnisotropy = 16;

        // Point sampler
        s_material.sampl.point = new Sampler();
        s_material.sampl.point->Create(pDesc);

        // Compare point sampler
        pDesc.ComparisonFunc = D3D11_COMPARISON_GREATER;
        s_material.sampl.point_comp = new Sampler();
        s_material.sampl.point_comp->Create(pDesc);

        // Linear sampler
        pDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        pDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        s_material.sampl.linear = new Sampler();
        s_material.sampl.linear->Create(pDesc);

        // Compare linear sampler
        pDesc.ComparisonFunc = D3D11_COMPARISON_GREATER;
        s_material.sampl.linear_comp = new Sampler();
        s_material.sampl.linear_comp->Create(pDesc);
    }
#pragma endregion

#pragma region Effects
    gHDRPostProcess             = new HDRPostProcess;
    gSSAOPostProcess            = new SSAOPostProcess;
    gSSLRPostProcess            = new SSLRPostProcess;
    gCascadeShadowMapping       = new CascadeShadowMapping;
    //gCoverageBuffer             = new CoverageBuffer;
    gOrderIndendentTransparency = new OrderIndendentTransparency;
#pragma endregion

#pragma region Blend states
    s_states.blend.normal  = new BlendState;
    s_states.blend.add     = new BlendState;

    {
        D3D11_BLEND_DESC pDesc;
        pDesc.RenderTarget[0].BlendEnable = true;
        pDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        pDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        pDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        pDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        pDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        pDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        pDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        pDesc.AlphaToCoverageEnable = false;
        pDesc.IndependentBlendEnable = false;

        s_states.blend.normal->Create(pDesc, { 1.f, 1.f, 1.f, 1.f });

        // 

        //s_states.blend.add->Create(pDesc, { 1.f, 1.f, 1.f, 1.f });
    }
#pragma endregion

#pragma region Depth stencil states
    s_states.depth.normal = new DepthStencilState;

    {

    }
#pragma endregion

#pragma region Raster states
    s_states.raster.normal          = new RasterState;
    s_states.raster.wire            = new RasterState;
    s_states.raster.normal_scissors = new RasterState;
    s_states.raster.wire_scissors   = new RasterState;

    {
        D3D11_RASTERIZER_DESC pDesc;
        ZeroMemory(&pDesc, sizeof(D3D11_RASTERIZER_DESC));
        pDesc.AntialiasedLineEnable = true;
        pDesc.CullMode              = D3D11_CULL_NONE;
        pDesc.DepthBias             = 0;
        pDesc.DepthBiasClamp        = 0.0f;
        pDesc.DepthClipEnable       = true;
        pDesc.FillMode              = D3D11_FILL_SOLID;
        pDesc.FrontCounterClockwise = false;
        pDesc.MultisampleEnable     = true;
        pDesc.ScissorEnable         = false;
        pDesc.SlopeScaledDepthBias  = 0.0f;

        // Normal
        s_states.raster.normal->Create(pDesc);

        // Normal + Scissors
        pDesc.ScissorEnable = true;
        s_states.raster.normal_scissors->Create(pDesc);

        // Wireframe + Scissors
        pDesc.FillMode = D3D11_FILL_WIREFRAME;
        s_states.raster.wire->Create(pDesc);

        // Wireframe
        pDesc.ScissorEnable = false;
        s_states.raster.wire->Create(pDesc);
    }
#pragma endregion

    cbTransform = new ConstantBuffer();
    cbTransform->CreateDefault(sizeof(TransformBuff));

    IdentityTransf = new TransformComponent;
    IdentityTransf->fAcceleration = 0.f;
    IdentityTransf->fVelocity     = 0.f;
    IdentityTransf->vDirection    = { 0.f, 0.f, 0.f };
    IdentityTransf->vPosition     = { 0.f, 0.f, 0.f };
    IdentityTransf->vRotation     = { 0.f, 0.f, 0.f };
    IdentityTransf->vScale        = { 1.f, 1.f, 1.f };
    IdentityTransf->mWorld        = DirectX::XMMatrixIdentity();
}

void RendererDeferred::Render() {
    ScopedRangeProfiler q0(L"Deferred Renderer");
    mScene = Scene::Current();
    if( !mScene ) {
        printf_s("[%s]: No scene is bound!\n", __FUNCTION__);
        return;
    }

    {
        ScopedRangeProfiler q(L"Geometry rendering");

        Shadows();
        GBuffer();
        OIT();
    }

    {
        ScopedRangeProfiler q(L"Screen-Space");

        // Light calculation
        SSAO();
        SSLR();
        SSLF();
        FSSSSS();
        Deferred();

        // Combination pass
        Combine();
    }

    {
        ScopedRangeProfiler q(L"Final post processing");

        // Final post-processing
        DOF();
        HDR();
        Final();
    }

    {
        ScopedRangeProfiler q(L"Unbind resources");

        // HDR Post Processing
        // Swap buffer data
        gHDRPostProcess->End();

        // SSAO
        // Unbind for further use
        gSSAOPostProcess->End();
    }
}

void RendererDeferred::FinalScreen() {
    shGUI->Bind();

    // Bind resources
    s_material.sampl.point->Bind(Shader::Pixel);
    rtFinalPass->Bind(0u, Shader::Pixel, 0, false);

    BindOrtho();

    // 
    DXDraw(6, 0);
}

void RendererDeferred::Resize(float W, float H) {
    // TODO: 
}

void RendererDeferred::Release() {
    // Shaders
    SAFE_RELEASE(shSurface);
    SAFE_RELEASE(shVertexOnly);
    SAFE_RELEASE(shPostProcess);
    SAFE_RELEASE(shCombinationPass);
    SAFE_RELEASE(shGUI);

    // Render Targets
    SAFE_RELEASE(rtTransparency);
    SAFE_RELEASE(rtGBuffer);
    SAFE_RELEASE(rtDepth);
    SAFE_RELEASE(rtFinalPass);

    // Textures
    SAFE_RELEASE(s_material.tex.bluenoise_rg_512);
    SAFE_RELEASE(s_material.tex.checkboard);
    SAFE_RELEASE(s_material.mCubemap);

    // Samplers
    SAFE_RELEASE(s_material.sampl.point      );
    SAFE_RELEASE(s_material.sampl.point_comp );
    SAFE_RELEASE(s_material.sampl.linear     );
    SAFE_RELEASE(s_material.sampl.linear_comp);

    // Effects
    SAFE_DELETE(gHDRPostProcess            );
    SAFE_DELETE(gSSAOPostProcess           );
    SAFE_DELETE(gSSLRPostProcess           );
    SAFE_DELETE(gCascadeShadowMapping      );
    //SAFE_DELETE(gCoverageBuffer            );
    SAFE_DELETE(gOrderIndendentTransparency);
    
    // States
    SAFE_RELEASE(s_states.blend.normal );
    SAFE_RELEASE(s_states.blend.add    );
    SAFE_RELEASE(s_states.raster.normal);
    SAFE_RELEASE(s_states.raster.wire  );
    SAFE_RELEASE(s_states.raster.normal_scissors);
    SAFE_RELEASE(s_states.raster.wire_scissors  );
    SAFE_RELEASE(s_states.depth.normal );

    // Buffers
    SAFE_RELEASE(cbTransform);

    // Other
    SAFE_DELETE(IdentityTransf);
}

void RendererDeferred::ImGui() {

}

void RendererDeferred::ClearMainRT() {
    gDirectX->gContext->ClearRenderTargetView(gDirectX->gRTV, s_clear.black_void2);
    gDirectX->gContext->ClearDepthStencilView(gDirectX->gDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);
}

void RendererDeferred::Shadows() {
    ScopedRangeProfiler s1(L"World light");

    // Bind render target
    rtDepth->Bind();
    rtDepth->Clear(0.f, 0, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL);

    //gDirectX->gContext->OMSetDepthStencilState(pDSS_Default, 1);
    gDirectX->gContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    shVertexOnly->Bind();
    
    // 
    uint32_t old = mScene->GetActiveCamera();
    mScene->SetActiveCamera(1);
    
    {
        mScene->BindCamera(1, Shader::Vertex, 0); // Light camera
        mScene->RenderOpaque(RendererFlags::ShadowPass, Shader::Vertex);
    }

    mScene->SetActiveCamera(old);
}

void RendererDeferred::GBuffer() {
    ScopedRangeProfiler s1(L"Render GBuffer");

    // Bind render target
    rtGBuffer->Bind();
    rtGBuffer->Clear(0.f, 0, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL);
    rtGBuffer->Clear(s_clear.black_void2);

    // Bind shader
    shSurface->Bind();

    // Bind default material textures
    for( uint32_t i = 0; i < 6; i++ ) {
        s_material.tex.checkboard->Bind(Shader::Pixel, i);
        s_material.sampl.linear->Bind(Shader::Pixel, i);
    }

    // Bind depth buffer
    rtDepth->Bind(0u, Shader::Pixel, 6);
    s_material.sampl.linear_comp->Bind(Shader::Pixel, 6);

    // Bind noise texture
    s_material.tex.bluenoise_rg_512->Bind(Shader::Pixel, 7);
    s_material.sampl.point->Bind(Shader::Pixel, 7);

    // Bind cubemap
    s_material.mCubemap->Bind(Shader::Pixel, 8);
    s_material.sampl.linear->Bind(Shader::Pixel, 8);

    // Bind CBs
    mScene->BindCamera(0, Shader::Vertex, 1); // Main camera
    mScene->BindCamera(1, Shader::Vertex, 2); // Shadow camera

    // Bind material layers
    uint32_t old_ml = mScene->GetEnabledMaterialLayers();
    mScene->SetLayersState(Scene::Default);

    uint32_t old = mScene->GetActiveCamera();
    mScene->SetActiveCamera(0);
    
    {
        mScene->Render(RendererFlags::OpaquePass, Shader::Vertex);
    }

    // Restore states
    mScene->SetActiveCamera(old);
    mScene->SetLayersState(old_ml);

    // Unbind textures
    LunaEngine::PSDiscardSRV<8>();
}

void RendererDeferred::OIT() {
    // 
    gOrderIndendentTransparency->Begin(rtGBuffer);

    // Resolve MSAA
    rtGBuffer->MSAAResolve();

    // 
    mScene->BindCamera(0, Shader::Vertex, 1); // Main camera
    mScene->Render(RendererFlags::OpacityPass, Shader::Vertex);

    // Done
    gOrderIndendentTransparency->End(rtTransparency, gOITSettings);
}

void RendererDeferred::CoverageBuffer() {

}

void RendererDeferred::Deferred() {

}

void RendererDeferred::SSAO() {
    // Opaque
    gSSAOPostProcess->Begin(rtGBuffer, gSSAOArgs);

    // TODO: Add separate RT for OIT
    // Transparent
    //gSSAOPostProcess->Begin(rtTransparency, gSSAOArgs);
}

void RendererDeferred::SSLR() {

}

void RendererDeferred::SSLF() {

}

void RendererDeferred::FSSSSS() {

}

void RendererDeferred::Combine() {
    ScopedRangeProfiler q("Combine Pass");

    shCombinationPass->Bind();

    BlendState::Push();

    s_states.blend.normal->Bind();
    rtCombinedGBuffer->Bind();
    rtCombinedGBuffer->Clear(s_clear.black_void2);

    // 
    BindOrtho();

    // Bind resources
    s_material.sampl.linear->Bind(Shader::Pixel, 0);

    rtGBuffer->Bind(1, Shader::Pixel, 0);
    rtGBuffer->Bind(3, Shader::Pixel, 1);
    gSSAOPostProcess->BindAO(Shader::Pixel, 2);

    // Draw call
    DXDraw(6, 0);

    // 
    BlendState::Pop();
    LunaEngine::PSDiscardSRV<3>();

    // Copy result
    gDirectX->gContext->CopyResource(rtGBuffer->GetBufferTexture<0>(), rtCombinedGBuffer->GetBufferTexture<0>());

}

void RendererDeferred::DOF() {

}

void RendererDeferred::HDR() {

}

void RendererDeferred::Final() {
    rtFinalPass->Bind();

    BindOrtho();

    shPostProcess->Bind();


    // HDR Post Processing; Eye Adaptation; Bloom; Depth of Field
    gHDRPostProcess->BindFinalPass(Shader::Pixel, 0);
    gHDRPostProcess->BindLuminance(Shader::Pixel, 4);
    gHDRPostProcess->BindBloom(Shader::Pixel, 5);
    gHDRPostProcess->BindBlur(Shader::Pixel, 6);

    gSSAOPostProcess->BindAO(Shader::Pixel, 8);

    rtGBuffer->Bind(0u, Shader::Pixel, 7, false);

    s_material.sampl.linear->Bind(Shader::Pixel, 5);

    // 
    BindOrtho();

    // Diffuse
    rtCombinedGBuffer->Bind(1u, Shader::Pixel, 0, false);
    s_material.sampl.point->Bind(Shader::Pixel, 0);

    // SSLR
    //gContext->PSSetShaderResources(1, 1, &_SSLRBf->pSRV);
    s_material.sampl.point->Bind(Shader::Pixel, 1);

    // Shadows
    //gContext->PSSetShaderResources(2, 1, &_Shadow->pSRV);
    //sPoint->Bind(Shader::Pixel, 2);

    // Deferred
    //gContext->PSSetShaderResources(3, 1, &_ColorD->pSRV);
    //sPoint->Bind(Shader::Pixel, 3);

    DXDraw(6, 0);

    LunaEngine::PSDiscardSRV<10>();
}

void RendererDeferred::BindOrtho() {
    // Bind matrices
    mScene->DefineCameraOrtho(2, .1f, 10.f, Width(), Height());
    mScene->BindCamera(2, Shader::Vertex, 1);

    IdentityTransf->Bind(cbTransform, Shader::Vertex, 0);
}
