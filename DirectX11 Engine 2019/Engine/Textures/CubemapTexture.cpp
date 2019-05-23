#include "CubemapTexture.h"

void CubemapTexture::CreateFromFiles(std::string folder, bool bDepth, DXGI_FORMAT format) {
    isLoaded   = true;
    isDepthMap = bDepth;

    // 
    UINT flags = D3D11_BIND_SHADER_RESOURCE;
    //if( isDepthMap ) {
    //    flags |= D3D11_BIND_DEPTH_STENCIL;
    //} else {
    //    flags |= D3D11_BIND_RENDER_TARGET;
    //}

    // Create texture description
    D3D11_TEXTURE2D_DESC desc;
    desc.Format = format;
    desc.ArraySize = 6;
    desc.MipLevels = 1;
    desc.BindFlags = flags;
    desc.Usage = D3D11_USAGE_IMMUTABLE; // GPU Read only
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    desc.CPUAccessFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    D3D11_SUBRESOURCE_DATA *pSubres = new D3D11_SUBRESOURCE_DATA[6];

    // Load subresource data
    for( int i = 0; i < 6; i++ ) {
        // Load file
        std::string fname = folder + sSideNames[i] + ".png";
        void* data = stbi_load(fname.c_str(), &Width, &Height, &channels, 0);

        // Set data
        pSubres[i].SysMemSlicePitch = 0;

        if( data ) {
            pSubres[i].pSysMem = data;
            pSubres[i].SysMemPitch = channels * Width;
        } else {
            pSubres[i].pSysMem = nullptr;
            pSubres[i].SysMemPitch = 0;
        }
    }

    // Set side size
    desc.Width = Width;
    desc.Height = Height;

    // Create texture
    auto res = gDirectX->gDevice->CreateTexture2D(&desc, pSubres, &pTexture);
    if( FAILED(res) ) {
        std::cout << "Failed to create texture!" << std::endl;
        return;
    }

    // Free memory
    for( int i = 0; i < 6; i++ ) {
        stbi_image_free((void*)pSubres[i].pSysMem);
    }

    // Freeeeeeeeeee moar mem
    delete[] pSubres;

    // Create SRV desc
    D3D11_SHADER_RESOURCE_VIEW_DESC pSRVDesc;
    ZeroMemory(&pSRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    pSRVDesc.Format = format;
    pSRVDesc.Texture2DArray.MipLevels       = desc.MipLevels;
    pSRVDesc.Texture2DArray.MostDetailedMip = 0;
    pSRVDesc.Texture2DArray.ArraySize       = 6;
    pSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    pSRVDesc.Texture2DArray.FirstArraySlice = 0;

    // Create SRV
    res = gDirectX->gDevice->CreateShaderResourceView(pTexture, &pSRVDesc, &pSRV);
    if( FAILED(res) ) {
        std::cout << "Failed to create shader resource view!" << std::endl;
        return;
    }
}

void CubemapTexture::Bind(Shader::ShaderType type, UINT slot) {
    if( !pSRV ) { return; }
    switch( type ) {
        case Shader::Vertex  : gDirectX->gContext->VSSetShaderResources(slot, 1, &pSRV); break;
        case Shader::Pixel   : gDirectX->gContext->PSSetShaderResources(slot, 1, &pSRV); break;
        case Shader::Geometry: gDirectX->gContext->GSSetShaderResources(slot, 1, &pSRV); break;
        case Shader::Hull    : gDirectX->gContext->HSSetShaderResources(slot, 1, &pSRV); break;
        case Shader::Domain  : gDirectX->gContext->DSSetShaderResources(slot, 1, &pSRV); break;
        case Shader::Compute : gDirectX->gContext->CSSetShaderResources(slot, 1, &pSRV); break;
    }
}

void CubemapTexture::Release() {
    if( pTexture ) pTexture->Release();
    if( pSRV ) pSRV->Release();

    if( isLoaded ) { return; }
    for( int i = 0; i < 6; i++ ) {
        if( isDepthMap ) {
            if( pDSVs[i] ) pDSVs[i]->Release();
        } else {
            if( pRTVs[i] ) pRTVs[i]->Release();
        }
    }
}